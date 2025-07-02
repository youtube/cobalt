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

#ifndef STARBOARD_COMMON_MUTEX_DEBUGGER_H_
#define STARBOARD_COMMON_MUTEX_DEBUGGER_H_

#include <pthread.h>

#include "starboard/configuration.h"

namespace starboard {

class MutexDebugger {
 public:
  MutexDebugger();
  ~MutexDebugger();

#ifdef _DEBUG
  void debugInit();
  void debugSetReleased() const;
  void debugPreAcquire() const;
  void debugSetAcquired() const;
  mutable pthread_t current_thread_acquired_;
#else
  void debugInit() {}
  void debugSetReleased() const {}
  void debugPreAcquire() const {}
  void debugSetAcquired() const {}
#endif
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_MUTEX_DEBUGGER_H_
