// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-20.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/loader_app/startup_timer.h"

#include "starboard/common/once.h"
#include "starboard/common/time.h"

namespace loader_app {
namespace StartupTimer {
namespace {

class Impl {
 public:
  static Impl* Instance() {
    static Impl s_instance;
    return &s_instance;
  }
  int64_t StartTime() const { return start_time_; }
  int64_t TimeElapsed() const {
    return starboard::CurrentMonotonicTime() - start_time_;
  }

  void SetAppStartupTime() {
    pthread_once(&set_app_startup_time_once_, &Impl::SetAppStartupTimeOnce);
  }
  int64_t GetAppStartupTime() const { return app_startup_time_; }

 private:
  Impl() : start_time_(starboard::CurrentMonotonicTime()) {}

  static void SetAppStartupTimeOnce() {
    Instance()->app_startup_time_ = Instance()->TimeElapsed();
  }

  int64_t start_time_;
  int64_t app_startup_time_ = 0;
  pthread_once_t set_app_startup_time_once_ = PTHREAD_ONCE_INIT;
};

}  // namespace

int64_t StartTime() {
  return Impl::Instance()->StartTime();
}
int64_t TimeElapsed() {
  return Impl::Instance()->TimeElapsed();
}

void SetAppStartupTime() {
  Impl::Instance()->SetAppStartupTime();
}

SB_EXPORT int64_t GetAppStartupTime() {
  return Impl::Instance()->GetAppStartupTime();
}

}  // namespace StartupTimer
}  // namespace loader_app
