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

#include "cobalt/base/startup_timer.h"

#include "starboard/once.h"

namespace base {
namespace StartupTimer {
namespace {

// StartupTimer is designed to measure time since the startup of the app.
// It is loader initialized to have the most accurate start time possible.
class Impl {
 public:
  static Impl* Instance();
  base::TimeTicks StartTime() const { return start_time_; }
  base::TimeDelta TimeElapsed() const {
    return base::TimeTicks::Now() - start_time_;
  }

 private:
  Impl() : start_time_(base::TimeTicks::Now()) {}

  base::TimeTicks start_time_;
};

SB_ONCE_INITIALIZE_FUNCTION(Impl, Impl::Instance);
Impl* s_on_startup_init_dont_use = Impl::Instance();

}  // namespace

TimeTicks StartTime() { return Impl::Instance()->StartTime(); }
TimeDelta TimeElapsed() { return Impl::Instance()->TimeElapsed(); }

}  // namespace StartupTimer
}  // namespace base
