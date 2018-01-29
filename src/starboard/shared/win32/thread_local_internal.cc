// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/win32/thread_local_internal.h"

#include <windows.h>

namespace starboard {
namespace shared {
namespace win32 {

// FlsAlloc() is like TlsAlloc(), but allows destructors.
// The downside is that FlsAlloc() has a small upper limit of
// about ~100 available keys. To provide extra keys, TlsAlloc()
// is used whenever a destructor is not necessary.
DWORD TlsInternalAlloc(SbThreadLocalDestructor destructor_fn) {
  DWORD output = 0;
  if (destructor_fn) {
    output = FlsAlloc(destructor_fn);
  } else {
    output = TlsAlloc();
  }
  if (output == TLS_OUT_OF_INDEXES) {
    return TLS_OUT_OF_INDEXES;
  }
  return (output << 1) | (destructor_fn ? 0x1 : 0x0);
}

void TlsInternalFree(DWORD key) {
  bool has_destructor = key & 0x1;
  DWORD index = key >> 1;
  if (has_destructor) {
    FlsFree(index);
  } else {
    TlsFree(index);
  }
}

void* TlsInternalGetValue(DWORD key) {
  bool has_destructor = key & 0x1;
  DWORD index = key >> 1;
  if (has_destructor) {
    return FlsGetValue(index);
  } else {
    return TlsGetValue(index);
  }
}

bool TlsInternalSetValue(DWORD key, void* value) {
  bool has_destructor = key & 0x1;
  DWORD index = key >> 1;
  if (has_destructor) {
    return FlsSetValue(index, value);
  } else {
    return TlsSetValue(index, value);
  }
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
