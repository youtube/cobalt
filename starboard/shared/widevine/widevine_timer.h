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
#include <memory>
#include <mutex>

#include "starboard/shared/starboard/player/job_thread.h"
#include "third_party/internal/ce_cdm/cdm/include/cdm.h"

namespace starboard {

// Manages the scheduled callbacks of Widevine.  All its public functions can
// be called from any threads.
class WidevineTimer : public ::widevine::Cdm::ITimer {
 public:
  WidevineTimer();
  ~WidevineTimer() override;

  // Call |client->onTimerExpired(context)| after |delay_in_milliseconds|.
  void setTimeout(int64_t delay_in_milliseconds,
                  IClient* client,
                  void* context) override;

  // Cancel all timers associated with |client|.  No timer should be called on
  // the specific client after this function returns.
  void cancel(IClient* client) override;

 private:
  const std::unique_ptr<JobThread> job_thread_;
  std::mutex mutex_;
  std::map<IClient*, JobQueue::JobOwner*>
      active_clients_;  // Guarded by |mutex_|.
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_WIDEVINE_WIDEVINE_TIMER_H_
