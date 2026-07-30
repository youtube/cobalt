// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "init_musl.h"
#include "libc.h"
#include <sys/auxv.h>

// Initialize musl automatically before program execution begins.
// We use a constructor attribute with priority 101 (the highest user priority,
// as 0-100 are reserved for the compiler implementation) to guarantee this runs
// before any C++ static initializers or other application code.
// This is critical because it generates the master TLS stack canary. If it
// runs too late, thread-local storage pointers can become corrupted and
// functions protected by stack canaries will crash.
__attribute__((constructor(101))) void init_musl() {

  // Set __hwcap bitmask by getauxval.
  __hwcap = getauxval(AT_HWCAP);

  // Init the __stack_chk_guard.
  init_stack_guard();
}
