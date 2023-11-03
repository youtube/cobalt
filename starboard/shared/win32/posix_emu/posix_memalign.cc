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

#undef free

#include <errno.h>
#include <malloc.h>

// clang-format off
// The windows.h must be included before the synchapi.h
#include <windows.h>   // NOLINT
#include <synchapi.h>  // NOLINT
// clang-format on

#include <map>

static void* take_or_store_impl(bool take, void* p) {
  // Make sure the map is allocated and stays allocated for all global calls.
  static std::map<void*, int>* s_map = new std::map<void*, int>();
  if (take) {
    int flag = (*s_map)[p];

    // clear the entry
    (*s_map)[p] = 0;

    if (flag) {
      // return the address if found
      return p;
    } else {
      return nullptr;
    }
  } else {
    // Store the address
    (*s_map)[p] = 1;
    return p;
  }
}

static void* take_or_store(bool take, void* p) {
  static SRWLOCK s_lock = SRWLOCK_INIT;
  AcquireSRWLockExclusive(&s_lock);
  void* res = take_or_store_impl(take, p);
  ReleaseSRWLockExclusive(&s_lock);
  return res;
}

static void store_aligned_pointer(void* p) {
  take_or_store(false /* take */, p);
}

static void* take_aligned_pointer(void* p) {
  return take_or_store(true /* take */, p);
}

extern "C" void free(void* p);
extern "C" void __imp_free(void* p);
extern "C" void* _aligned_malloc(size_t size, size_t alignment);

extern "C" int posix_memalign(void** res, size_t alignment, size_t size) {
  *res = _aligned_malloc(size, alignment);
  store_aligned_pointer(*res);
  if (*res != nullptr) {
    return 0;
  }
  return ENOMEM;
}

extern "C" void sb_free(void* p) {
  if (!p) {
    return;
  }
  void* res = take_aligned_pointer(p);
  if (res == p) {
    _aligned_free(p);
  } else {
    free(p);
  }
}
