// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <process.h>
#include <memory>

#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/shared/win32/thread_private.h"

using starboard::shared::win32::GetThreadSubsystemSingleton;
using starboard::shared::win32::SbThreadPrivate;
using starboard::shared::win32::ThreadSubsystemSingleton;

namespace starboard {
namespace shared {
namespace win32 {

SB_ONCE_INITIALIZE_FUNCTION(ThreadSubsystemSingleton,
                            GetThreadSubsystemSingleton);

void RegisterMainThread() {
  std::unique_ptr<SbThreadPrivate> thread_private(new SbThreadPrivate());

  // GetCurrentThread() returns a pseudo-handle that must be duplicated
  // to be used in general cases.
  HANDLE pseudo_handle = GetCurrentThread();
  HANDLE handle;
  BOOL success =
      DuplicateHandle(GetCurrentProcess(), pseudo_handle, GetCurrentProcess(),
                      &handle, 0, false, DUPLICATE_SAME_ACCESS);
  SB_DCHECK(success) << "DuplicateHandle failed";

  thread_private->handle_ = handle;
  thread_private->wait_for_join_ = false;

  SbThreadSetLocalValue(GetThreadSubsystemSingleton()->thread_private_key_,
                        thread_private.release());
}

SbThreadPrivate* GetCurrentSbThreadPrivate() {
  SbThreadPrivate* sb_thread_private =
      static_cast<SbThreadPrivate*>(SbThreadGetLocalValue(
          GetThreadSubsystemSingleton()->thread_private_key_));
  if (sb_thread_private == nullptr) {
    // We are likely on a thread we did not create, so TLS needs to be setup.
    RegisterMainThread();
    sb_thread_private = static_cast<SbThreadPrivate*>(SbThreadGetLocalValue(
        GetThreadSubsystemSingleton()->thread_private_key_));
    // TODO: Clean up TLS storage for threads we do not create.
  }
  return sb_thread_private;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
