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

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/values.h"
#include "cobalt/watchdog/watchdog.h"
#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"

#if defined(_DEBUG)
#include "cobalt/browser/switches.h"
#endif  // defined(_DEBUG)

namespace cobalt {
namespace watchdog {

namespace {

// The Watchdog violations json file names.
const char kWatchdogCurrentViolationsJson[] = "watchdog.json";
const char kWatchdogPreviousViolationsJson[] = "watchdog_old.json";
// The default number of microseconds between each monitor loop.
const int64_t kWatchdogSmallestTimeInterval = 1000000;
// The maximum number of repeated Watchdog violations. Prevents excessive
// Watchdog violation updates.
const int64_t kWatchdogMaxViolations = 100;
// The maximum number of most recent ping infos to store.
const int64_t kWatchdogMaxPingInfos = 100;

// Persistent setting name and default setting for the boolean that controls
// whether or not crashes can be triggered.
const char kPersistentSettingWatchdogCrash[] =
    "kPersistentSettingWatchdogCrash";
const bool kDefaultSettingWatchdogCrash = false;

}  // namespace

bool Watchdog::Initialize(
    persistent_storage::PersistentSettings* persistent_settings) {
  SB_CHECK(SbMutexCreate(&mutex_));
  smallest_time_interval_ = kWatchdogSmallestTimeInterval;
  persistent_settings_ = persistent_settings;

#if defined(_DEBUG)
  // Sets Watchdog delay settings from command line switch.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(browser::switches::kWatchdog)) {
    std::string watchdog_settings =
        command_line->GetSwitchValueASCII(browser::switches::kWatchdog);
    std::istringstream ss(watchdog_settings);

    std::string delay_name;
    std::getline(ss, delay_name, ',');
    delay_name_ = delay_name;

    std::string delay_wait_time_microseconds;
    std::getline(ss, delay_wait_time_microseconds, ',');
    if (delay_wait_time_microseconds != "")
      delay_wait_time_microseconds_ = std::stoll(delay_wait_time_microseconds);

    std::string delay_sleep_time_microseconds;
    std::getline(ss, delay_sleep_time_microseconds, ',');
    if (delay_sleep_time_microseconds != "")
      delay_sleep_time_microseconds_ =
          std::stoll(delay_sleep_time_microseconds);
  }
#endif  // defined(_DEBUG)

  // Starts monitor thread.
  is_monitoring_ = true;
  SB_DCHECK(!SbThreadIsValid(watchdog_thread_));
  watchdog_thread_ = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                    true, "Watchdog", &Watchdog::Monitor, this);
  SB_DCHECK(SbThreadIsValid(watchdog_thread_));
  return true;
}

bool Watchdog::InitializeStub() {
  is_stub_ = true;
  return true;
}

void Watchdog::Uninitialize() {
  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  is_monitoring_ = false;
  SB_CHECK(SbMutexRelease(&mutex_));
  SbThreadJoin(watchdog_thread_, nullptr);
}

std::string Watchdog::GetWatchdogFilePaths(bool current) {
  // Gets the current Watchdog violations file path or the previous Watchdog
  // violations file path.
  if (watchdog_file_ == "") {
    // Sets Watchdog violations file paths.
    std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
    SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                    kSbFileMaxPath);
    watchdog_file_ = std::string(cache_dir.data()) + kSbFileSepString +
                     std::string(kWatchdogCurrentViolationsJson);
    SB_LOG(INFO) << "Current Watchdog violations file path: " << watchdog_file_;
    watchdog_old_file_ = std::string(cache_dir.data()) + kSbFileSepString +
                         std::string(kWatchdogPreviousViolationsJson);
    SB_LOG(INFO) << "Previous Watchdog violations file path: "
                 << watchdog_old_file_;
    PreservePreviousWatchdogViolations();
  }
  if (current) return watchdog_file_;
  return watchdog_old_file_;
}

void Watchdog::PreservePreviousWatchdogViolations() {
  // Copies the previous Watchdog violations file containing violations since
  // last app start, if it exists, to preserve it.
  starboard::ScopedFile read_file(watchdog_file_.c_str(),
                                  kSbFileOpenOnly | kSbFileRead);
  if (read_file.IsValid()) {
    int64_t kFileSize = read_file.GetSize();
    std::string watchdog_content(kFileSize + 1, '\0');
    read_file.ReadAll(&watchdog_content[0], kFileSize);
    starboard::ScopedFile write_file(watchdog_old_file_.c_str(),
                                     kSbFileCreateAlways | kSbFileWrite);
    write_file.WriteAll(&watchdog_content[0], kFileSize);
    starboard::SbFileDeleteRecursive(watchdog_file_.c_str(), true);
  }
}

void Watchdog::UpdateState(base::ApplicationState state) {
  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  state_ = state;
  SB_CHECK(SbMutexRelease(&mutex_));
}

void* Watchdog::Monitor(void* context) {
  while (1) {
    SB_CHECK(SbMutexAcquire(&(static_cast<Watchdog*>(context))->mutex_) ==
             kSbMutexAcquired);

    if (!((static_cast<Watchdog*>(context))->is_monitoring_)) {
      SB_CHECK(SbMutexRelease(&(static_cast<Watchdog*>(context))->mutex_));
      break;
    }

    int64_t current_time = SbTimeToPosix(SbTimeGetNow());
    SbTimeMonotonic current_monotonic_time = SbTimeGetMonotonicNow();
    std::string serialized_watchdog_index = "";

    // Iterates through Watchdog index to monitor all registered clients.
    bool new_watchdog_violation = false;
    for (auto& it : static_cast<Watchdog*>(context)->watchdog_index_) {
      // Ignores and resets clients in idle states.
      if (static_cast<Watchdog*>(context)->state_ > it.second->monitor_state) {
        it.second->time_registered_monotonic_microseconds =
            current_monotonic_time;
        it.second->time_last_pinged_microseconds = current_monotonic_time;
        continue;
      }

      SbTimeMonotonic time_delta =
          current_monotonic_time - it.second->time_last_pinged_microseconds;
      SbTimeMonotonic time_wait =
          current_monotonic_time -
          it.second->time_registered_monotonic_microseconds;

      // Watchdog violation
      if (time_delta > it.second->time_interval_microseconds &&
          time_wait > it.second->time_wait_microseconds) {
        // Reset time last pinged.
        it.second->time_last_pinged_microseconds = current_monotonic_time;
        // Get serialized Watchdog index.
        if (serialized_watchdog_index == "") {
          serialized_watchdog_index =
              static_cast<Watchdog*>(context)->GetSerializedWatchdogIndex();
        }

        // Updates Watchdog violations.
        auto iter = (static_cast<Watchdog*>(context)->watchdog_violations_)
                        .find(it.second->name);
        bool already_violated =
            iter !=
            (static_cast<Watchdog*>(context)->watchdog_violations_).end();

        if (already_violated) {
          // Prevents excessive Watchdog violation updates.
          if (iter->second->violation_count <= kWatchdogMaxViolations)
            new_watchdog_violation = true;
          iter->second->ping_infos = it.second->ping_infos;
          iter->second->violation_time_microseconds = current_time;
          iter->second->violation_delta_microseconds = time_delta;
          iter->second->violation_count++;
          iter->second->serialized_watchdog_index = serialized_watchdog_index;
        } else {
          new_watchdog_violation = true;
          std::unique_ptr<Violation> violation(new Violation);
          *violation = *(it.second);
          violation->violation_time_microseconds = current_time;
          violation->violation_delta_microseconds = time_delta;
          violation->violation_count = 1;
          violation->serialized_watchdog_index = serialized_watchdog_index;
          (static_cast<Watchdog*>(context)->watchdog_violations_)
              .emplace(violation->name, std::move(violation));
        }
      }
    }
    if (new_watchdog_violation) SerializeWatchdogViolations(context);

    SB_CHECK(SbMutexRelease(&(static_cast<Watchdog*>(context))->mutex_));
    SbThreadSleep(static_cast<Watchdog*>(context)->smallest_time_interval_);
  }
  return nullptr;
}

std::string Watchdog::GetSerializedWatchdogIndex() {
  // Gets the current list of registered clients from the Watchdog index and
  // returns it as a serialized json string.
  std::string serialized_watchdog_index = "[";
  std::string comma = "";
  for (auto& it : watchdog_index_) {
    serialized_watchdog_index += (comma + "\"" + it.first + "\"");
    comma = ", ";
  }
  serialized_watchdog_index += "]";
  return serialized_watchdog_index;
}

void Watchdog::SerializeWatchdogViolations(void* context) {
  // Writes current Watchdog violations to persistent storage as a json file.
  std::string watchdog_json = "{\n  \"watchdog_violations\": [\n";
  std::string comma = "";
  for (auto& it : static_cast<Watchdog*>(context)->watchdog_violations_) {
    std::string ping_infos = "[";
    std::string inner_comma = "";
    while (it.second->ping_infos.size() > 0) {
      ping_infos += (inner_comma + "\"" + it.second->ping_infos.front() + "\"");
      it.second->ping_infos.pop();
      inner_comma = ", ";
    }
    ping_infos += "]";

    std::ostringstream ss;
    ss << comma << "    {\n"
       << "      \"name\": \"" << it.second->name << "\",\n"
       << "      \"description\": \"" << it.second->description << "\",\n"
       << "      \"ping_infos\": " << ping_infos << ",\n"
       << "      \"monitor_state\": \""
       << std::string(GetApplicationStateString(it.second->monitor_state))
       << "\",\n"
       << "      \"time_interval_microseconds\": "
       << it.second->time_interval_microseconds << ",\n"
       << "      \"time_wait_microseconds\": "
       << it.second->time_wait_microseconds << ",\n"
       << "      \"time_registered_microseconds\": "
       << it.second->time_registered_microseconds << ",\n"
       << "      \"violation_time_microseconds\": "
       << it.second->violation_time_microseconds << ",\n"
       << "      \"violation_delta_microseconds\": "
       << it.second->violation_delta_microseconds << ",\n"
       << "      \"violation_count\": " << it.second->violation_count << ",\n"
       << "      \"watchdog_index\": " << it.second->serialized_watchdog_index
       << "\n"
       << "    }";
    watchdog_json += ss.str();
    comma = ",\n";
  }
  watchdog_json += "\n  ]\n}";

  SB_LOG(INFO) << "Writing Watchdog violations to: "
               << static_cast<Watchdog*>(context)->GetWatchdogFilePaths(true);
  SB_LOG(INFO) << watchdog_json;

  starboard::ScopedFile watchdog_file(
      (static_cast<Watchdog*>(context)->GetWatchdogFilePaths(true)).c_str(),
      kSbFileCreateAlways | kSbFileWrite);
  watchdog_file.WriteAll(watchdog_json.c_str(),
                         static_cast<int>(watchdog_json.size()));

  MaybeTriggerCrash(context);
}

void Watchdog::MaybeTriggerCrash(void* context) {
  if (static_cast<Watchdog*>(context)->GetPersistentSettingWatchdogCrash()) {
    SB_LOG(ERROR) << "Triggering Watchdog Violation Crash!";
    CHECK(false);
  }
}

bool Watchdog::Register(std::string name, base::ApplicationState monitor_state,
                        int64_t time_interval, int64_t time_wait,
                        Replace replace) {
  return Register(name, name, monitor_state, time_interval, time_wait, replace);
}

bool Watchdog::Register(std::string name, std::string description,
                        base::ApplicationState monitor_state,
                        int64_t time_interval, int64_t time_wait,
                        Replace replace) {
  // Watchdog stub
  if (is_stub_) return true;

  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);

  // If replace is PING or ALL, handles already registered cases.
  if (replace != NONE) {
    auto it = watchdog_index_.find(name);
    bool already_registered = it != watchdog_index_.end();

    if (already_registered) {
      if (replace == PING) {
        it->second->time_last_pinged_microseconds = SbTimeGetMonotonicNow();
        SB_CHECK(SbMutexRelease(&mutex_));
        return true;
      }
      if (replace == ALL) Unregister(name, false);
    }
  }

  // Creates new Client.
  std::unique_ptr<Client> client(new Client);
  client->name = name;
  client->description = description;
  client->ping_infos = std::queue<std::string>();
  client->monitor_state = monitor_state;
  client->time_interval_microseconds = time_interval;
  client->time_wait_microseconds = time_wait;
  client->time_registered_microseconds = SbTimeToPosix(SbTimeGetNow());
  client->time_registered_monotonic_microseconds = SbTimeGetMonotonicNow();
  client->time_last_pinged_microseconds =
      client->time_registered_monotonic_microseconds;

  // Registers.
  auto result = watchdog_index_.emplace(name, std::move(client));
  // Checks for new smallest_time_interval_.
  smallest_time_interval_ = std::min(smallest_time_interval_, time_interval);

  SB_CHECK(SbMutexRelease(&mutex_));

  if (result.second) {
    SB_DLOG(INFO) << "Watchdog Registered: " << name;
  } else {
    SB_DLOG(ERROR) << "Watchdog Unable to Register: " << name;
  }
  return result.second;
}

bool Watchdog::Unregister(const std::string& name, bool lock) {
  // Watchdog stub
  if (is_stub_) return true;

  if (lock) SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  // Unregisters.
  auto result = watchdog_index_.erase(name);
  // Sets new smallest_time_interval_.
  smallest_time_interval_ = kWatchdogSmallestTimeInterval;
  for (auto& it : watchdog_index_) {
    smallest_time_interval_ = std::min(smallest_time_interval_,
                                       it.second->time_interval_microseconds);
  }
  if (lock) SB_CHECK(SbMutexRelease(&mutex_));

  if (result) {
    SB_DLOG(INFO) << "Watchdog Unregistered: " << name;
  } else {
    SB_DLOG(ERROR) << "Watchdog Unable to Unregister: " << name;
  }
  return result;
}

bool Watchdog::Ping(const std::string& name) { return Ping(name, ""); }

bool Watchdog::Ping(const std::string& name, const std::string& info) {
  // Watchdog stub
  if (is_stub_) return true;

  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  auto it = watchdog_index_.find(name);
  bool client_exists = it != watchdog_index_.end();

  if (client_exists) {
    // Updates last ping.
    it->second->time_last_pinged_microseconds = SbTimeGetMonotonicNow();
    if (info != "") {
      int64_t current_time = SbTimeToPosix(SbTimeGetNow());
      it->second->ping_infos.push(std::to_string(current_time) + "\\n" + info +
                                  "\\n");
      if (it->second->ping_infos.size() > kWatchdogMaxPingInfos)
        it->second->ping_infos.pop();
    }
  } else {
    SB_DLOG(ERROR) << "Watchdog Unable to Ping: " << name;
  }
  SB_CHECK(SbMutexRelease(&mutex_));
  return client_exists;
}

std::string Watchdog::GetWatchdogViolations(bool current) {
  // Gets the current Watchdog violations file representing ongoing violations
  // or gets the previous Watchdog violations file containing violations from
  // previous app starts and since the last call (up to a limit).

  // Watchdog stub
  if (is_stub_) return "";

  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  starboard::ScopedFile read_file(GetWatchdogFilePaths(current).c_str(),
                                  kSbFileOpenOnly | kSbFileRead);
  if (read_file.IsValid()) {
    int64_t kFileSize = read_file.GetSize();
    std::string watchdog_content(kFileSize + 1, '\0');
    read_file.ReadAll(&watchdog_content[0], kFileSize);
    SB_CHECK(SbMutexRelease(&mutex_));
    SB_LOG(INFO) << "Reading Watchdog violations:\n" << watchdog_content;
    return watchdog_content;
  } else {
    SB_CHECK(SbMutexRelease(&mutex_));
    SB_LOG(INFO) << "No Watchdog Violations.";
    return "";
  }
}

bool Watchdog::GetPersistentSettingWatchdogCrash() {
  // Watchdog stub
  if (is_stub_) return kDefaultSettingWatchdogCrash;

  // Gets the boolean that controls whether or not crashes can be triggered.
  return persistent_settings_->GetPersistentSettingAsBool(
      kPersistentSettingWatchdogCrash, kDefaultSettingWatchdogCrash);
}

void Watchdog::SetPersistentSettingWatchdogCrash(bool can_trigger_crash) {
  // Watchdog stub
  if (is_stub_) return;

  // Sets the boolean that controls whether or not crashes can be triggered.
  persistent_settings_->SetPersistentSetting(
      kPersistentSettingWatchdogCrash,
      std::make_unique<base::Value>(can_trigger_crash));
}

#if defined(_DEBUG)
// Sleeps threads based off of environment variables for Watchdog debugging.
void Watchdog::MaybeInjectDebugDelay(const std::string& name) {
  // Watchdog stub
  if (is_stub_) return;

  starboard::ScopedLock scoped_lock(delay_lock_);

  if (name != delay_name_) return;

  SbTimeMonotonic current_time = SbTimeGetMonotonicNow();
  if (time_last_delayed_microseconds_ == 0)
    time_last_delayed_microseconds_ = current_time;

  if (current_time >
      time_last_delayed_microseconds_ + delay_wait_time_microseconds_) {
    SbThreadSleep(delay_sleep_time_microseconds_);
    time_last_delayed_microseconds_ = SbTimeGetMonotonicNow();
  }
}
#endif  // defined(_DEBUG)

}  // namespace watchdog
}  // namespace cobalt
