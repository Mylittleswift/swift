//===--- GlobalObjects.h - Statically-initialized objects -------*- C++ -*-===//
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
//
//  Objects that are allocated at global scope instead of on the heap,
//  and statically initialized to avoid synchronization costs, are
//  defined here.
//
//===----------------------------------------------------------------------===//
#ifndef SWIFT_STDLIB_SHIMS_GLOBALOBJECTS_H_
#define SWIFT_STDLIB_SHIMS_GLOBALOBJECTS_H_

#include "SwiftStdint.h"
#include "SwiftStdbool.h"
#include "HeapObject.h"
#include "Visibility.h"

#ifdef __cplusplus
namespace swift { extern "C" {
#endif

struct _SwiftArrayBodyStorage {
  __swift_intptr_t count;
  __swift_uintptr_t _capacityAndFlags;
};

struct _SwiftEmptyArrayStorage {
  struct HeapObject header;
  struct _SwiftArrayBodyStorage body;
};

SWIFT_RUNTIME_STDLIB_API
struct _SwiftEmptyArrayStorage _swiftEmptyArrayStorage;

struct _SwiftDictionaryBodyStorage {
  __swift_intptr_t count;
  __swift_intptr_t capacity;
  __swift_intptr_t scale;
  __swift_intptr_t seed;
  void *rawKeys;
  void *rawValues;
};

struct _SwiftSetBodyStorage {
  __swift_intptr_t count;
  __swift_intptr_t capacity;
  __swift_intptr_t scale;
  __swift_intptr_t seed;
  void *rawElements;
};

struct _SwiftEmptyDictionarySingleton {
  struct HeapObject header;
  struct _SwiftDictionaryBodyStorage body;
  __swift_uintptr_t metadata;
};

struct _SwiftEmptySetSingleton {
  struct HeapObject header;
  struct _SwiftSetBodyStorage body;
  __swift_uintptr_t metadata;
};

SWIFT_RUNTIME_STDLIB_API
struct _SwiftEmptyDictionarySingleton _swiftEmptyDictionarySingleton;

SWIFT_RUNTIME_STDLIB_API
struct _SwiftEmptySetSingleton _swiftEmptySetSingleton;

struct _SwiftHashingParameters {
  __swift_uint64_t seed0;
  __swift_uint64_t seed1;
  __swift_bool deterministic;
};
  
SWIFT_RUNTIME_STDLIB_API
struct _SwiftHashingParameters _swift_stdlib_Hashing_parameters;

#ifdef __cplusplus

static_assert(std::is_pod<_SwiftEmptyArrayStorage>::value,
              "empty array type should be POD");
static_assert(std::is_pod<_SwiftEmptyDictionarySingleton>::value,
              "empty dictionary type should be POD");
static_assert(std::is_pod<_SwiftEmptySetSingleton>::value,
              "empty set type should be POD");

}} // extern "C", namespace swift
#endif

#endif
