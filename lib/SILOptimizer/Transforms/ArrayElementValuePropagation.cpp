//===--- ArrayElementValuePropagation.cpp - Propagate values of arrays ----===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
#define DEBUG_TYPE "array-element-propagation"

#include "swift/AST/NameLookup.h"
#include "swift/AST/ParameterList.h"
#include "swift/AST/GenericEnvironment.h"
#include "swift/SIL/SILBasicBlock.h"
#include "swift/SIL/SILInstruction.h"
#include "swift/SIL/DebugUtils.h"
#include "swift/SILOptimizer/Analysis/ArraySemantic.h"
#include "swift/SILOptimizer/PassManager/Passes.h"
#include "swift/SILOptimizer/PassManager/Transforms.h"
#include "llvm/ADT/SmallVector.h"

using namespace swift;

/// Propagate the elements of array values to calls of the array's get_element
/// method, and replace calls of append(contentsOf:) with append(element:).
///
/// Array literal construction and array initialization of array values
/// associates element values with the array value. These values can be
/// propagated to the get_element method if we can prove that the array value
/// has not changed until reading the array value's element. These values can
/// also be used to replace append(contentsOf:) with multiple append(element:)
/// calls.
///
/// Propagation of the elements of one array allocation.
///
/// We propagate the elements associated with calls of
///
/// * Array.init(count:repeatedValue:)
///   The 'repeatedValue'.
///   TODO: this is not yet implemented.
///
/// * Array._adoptStorage(storage:count:)
///   The stores on the returned array element buffer pointer.
///
namespace {

/// Utility class for analysis array literal initializations.
///
/// Array literals are initialized by allocating an array buffer, and storing
/// the elements into it.
/// This class analysis all the code which does the array literal
/// initialization. It also collects uses of the array, like getElement calls
/// and append(contentsOf) calls.
class ArrayAllocation {
  /// The array value returned by the allocation call.
  SILValue ArrayValue;

  /// The calls to Array get_element that use this array allocation.
  llvm::SmallSetVector<ApplyInst *, 16> GetElementCalls;

  /// The calls to Array append_contentsOf that use this array allocation.
  llvm::SmallVector<ApplyInst *, 4> AppendContentsOfCalls;

  /// A map of Array indices to element values
  llvm::DenseMap<uint64_t, SILValue> ElementValueMap;

  bool mapInitializationStores(SILValue ElementBuffer);
  bool recursivelyCollectUses(ValueBase *Def);
  bool replacementsAreValid();

  // After approx. this many elements, it's faster to use append(contentsOf:)
  static constexpr unsigned APPEND_CONTENTSOF_REPLACEMENT_VALUES_MAX = 6;

public:

  ArrayAllocation() {}

  /// Analyzes an array allocation call.
  ///
  /// Returns true if \p Alloc is the allocation of an array literal (or a
  /// similar pattern) and the array values can be used to replace get_element
  /// or append(contentof) calls.
  bool analyze(ApplyInst *Alloc);

  /// Replace getElement calls with the actual values.
  bool replaceGetElements();

  /// Replace append(contentsOf:) with multiple append(element:)
  bool replaceAppendContentOf();
};

/// Map the indices of array element initialization stores to their values.
bool ArrayAllocation::mapInitializationStores(SILValue ElementBuffer) {
  assert(ElementBuffer &&
         "Must have identified an array element storage pointer");

  // Match initialization stores.
  // %83 = struct_extract %element_buffer : $UnsafeMutablePointer<Int>
  // %84 = pointer_to_address %83 : $Builtin.RawPointer to strict $*Int
  // store %85 to %84 : $*Int
  // %87 = integer_literal $Builtin.Word, 1
  // %88 = index_addr %84 : $*Int, %87 : $Builtin.Word
  // store %some_value to %88 : $*Int

  auto *UnsafeMutablePointerExtract =
      dyn_cast_or_null<StructExtractInst>(getSingleNonDebugUser(ElementBuffer));
  if (!UnsafeMutablePointerExtract)
    return false;
  auto *PointerToAddress = dyn_cast_or_null<PointerToAddressInst>(
      getSingleNonDebugUser(UnsafeMutablePointerExtract));
  if (!PointerToAddress)
    return false;

  // Match the stores. We can have either a store directly to the address or
  // to an index_addr projection.
  for (auto *Op : PointerToAddress->getUses()) {
    auto *Inst = Op->getUser();

    // Store to the base.
    auto *SI = dyn_cast<StoreInst>(Inst);
    if (SI && SI->getDest() == PointerToAddress) {
      // We have already seen an entry for this index bail.
      if (ElementValueMap.count(0))
        return false;
      ElementValueMap[0] = SI->getSrc();
      continue;
    } else if (SI)
      return false;

    // Store an index_addr projection.
    auto *IndexAddr = dyn_cast<IndexAddrInst>(Inst);
    if (!IndexAddr)
      return false;
    SI = dyn_cast_or_null<StoreInst>(getSingleNonDebugUser(IndexAddr));
    if (!SI || SI->getDest() != IndexAddr)
      return false;
    auto *Index = dyn_cast<IntegerLiteralInst>(IndexAddr->getIndex());
    if (!Index)
      return false;
    auto IndexVal = Index->getValue();
    // Let's not blow up our map.
    if (IndexVal.getActiveBits() > 16)
      return false;
    // Already saw an entry.
    if (ElementValueMap.count(IndexVal.getZExtValue()))
      return false;

    ElementValueMap[IndexVal.getZExtValue()] = SI->getSrc();
  }
  return !ElementValueMap.empty();
}

bool ArrayAllocation::replacementsAreValid() {
  unsigned ElementCount = ElementValueMap.size();

  if (ElementCount > APPEND_CONTENTSOF_REPLACEMENT_VALUES_MAX)
    return false;

  // Bail if elements aren't contiguous
  for (unsigned i = 0; i < ElementCount; ++i)
    if (!ElementValueMap.count(i))
      return false;

  return true;
}

/// Recursively look at all uses of this definition. Abort if the array value
/// could escape or be changed. Collect all uses that are calls to array.count.
bool ArrayAllocation::recursivelyCollectUses(ValueBase *Def) {
  for (auto *Opd : Def->getUses()) {
    auto *User = Opd->getUser();
    // Ignore reference counting and debug instructions.
    if (isa<RefCountingInst>(User) ||
        isa<DebugValueInst>(User))
      continue;

    // Array value projection.
    if (auto *SEI = dyn_cast<StructExtractInst>(User)) {
      if (!recursivelyCollectUses(SEI))
        return false;
      continue;
    }

    // Check array semantic calls.
    ArraySemanticsCall ArrayOp(User);
    if (ArrayOp) {
      if (ArrayOp.getKind() == ArrayCallKind::kAppendContentsOf) {
        AppendContentsOfCalls.push_back(ArrayOp);
        continue;
      } else if (ArrayOp.getKind() == ArrayCallKind::kGetElement) {
        GetElementCalls.insert(ArrayOp);
        continue;
      } else if (ArrayOp.doesNotChangeArray()) {
        continue;
      }
    }

    // An operation that escapes or modifies the array value.
    return false;
  }
  return true;
}

bool ArrayAllocation::analyze(ApplyInst *Alloc) {
  GetElementCalls.clear();
  AppendContentsOfCalls.clear();
  ElementValueMap.clear();

  ArraySemanticsCall Uninitialized(Alloc, "array.uninitialized");
  if (!Uninitialized)
    return false;

  ArrayValue = Uninitialized.getArrayValue();
  if (!ArrayValue)
    return false;

  SILValue ElementBuffer = Uninitialized.getArrayElementStoragePointer();
  if (!ElementBuffer)
    return false;

  // Figure out all stores to the array.
  if (!mapInitializationStores(ElementBuffer))
    return false;

  // Check if the array value was stored or has escaped.
  if (!recursivelyCollectUses(ArrayValue))
    return false;

  return true;
}

/// Replace getElement calls with the actual values.
///
/// \code
///    store %x to %element_address
///    ...
///    %e = apply %getElement(%array, %constant_index)
/// \endcode
/// The value %e is replaced with %x.
bool ArrayAllocation::replaceGetElements() {
  bool Changed = false;
  for (auto *GetElementCall : GetElementCalls) {
    ArraySemanticsCall GetElement(GetElementCall);
    assert(GetElement.getKind() == ArrayCallKind::kGetElement);

    auto ConstantIndex = GetElement.getConstantIndex();
    if (ConstantIndex == None)
      continue;

    assert(*ConstantIndex >= 0 && "Must have a positive index");

    auto EltValueIt = ElementValueMap.find(*ConstantIndex);
    if (EltValueIt == ElementValueMap.end())
      continue;

    Changed |= GetElement.replaceByValue(EltValueIt->second);
  }
  return Changed;
}

/// Replace append(contentsOf:) with multiple append(element:)
///
/// \code
///    store %x to %source_array_element_address_0
///    store %y to %source_array_element_address_1
///    ...
///    apply %append_contentsOf(%dest_array, %source_array)
/// \endcode
/// is replaced by
/// \code
///    store %x to %source_array_element_address_0
///    store %y to %source_array_element_address_1
///    ...
///    apply %reserveCapacityForAppend(%dest_array, %number_of_values)
///    apply %append_element(%dest_array, %x)
///    apply %append_element(%dest_array, %y)
///    ...
/// \endcode
/// The source_array and its initialization code can then be deleted (if not
/// used otherwise).
bool ArrayAllocation::replaceAppendContentOf() {
  if (AppendContentsOfCalls.empty())
    return false;
  if (ElementValueMap.empty())
    return false;

  // Check if there is a store to each element.
  if (!replacementsAreValid())
    return false;

  llvm::SmallVector<SILValue, 4> ElementValueVector;
  for (unsigned i = 0; i < ElementValueMap.size(); ++i) {
    SILValue V = ElementValueMap[i];
    ElementValueVector.push_back(V);
  }

  SILFunction *Fn = AppendContentsOfCalls[0]->getFunction();
  SILModule &M = Fn->getModule();
  ASTContext &Ctx = M.getASTContext();

  LLVM_DEBUG(llvm::dbgs() << "Array append contentsOf calls replaced in "
             << Fn->getName() << "\n");

  // Get the needed Array helper functions.
  FuncDecl *AppendFnDecl = Ctx.getArrayAppendElementDecl();
  if (!AppendFnDecl)
    return false;

  FuncDecl *ReserveFnDecl = Ctx.getArrayReserveCapacityDecl();
  if (!ReserveFnDecl)
    return false;

  auto Mangled = SILDeclRef(AppendFnDecl, SILDeclRef::Kind::Func).mangle();
  SILFunction *AppendFn = M.findFunction(Mangled, SILLinkage::PublicExternal);
  if (!AppendFn)
    return false;

  Mangled = SILDeclRef(ReserveFnDecl, SILDeclRef::Kind::Func).mangle();
  SILFunction *ReserveFn = M.findFunction(Mangled, SILLinkage::PublicExternal);
  if (!ReserveFn)
    return false;

  bool Changed = false;
  // Usually there is only a single append(contentsOf:) call. But there might
  // be multiple - with the same source array to append.
  for (ApplyInst *AppendContentOfCall : AppendContentsOfCalls) {
    ArraySemanticsCall AppendContentsOf(AppendContentOfCall);
    assert(AppendContentsOf && "Must be AppendContentsOf call");

    NominalTypeDecl *AppendSelfArray = AppendContentsOf.getSelf()->getType().
    getASTType()->getAnyNominal();

    // In case if it's not an Array, but e.g. an ContiguousArray
    if (AppendSelfArray != Ctx.getArrayDecl())
      continue;

    SILType ArrayType = ArrayValue->getType();
    auto *NTD = ArrayType.getASTType()->getAnyNominal();
    SubstitutionMap ArraySubMap = ArrayType.getASTType()
    ->getContextSubstitutionMap(M.getSwiftModule(), NTD);

    AppendContentsOf.replaceByAppendingValues(M, AppendFn, ReserveFn,
                                              ElementValueVector,
                                              ArraySubMap);
    Changed = true;
  }
  return Changed;
}

// =============================================================================
//                                 Driver
// =============================================================================

class ArrayElementPropagation : public SILFunctionTransform {
public:
  ArrayElementPropagation() {}

  void run() override {
    auto &Fn = *getFunction();

    // FIXME: Update for ownership.
    if (Fn.hasOwnership())
      return;

    bool Changed = false;

    for (auto &BB :Fn) {
      for (auto &Inst : BB) {
        if (auto *Apply = dyn_cast<ApplyInst>(&Inst)) {
          ArrayAllocation ALit;
          if (!ALit.analyze(Apply))
            continue;

          // First optimization: replace getElemente calls.
          if (ALit.replaceGetElements()) {
            Changed = true;
            // Re-do the analysis if the SIL changed.
            if (!ALit.analyze(Apply))
              continue;
          }
          // Second optimization: replace append(contentsOf:) calls.
          Changed |= ALit.replaceAppendContentOf();
        }
      }
    }

    if (Changed) {
      PM->invalidateAnalysis(
          &Fn, SILAnalysis::InvalidationKind::CallsAndInstructions);
    }
  }
};
} // end anonymous namespace

SILTransform *swift::createArrayElementPropagation() {
  return new ArrayElementPropagation();
}
