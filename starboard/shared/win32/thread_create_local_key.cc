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

#include "starboard/thread.h"

#include <windows.h>

#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/shared/win32/thread_private.h"

using starboard::shared::win32::GetThreadSubsystemSingleton;
using starboard::shared::win32::ThreadSubsystemSingleton;

namespace starboard {
namespace shared {
namespace win32 {

SbThreadLocalKey SbThreadCreateLocalKeyInternal(
    SbThreadLocalDestructor destructor,
    ThreadSubsystemSingleton* singleton) {
  // Note that destructor used here to allow destruction of objects created by
  // TLS access by non-starboard threads.
  // Note that FlsAlloc's destructor mechanism is insufficient to pass
  // base_unittests, which expects that multiple destructor passes for
  // objects which insert destructable TLS pointers as side effects. For
  // starboard threads, the TLS destructors will run first and set the
  // TLS pointers to null, then the destructors will run a second time
  // but this is okay since the pointers will now be nullptrs, which is
  // a no-op. For non starboard threads, only the secondary destructor
  // will run.
  DWORD index = FlsAlloc(destructor);

  if (index == TLS_OUT_OF_INDEXES) {
    return kSbThreadLocalKeyInvalid;
  }

  SbThreadLocalKeyPrivate* result = static_cast<SbThreadLocalKeyPrivate*>(
      SbMemoryAllocateNoReport(sizeof(SbThreadLocalKeyPrivate)));

  if (result == nullptr) {
    return kSbThreadLocalKeyInvalid;
  }

  result->tls_index = index;
  result->destructor = destructor;

  SbMutexAcquire(&singleton->mutex_);
  singleton->thread_local_keys_.insert(std::make_pair(index, result));
  SbMutexRelease(&singleton->mutex_);
  return result;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

SbThreadLocalKey SbThreadCreateLocalKey(SbThreadLocalDestructor destructor) {
  ThreadSubsystemSingleton* singleton = GetThreadSubsystemSingleton();
  return SbThreadCreateLocalKeyInternal(destructor, singleton);
}
