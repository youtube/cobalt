// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/mutex_debugger.h"

#include "starboard/common/log.h"

namespace starboard {

MutexDebugger::MutexDebugger() {
  debugInit();
}

MutexDebugger::~MutexDebugger() {}

#ifdef _DEBUG
void MutexDebugger::debugInit() {
  current_thread_acquired_ = 0;
}
void MutexDebugger::debugSetReleased() const {
  pthread_t current_thread = pthread_self();
  SB_DCHECK(pthread_equal(current_thread_acquired_, current_thread));
  current_thread_acquired_ = 0;
}
void MutexDebugger::debugPreAcquire() const {
  // Check that the mutex is not held by the current thread.
  pthread_t current_thread = pthread_self();
  SB_DCHECK(!pthread_equal(current_thread_acquired_, current_thread));
}
void MutexDebugger::debugSetAcquired() const {
  // Check that the thread has already not been held.
  SB_DCHECK(current_thread_acquired_ == 0);
  current_thread_acquired_ = pthread_self();
}
#endif

}  // namespace starboard
