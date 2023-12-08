// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "SkMalloc.h"
#include "starboard/memory.h"
#include "starboard/system.h"

#define SK_DEBUGFAILF(fmt, ...) \
  SkASSERT((SkDebugf(fmt "\n", __VA_ARGS__), false))

static inline void sk_out_of_memory(size_t size) {
  SK_DEBUGFAILF("sk_out_of_memory (asked for %zu bytes)", size);
  SbSystemBreakIntoDebugger();
}

static inline void* throw_on_failure(size_t size, void* p) {
  if (size > 0 && p == NULL) {
    // If we've got a NULL here, the only reason we should have failed is
    // running out of RAM.
    sk_out_of_memory(size);
  }
  return p;
}

void sk_abort_no_print() { SbSystemBreakIntoDebugger(); }

void sk_out_of_memory(void) { SbSystemBreakIntoDebugger(); }

void* sk_realloc_throw(void* addr, size_t size) {
  return throw_on_failure(size, realloc(addr, size));
}

void sk_free(void* p) {
  if (p) {
    free(p);
  }
}

void* sk_malloc_flags(size_t size, unsigned flags) {
  void* p;
  if (flags & SK_MALLOC_ZERO_INITIALIZE) {
    p = calloc(size, 1);
  } else {
    p = malloc(size);
  }
  if (flags & SK_MALLOC_THROW) {
    return throw_on_failure(size, p);
  } else {
    return p;
  }
}
