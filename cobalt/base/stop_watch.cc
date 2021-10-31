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

#include "cobalt/base/stop_watch.h"

namespace base {

StopWatch::StopWatch(int id, AutoStart auto_start, StopWatchOwner* owner)
    : id_(id), owner_(owner) {
  if (auto_start == kAutoStartOn) {
    Start();
  }
}

StopWatch::~StopWatch() { Stop(); }

void StopWatch::Start() {
  if (!IsCounting() && owner_->IsStopWatchEnabled(id_)) {
    start_time_ = base::TimeTicks::Now();
  }
}

void StopWatch::Stop() {
  if (IsCounting()) {
    base::TimeDelta time_elapsed = base::TimeTicks::Now() - start_time_;
    start_time_ = base::TimeTicks();
    owner_->OnStopWatchStopped(id_, time_elapsed);
  }
}

bool StopWatch::IsCounting() const { return !start_time_.is_null(); }

}  // namespace base
