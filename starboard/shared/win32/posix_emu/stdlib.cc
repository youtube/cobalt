// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include <stdlib.h>

#include <errno.h>

// Undef alias to `free` and pull in the system-level header so we can use it
#undef free
#include <../ucrt/malloc.h>

// clang-format off
// The windows.h must be included before the synchapi.h
#include <windows.h>   // NOLINT
#include <synchapi.h>  // NOLINT
// clang-format on

#include <set>

static void* take_or_store_impl(bool take, void* p) {
  // Make sure the set is allocated and stays allocated for all global calls.
  static std::set<void*>* s_addr = new std::set<void*>();
  if (take) {
    bool found = (s_addr->find(p) != s_addr->end());

    // remove the address
    s_addr->erase(p);

    if (found) {
      return p;
    } else {
      return nullptr;
    }
  } else {
    // Store the address
    s_addr->insert(p);
    return p;
  }
}

struct CriticalSection {
  CriticalSection() { InitializeCriticalSection(&critical_section_); }

  CRITICAL_SECTION critical_section_;
};

static void* take_or_store(bool take, void* p) {
  static CriticalSection s_critical_section;

  EnterCriticalSection(&s_critical_section.critical_section_);
  void* res = take_or_store_impl(take, p);
  LeaveCriticalSection(&s_critical_section.critical_section_);
  return res;
}

static void store_aligned_pointer(void* p) {
  take_or_store(false /* take */, p);
}

static void* take_aligned_pointer(void* p) {
  return take_or_store(true /* take */, p);
}

///////////////////////////////////////////////////////////////////////////////
// Implementations below exposed externally in pure C for emulation.
///////////////////////////////////////////////////////////////////////////////

extern "C" {

int posix_memalign(void** res, size_t alignment, size_t size) {
  *res = _aligned_malloc(size, alignment);
  store_aligned_pointer(*res);
  if (*res != nullptr) {
    return 0;
  }
  return ENOMEM;
}

void sb_free(void* p) {
  if (!p) {
    return;
  }
  void* res = take_aligned_pointer(p);
  if (res == p) {
    _aligned_free(p);
  } else {
    free(p);  // This is using the system-level implementation.
  }
}

}  // extern "C"
