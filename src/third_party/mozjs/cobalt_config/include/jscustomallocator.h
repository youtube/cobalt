// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "jstypes.h"
#include "starboard/memory.h"
#include "starboard/string.h"

#define JS_OOM_POSSIBLY_FAIL() do {} while(0)
#define JS_OOM_POSSIBLY_FAIL_REPORT(cx) do {} while(0)

static JS_INLINE void* js_malloc(size_t bytes) {
  return SbMemoryAllocate(bytes);
}

static JS_INLINE void* js_calloc(size_t bytes) {
  return SbMemoryCalloc(bytes, 1);
}

static JS_INLINE void* js_calloc(size_t nmemb, size_t size) {
  return SbMemoryCalloc(nmemb, size);
}

static JS_INLINE void* js_realloc(void* p, size_t bytes) {
  return SbMemoryReallocate(p, bytes);
}

static JS_INLINE void js_free(void* p) {
  SbMemoryFree(p);
}

static JS_INLINE char* js_strdup(char* s) {
  return SbStringDuplicate(s);
}
