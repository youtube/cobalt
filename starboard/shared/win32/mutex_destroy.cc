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

#include "starboard/mutex.h"

#include <windows.h>

#include "starboard/configuration.h"

bool SbMutexDestroy(SbMutex* mutex) {
  if (!mutex) {
    return false;
  }
  // On Windows a SRWLOCK is used in place of the heavier mutex. These locks
  // cannot be acquired recursively, and the behavior when this is attempted is
  // not clear in the documentation. A Microsoft DevBlog seems to suggest this
  // is undefined behavior:
  //
  //   It’s a programming error. It is your responsibility as a programmer not
  //   to call Acquire­SRW­Lock­Shared or Acquire­SRW­Lock­Exclusive from a
  //   thread that has already acquired the lock. Failing to comply with this
  //   rule will result in undefined behavior.
  //
  // https://devblogs.microsoft.com/oldnewthing/20160819-00/?p=94125
  return true;
}
