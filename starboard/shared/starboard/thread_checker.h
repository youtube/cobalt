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

#ifndef STARBOARD_SHARED_STARBOARD_THREAD_CHECKER_H_
#define STARBOARD_SHARED_STARBOARD_THREAD_CHECKER_H_

#include <atomic>

#include "build/build_config.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard {

class ThreadChecker {
 public:
  ThreadChecker() : thread_id_(SbThreadGetId()) {
    SB_CHECK(SbThreadIsValidId(thread_id_));
  }

  bool CalledOnValidThread() const { return thread_id_ == SbThreadGetId(); }

 private:
  const SbThreadId thread_id_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_THREAD_CHECKER_H_
