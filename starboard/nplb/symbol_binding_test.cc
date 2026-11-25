// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <setjmp.h>
#include <string.h>

#include <cstddef>
#include <cstdint>
#include <new>

#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

// The start and the end of this library's image, defined by the linker.
// Unlike `__ehdr_start`, `_end` is exported, and the loader has its own.
extern char __ehdr_start;
extern char _end __attribute__((visibility("hidden")));

namespace nplb {
namespace {

bool IsInThisLibrary(const void* address) {
  const auto value = reinterpret_cast<uintptr_t>(address);
  return value >= reinterpret_cast<uintptr_t>(&__ehdr_start) &&
         value < reinterpret_cast<uintptr_t>(&_end);
}

template <typename Function>
const void* AddressOf(Function* function) {
  return reinterpret_cast<const void*>(function);
}

TEST(SymbolBindingTest, TellsOtherLibrariesApart) {
  // Defined by the Starboard library.
  EXPECT_FALSE(IsInThisLibrary(AddressOf(&SbSystemGetStack)));
  EXPECT_TRUE(IsInThisLibrary(AddressOf(&IsInThisLibrary)));
}

// The library defines these, and so do the platform's libraries. Its own
// code must use its own definitions, whatever else the process has loaded.
TEST(SymbolBindingTest, UsesItsOwnDefinitions) {
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
  GTEST_SKIP() << "The sanitizers' runtimes interpose these functions.";
#endif

  const struct {
    const char* name;
    const void* address;
  } kFunctions[] = {
      {"operator new", AddressOf<void*(size_t)>(&::operator new)},
      {"operator delete", AddressOf<void(void*)>(&::operator delete)},
      {"memcpy", AddressOf(&memcpy)},
      {"memset", AddressOf(&memset)},
      {"setjmp", AddressOf(&setjmp)},
      {"longjmp", AddressOf(&longjmp)},
  };

  for (const auto& function : kFunctions) {
    EXPECT_TRUE(IsInThisLibrary(function.address)) << function.name;
  }
}

}  // namespace
}  // namespace nplb
