// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WATCHDOG_WATCHDOG_H_
#define COBALT_WATCHDOG_WATCHDOG_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/values.h"
#include "cobalt/base/application_state.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "cobalt/watchdog/singleton.h"
#include "starboard/atomic.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/thread.h"
#include "starboard/time.h"

namespace cobalt {
namespace watchdog {

// Client to monitor
typedef struct Client {
  std::string name;
  std::string description;
  // List of strings optionally provided with each Ping.
  base::Value ping_infos;
  // Application state to continue monitoring client up to. Inclusive.
  base::ApplicationState monitor_state;
  // Maximum number of microseconds allowed between pings before triggering a
  // Watchdog violation.
  int64_t time_interval_microseconds;
  // Number of microseconds to initially wait before Watchdog violations can be
  // triggered. Reapplies after client resumes from idle state due to
  // application state changes.
  int64_t time_wait_microseconds;
  // Epoch time when client was registered.
  int64_t time_registered_microseconds;
  // Monotonically increasing timestamp when client was registered. Used as the
  // start value for time wait calculations.
  SbTimeMonotonic time_registered_monotonic_microseconds;
  // Epoch time when client was last pinged. Set by Ping() and Register() when
  // in PING replace mode or set initially by Register().
  int64_t time_last_pinged_microseconds;
  // Monotonically increasing timestamp when client was last updated. Set by
  // Ping() and Register() when in PING replace mode or set initially by
  // Register(). Also reset by Monitor() when in idle states or when a
  // violation occurs. Prevents excessive violations as they must occur
  // time_interval_microseconds apart rather than on each monitor loop. Used as
  // the start value for time interval calculations.
  SbTimeMonotonic time_last_updated_monotonic_microseconds;
} Client;

// Register behavior with previously registered clients of the same name.
enum Replace {
  NONE = 0,
  PING = 1,
  ALL = 2,
};

class Watchdog : public Singleton<Watchdog> {
 public:
  bool Initialize(persistent_storage::PersistentSettings* persistent_settings);
  // Directly used for testing only.
  bool InitializeCustom(
      persistent_storage::PersistentSettings* persistent_settings,
      std::string watchdog_file_name, int64_t watchdog_monitor_frequency);
  bool InitializeStub();
  void Uninitialize();
  std::string GetWatchdogFilePath();
  void UpdateState(base::ApplicationState state);
  bool Register(std::string name, std::string description,
                base::ApplicationState monitor_state,
                int64_t time_interval_microseconds,
                int64_t time_wait_microseconds = 0, Replace replace = NONE);
  bool Unregister(const std::string& name, bool lock = true);
  bool Ping(const std::string& name);
  bool Ping(const std::string& name, const std::string& info);
  std::string GetWatchdogViolations(bool clear = true);
  bool GetPersistentSettingWatchdogEnable();
  void SetPersistentSettingWatchdogEnable(bool enable_watchdog);
  bool GetPersistentSettingWatchdogCrash();
  void SetPersistentSettingWatchdogCrash(bool can_trigger_crash);

#if defined(_DEBUG)
  // Sleeps threads based off of environment variables for Watchdog debugging.
  void MaybeInjectDebugDelay(const std::string& name);
#endif  // defined(_DEBUG)

 private:
  void WriteWatchdogViolations();
  static void* Monitor(void* context);
  static void UpdateViolationsMap(void* context, Client* client,
                                  SbTimeMonotonic time_delta);
  static void InitializeViolationsMap(void* context);
  static void EvictWatchdogViolation(void* context);
  static void MaybeWriteWatchdogViolations(void* context);
  static void MaybeTriggerCrash(void* context);

  // The Watchdog violations json filename.
  std::string watchdog_file_name_;
  // The Watchdog violations json filepath.
  std::string watchdog_file_path_;
  // Access to persistent settings.
  persistent_storage::PersistentSettings* persistent_settings_;
  // Flag to disable Watchdog. When disabled, Watchdog behaves like a stub
  // except that persistent settings can still be get/set.
  bool is_disabled_;
  // Creates a lock which ensures that each loop of monitor is atomic in that
  // modifications to is_monitoring_, state_, and most importantly to the
  // dictionaries containing Watchdog clients, client_map_ and violations_map_,
  // only occur in between loops of monitor. API functions like Register(),
  // Unregister(), Ping(), and GetWatchdogViolations() will be called by
  // various threads and interact with these class variables.
  starboard::Mutex mutex_;
  // Tracks application state.
  base::ApplicationState state_ = base::kApplicationStateStarted;
  // Flag to trigger Watchdog violations writes to persistent storage.
  bool pending_write_;
  // Monotonically increasing timestamp when Watchdog violations were last
  // written to persistent storage. 0 indicates that it has never been written.
  SbTimeMonotonic time_last_written_microseconds_ = 0;
  // Number of microseconds between writes.
  int64_t write_wait_time_microseconds_;
  // Dictionary of registered Watchdog clients.
  std::unordered_map<std::string, std::unique_ptr<Client>> client_map_;
  // Dictionary of lists of Watchdog violations represented as dictionaries.
  std::unique_ptr<base::Value> violations_map_;
  // Number of violations in violations_map_;
  int violations_count_;
  // Monitor thread.
  SbThread watchdog_thread_;
  // Flag to stop monitor thread.
  starboard::atomic_bool is_monitoring_;
  // Conditional Variable to wait and shutdown monitor thread.
  starboard::ConditionVariable monitor_wait_ =
      starboard::ConditionVariable(mutex_);
  // The frequency in microseconds of monitor loops.
  int64_t watchdog_monitor_frequency_;

#if defined(_DEBUG)
  starboard::Mutex delay_mutex_;
  // Name of the client to inject a delay for.
  std::string delay_name_ = "";
  // Monotonically increasing timestamp when a delay was last injected. 0
  // indicates that it has never been injected.
  SbTimeMonotonic time_last_delayed_microseconds_ = 0;
  // Number of microseconds between delays.
  int64_t delay_wait_time_microseconds_ = 0;
  // Number of microseconds to delay.
  int64_t delay_sleep_time_microseconds_ = 0;
#endif
};

}  // namespace watchdog
}  // namespace cobalt

#endif  // COBALT_WATCHDOG_WATCHDOG_H_
