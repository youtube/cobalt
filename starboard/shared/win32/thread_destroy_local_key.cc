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
#include "starboard/shared/win32/thread_private.h"

using starboard::shared::win32::GetThreadSubsystemSingleton;
using starboard::shared::win32::ThreadSubsystemSingleton;

void SbThreadDestroyLocalKey(SbThreadLocalKey key) {
  if (!SbThreadIsValidLocalKey(key)) {
    return;
  }
  DWORD tls_index = static_cast<SbThreadLocalKeyPrivate*>(key)->tls_index;
  ThreadSubsystemSingleton* singleton = GetThreadSubsystemSingleton();

  SbMutexAcquire(&singleton->mutex_);
  singleton->thread_local_keys_.erase(tls_index);
  SbMutexRelease(&singleton->mutex_);

  FlsFree(tls_index);
  SbMemoryDeallocateNoReport(key);
}
