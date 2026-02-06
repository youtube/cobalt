// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is- distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/performance/startup_time.h"

#include <atomic>

#include "base/time/time.h"

#include "starboard/common/log.h" // nogncheck

namespace cobalt {
namespace browser {

namespace {
std::atomic<int64_t> g_startup_time{0};
std::atomic<int64_t> g_startup_time1{0};
}  // namespace

void SetStartupTime(int64_t startup_time) {
  g_startup_time.store(startup_time);
}

void SetStartupTime1(int64_t startup_time1) {
  g_startup_time1.store(startup_time1);
}

int64_t GetStartupTime() {
  SB_LOG(INFO) << "potential startup time"
               << g_startup_time.load() - g_startup_time1.load();
  return g_startup_time.load();
}

int64_t GetStartupTime1() {
  return g_startup_time1.load();
}

}  // namespace browser
}  // namespace cobalt
