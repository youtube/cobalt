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

#ifndef COBALT_H5VCC_H5VCC_CRASH_LOG_H_
#define COBALT_H5VCC_H5VCC_CRASH_LOG_H_

#include <string>

#include "cobalt/h5vcc/h5vcc_crash_type.h"
#include "cobalt/h5vcc/watchdog_replace.h"
#include "cobalt/h5vcc/watchdog_state.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/watchdog/watchdog.h"

namespace cobalt {
namespace h5vcc {

class H5vccCrashLog : public script::Wrappable {
 public:
  H5vccCrashLog() {}

  bool SetString(const std::string& key, const std::string& value);

  void TriggerCrash(H5vccCrashType intent);

  bool Register(const std::string& name, const std::string& description,
                WatchdogState watchdog_state,
                int64_t time_interval_milliseconds,
                int64_t time_wait_milliseconds,
                WatchdogReplace watchdog_replace);

  bool Unregister(const std::string& name);

  bool Ping(const std::string& name, const std::string& ping_info);

  std::string GetWatchdogViolations();

  bool GetPersistentSettingWatchdogEnable();

  void SetPersistentSettingWatchdogEnable(bool enable_watchdog);

  bool GetPersistentSettingWatchdogCrash();

  void SetPersistentSettingWatchdogCrash(bool can_trigger_crash);

  DEFINE_WRAPPABLE_TYPE(H5vccCrashLog);

 private:
  DISALLOW_COPY_AND_ASSIGN(H5vccCrashLog);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_CRASH_LOG_H_
