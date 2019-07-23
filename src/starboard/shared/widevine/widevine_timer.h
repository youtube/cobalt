// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIDEVINE_WIDEVINE_TIMER_H_
#define STARBOARD_SHARED_WIDEVINE_WIDEVINE_TIMER_H_

#include <map>

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/thread.h"
#include "third_party/ce_cdm/cdm/include/cdm.h"

namespace starboard {
namespace shared {
namespace widevine {

// Manages the scheduled callbacks of Widevine.  All its public functions can
// be called from any threads.
class WidevineTimer : public ::widevine::Cdm::ITimer {
 public:
  ~WidevineTimer() override;

  // Call |client->onTimerExpired(context)| after |delay_in_milliseconds|.
  void setTimeout(int64_t delay_in_milliseconds,
                  IClient* client,
                  void* context) override;

  // Cancel all timers associated with |client|.  No timer should be called on
  // the specific client after this function returns.
  void cancel(IClient* client) override;

 private:
  typedef starboard::player::JobQueue JobQueue;

  static void* ThreadFunc(void* param);
  void RunLoop(ConditionVariable* condition_variable);
  void CancelAllJobsOnClient(IClient* client,
                             ConditionVariable* condition_variable);

  Mutex mutex_;
  SbThread thread_ = kSbThreadInvalid;
  JobQueue* job_queue_ = NULL;
  std::map<IClient*, JobQueue::JobOwner*> active_clients_;
};

}  // namespace widevine
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIDEVINE_WIDEVINE_TIMER_H_
