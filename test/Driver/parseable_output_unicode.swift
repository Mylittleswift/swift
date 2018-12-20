// RUN: %swiftc_driver_plain -emit-executable "%S/Inputs/你好.swift" -o %t.out -emit-module -emit-module-path %t.swiftmodule -emit-objc-header-path %t.h -serialize-diagnostics -emit-dependencies -parseable-output -driver-skip-execution 2>&1 | %FileCheck %s

// XFAIL: freebsd, linux

// CHECK: {{[1-9][0-9]*}}
// CHECK-NEXT: {
// CHECK-NEXT:   "kind": "began",
// CHECK-NEXT:   "name": "compile",
// CHECK-NEXT:   "command": "{{.*[\\/]}}swift{{c?(\.EXE)?(\\")?}} -frontend -c -primary-file {{.*[\\/]}}你好.swift{{(\\")? .*}} -o {{.*[\\/]}}你好-[[OUTPUT:.*]].o{{(\\")?}}",
// CHECK-NEXT:   "command_executable": "{{.*[\\/]}}swift{{c?(\.EXE)?(\\")?}}",
// CHECK-NEXT:   "command_arguments": [
// CHECK-NEXT:     "-frontend",
// CHECK-NEXT:     "-c",
// CHECK-NEXT:     "-primary-file",
// CHECK-NEXT:     "{{.*[\\/]}}你好.swift",
// CHECK:          "-o",
// CHECK-NEXT:     "{{.*[\\/]}}你好-[[OUTPUT:.*]].o"
// CHECK-NEXT:   ],
// CHECK-NEXT:   "inputs": [
// CHECK-NEXT:     "{{.*[\\/]}}你好.swift"
// CHECK-NEXT:   ],
// CHECK-NEXT:   "outputs": [
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "object",
// CHECK-NEXT:       "path": "{{.*[\\/]}}你好-[[OUTPUT]].o"
// CHECK-NEXT:     },
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "dependencies",
// CHECK-NEXT:       "path": "{{.*[\\/]}}你好-[[OUTPUT]].d"
// CHECK-NEXT:     },
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "swiftmodule",
// CHECK-NEXT:       "path": "{{.*[\\/]}}你好-[[OUTPUT]].swiftmodule"
// CHECK-NEXT:     },
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "swiftdoc",
// CHECK-NEXT:       "path": "{{.*[\\/]}}你好-[[OUTPUT]].swiftdoc"
// CHECK-NEXT:     },
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "diagnostics",
// CHECK-NEXT:       "path": "{{.*[\\/]}}你好-[[OUTPUT]].dia"
// CHECK-NEXT:     }
// CHECK-NEXT:   ],
// CHECK-NEXT:   "pid": 1,
// CHECK-NEXT:   "process": {
// CHECK-NEXT:   	"real_pid": 1
// CHECK-NEXT:   }
// CHECK-NEXT: }

// CHECK-NEXT: {{[1-9][0-9]*}}
// CHECK-NEXT: {
// CHECK-NEXT:   "kind": "finished",
// CHECK-NEXT:   "name": "compile",
// CHECK-NEXT:   "pid": 1,
// CHECK-NEXT:   "output": "Output placeholder\n",
// CHECK-NEXT:   "process": {
// CHECK-NEXT:   	"real_pid": 1
// CHECK-NEXT:   },
// CHECK-NEXT:   "exit-status": 0
// CHECK-NEXT: }

// CHECK-NEXT: {{[1-9][0-9]*}}
// CHECK-NEXT: {
// CHECK-NEXT:   "kind": "began",
// CHECK-NEXT:   "name": "merge-module",
// CHECK-NEXT:   "command": "{{.*[\\/]}}swift{{c?}} -frontend -merge-modules -emit-module {{.*[\\/]}}你好-[[OUTPUT]].swiftmodule {{.*}} -o {{.*[\\/]}}parseable_output_unicode.swift.tmp.swiftmodule",
// CHECK-NEXT:   "command_executable": "{{.*[\\/]}}swift{{c?}}",
// CHECK-NEXT:   "command_arguments": [
// CHECK-NEXT:     "-frontend",
// CHECK-NEXT:     "-merge-modules",
// CHECK-NEXT:     "-emit-module",
// CHECK-NEXT:     "{{.*[\\/]}}你好-[[OUTPUT]].swiftmodule",
// CHECK:          "-o",
// CHECK-NEXT:     "{{.*[\\/]}}parseable_output_unicode.swift.tmp.swiftmodule"
// CHECK-NEXT:   ],
// CHECK-NEXT:   "inputs": [
// CHECK-NEXT:     "{{.*[\\/]}}你好-[[OUTPUT]].o"
// CHECK-NEXT:   ],
// CHECK-NEXT:   "outputs": [
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "swiftmodule",
// CHECK-NEXT:       "path": "{{.*[\\/]}}parseable_output_unicode.swift.tmp.swiftmodule"
// CHECK-NEXT:     },
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "swiftdoc",
// CHECK-NEXT:       "path": "{{.*[\\/]}}parseable_output_unicode.swift.tmp.swiftdoc"
// CHECK-NEXT:     },
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "objc-header",
// CHECK-NEXT:       "path": "{{.*[\\/]}}parseable_output_unicode.swift.tmp.h"
// CHECK-NEXT:     }
// CHECK-NEXT:   ],
// CHECK-NEXT:   "pid": 2,
// CHECK-NEXT:   "process": {
// CHECK-NEXT:   	"real_pid": 2
// CHECK-NEXT:   }
// CHECK-NEXT: }

// CHECK-NEXT: {{[1-9][0-9]*}}
// CHECK-NEXT: {
// CHECK-NEXT:   "kind": "finished",
// CHECK-NEXT:   "name": "merge-module",
// CHECK-NEXT:   "pid": 2,
// CHECK-NEXT:   "output": "Output placeholder\n",
// CHECK-NEXT:   "process": {
// CHECK-NEXT:   	"real_pid": 2
// CHECK-NEXT:   },
// CHECK-NEXT:   "exit-status": 0
// CHECK-NEXT: }

// CHECK-NEXT: {{[1-9][0-9]*}}
// CHECK-NEXT: {
// CHECK-NEXT:   "kind": "began",
// CHECK-NEXT:   "name": "link",
// CHECK-NEXT:   "command": "{{.*[\\/]}}ld{{(\\")?}} {{.*[\\/]}}你好-[[OUTPUT]].o {{.*}} -o {{.*[\\/]}}parseable_output_unicode.swift.tmp.out",
// CHECK-NEXT:   "command_executable": "{{.*[\\/]}}ld{{(\\")?}}",
// CHECK-NEXT:   "command_arguments": [
// CHECK-NEXT:     "{{.*[\\/]}}你好-[[OUTPUT]].o",
// CHECK:          "-o",
// CHECK-NEXT:     "{{.*[\\/]}}parseable_output_unicode.swift.tmp.out"
// CHECK-NEXT:   ],
// CHECK-NEXT:   "inputs": [
// CHECK-NEXT:     "{{.*[\\/]}}你好-[[OUTPUT]].o"
// CHECK-NEXT:   ],
// CHECK-NEXT:   "outputs": [
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "image",
// CHECK-NEXT:       "path": "{{.*[\\/]}}parseable_output_unicode.swift.tmp.out"
// CHECK-NEXT:     }
// CHECK-NEXT:   ],
// CHECK-NEXT:   "pid": 3,
// CHECK-NEXT:   "process": {
// CHECK-NEXT:   	"real_pid": 3
// CHECK-NEXT:   }
// CHECK-NEXT: }

// CHECK-NEXT: {{[1-9][0-9]*}}
// CHECK-NEXT: {
// CHECK-NEXT:   "kind": "finished",
// CHECK-NEXT:   "name": "link",
// CHECK-NEXT:   "pid": 3,
// CHECK-NEXT:   "output": "Output placeholder\n",
// CHECK-NEXT:   "process": {
// CHECK-NEXT:   	"real_pid": 3
// CHECK-NEXT:   },
// CHECK-NEXT:   "exit-status": 0
// CHECK-NEXT: }
