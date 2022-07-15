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
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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

// The Watchdog violations json filename.
const char kWatchdogViolationsJson[] = "watchdog.json";
// The default number of microseconds between each monitor loop.
const int64_t kWatchdogSmallestTimeInterval = 1000000;
// The maximum number of most recent repeated Watchdog violations.
const int64_t kWatchdogMaxViolations = 100;
// The maximum number of most recent ping infos.
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
  persistent_settings_ = persistent_settings;
  smallest_time_interval_ = kWatchdogSmallestTimeInterval;

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

std::string Watchdog::GetWatchdogFilePath() {
  // Gets the Watchdog violations file path with lazy initialization.
  if (watchdog_file_ == "") {
    // Sets Watchdog violations file path.
    std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
    SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                    kSbFileMaxPath);
    watchdog_file_ = std::string(cache_dir.data()) + kSbFileSepString +
                     std::string(kWatchdogViolationsJson);
    SB_LOG(INFO) << "[Watchdog] Violations filepath: " << watchdog_file_;
  }
  return watchdog_file_;
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
    base::Value registered_clients(base::Value::Type::LIST);

    // Iterates through client map to monitor all registered clients.
    bool watchdog_violation = false;
    for (auto& it : static_cast<Watchdog*>(context)->client_map_) {
      Client* client = it.second.get();
      // Ignores and resets clients in idle states, clients whose monitor_state
      // is below the current application state. Resets time_wait_microseconds
      // and time_interval_microseconds deltas.
      if (static_cast<Watchdog*>(context)->state_ > client->monitor_state) {
        client->time_registered_monotonic_microseconds = current_monotonic_time;
        client->time_last_pinged_monotonic_microseconds =
            current_monotonic_time;
        continue;
      }

      SbTimeMonotonic time_delta =
          current_monotonic_time -
          client->time_last_pinged_monotonic_microseconds;
      SbTimeMonotonic time_wait =
          current_monotonic_time -
          client->time_registered_monotonic_microseconds;

      // Watchdog violation
      if (time_delta > client->time_interval_microseconds &&
          time_wait > client->time_wait_microseconds) {
        watchdog_violation = true;

        // Resets time last pinged.
        client->time_last_pinged_monotonic_microseconds =
            current_monotonic_time;

        // Creates new violation.
        base::Value violation(base::Value::Type::DICTIONARY);
        violation.SetKey("ping_infos", client->ping_infos.Clone());
        violation.SetKey("monitor_state",
                         base::Value(std::string(GetApplicationStateString(
                             client->monitor_state))));
        violation.SetKey(
            "time_interval_microseconds",
            base::Value(static_cast<int>(client->time_interval_microseconds)));
        violation.SetKey(
            "time_wait_microseconds",
            base::Value(static_cast<int>(client->time_wait_microseconds)));
        violation.SetKey(
            "registered_timestamp_microseconds",
            base::Value(std::to_string(client->time_registered_microseconds)));
        violation.SetKey("violation_timestamp_microseconds",
                         base::Value(std::to_string(current_time)));
        violation.SetKey("violation_delta_microseconds",
                         base::Value(static_cast<int>(time_delta)));
        if (registered_clients.GetList().empty()) {
          for (auto& it : static_cast<Watchdog*>(context)->client_map_) {
            registered_clients.GetList().emplace_back(base::Value(it.first));
          }
        }
        violation.SetKey("registered_clients", registered_clients.Clone());

        // Updates violations_map_.
        if (static_cast<Watchdog*>(context)->violations_map_ == nullptr)
          InitializeViolationsMap(context);
        base::Value* value = (static_cast<Watchdog*>(context)->violations_map_)
                                 ->FindKey(client->name);
        if (value) {
          base::Value* violations = value->FindKey("violations");
          violations->GetList().emplace_back(violation.Clone());
          if (violations->GetList().size() > kWatchdogMaxViolations)
            violations->GetList().erase(violations->GetList().begin());
        } else {
          base::Value dict(base::Value::Type::DICTIONARY);
          dict.SetKey("description", base::Value(client->description));
          base::Value list(base::Value::Type::LIST);
          list.GetList().emplace_back(violation.Clone());
          dict.SetKey("violations", list.Clone());
          (static_cast<Watchdog*>(context)->violations_map_)
              ->SetKey(client->name, dict.Clone());
        }
      }
    }
    if (watchdog_violation) {
      WriteWatchdogViolations(context);
      MaybeTriggerCrash(context);
    }

    SB_CHECK(SbMutexRelease(&(static_cast<Watchdog*>(context))->mutex_));
    SbThreadSleep(static_cast<Watchdog*>(context)->smallest_time_interval_);
  }
  return nullptr;
}

void Watchdog::InitializeViolationsMap(void* context) {
  // Loads the previous Watchdog violations file containing violations before
  // app start, if it exists, to populate violations_map_.
  starboard::ScopedFile read_file(
      (static_cast<Watchdog*>(context)->GetWatchdogFilePath()).c_str(),
      kSbFileOpenOnly | kSbFileRead);
  if (read_file.IsValid()) {
    int64_t kFileSize = read_file.GetSize();
    std::vector<char> buffer(kFileSize + 1, 0);
    read_file.ReadAll(buffer.data(), kFileSize);
    std::string watchdog_json = std::string(buffer.data());
    static_cast<Watchdog*>(context)->violations_map_ =
        base::JSONReader::Read(watchdog_json);
  }
  if (static_cast<Watchdog*>(context)->violations_map_ == nullptr) {
    SB_LOG(INFO) << "[Watchdog] No previous violations JSON.";
    static_cast<Watchdog*>(context)->violations_map_ =
        std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  }
}

void Watchdog::WriteWatchdogViolations(void* context) {
  // Writes Watchdog violations to persistent storage as a json file.
  std::string watchdog_json;
  base::JSONWriter::Write(*(static_cast<Watchdog*>(context)->violations_map_),
                          &watchdog_json);

  SB_LOG(INFO) << "[Watchdog] Writing violations to JSON:\n" << watchdog_json;

  starboard::ScopedFile watchdog_file(
      (static_cast<Watchdog*>(context)->GetWatchdogFilePath()).c_str(),
      kSbFileCreateAlways | kSbFileWrite);
  watchdog_file.WriteAll(watchdog_json.c_str(),
                         static_cast<int>(watchdog_json.size()));
}

void Watchdog::MaybeTriggerCrash(void* context) {
  if (static_cast<Watchdog*>(context)->GetPersistentSettingWatchdogCrash()) {
    SB_LOG(ERROR) << "[Watchdog] Triggering violation Crash!";
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
    auto it = client_map_.find(name);
    bool already_registered = it != client_map_.end();

    if (already_registered) {
      if (replace == PING) {
        it->second->time_last_pinged_monotonic_microseconds =
            SbTimeGetMonotonicNow();
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
  client->ping_infos = base::Value(base::Value::Type::LIST);
  client->monitor_state = monitor_state;
  client->time_interval_microseconds = time_interval;
  client->time_wait_microseconds = time_wait;
  client->time_registered_microseconds = SbTimeToPosix(SbTimeGetNow());
  client->time_registered_monotonic_microseconds = SbTimeGetMonotonicNow();
  client->time_last_pinged_monotonic_microseconds =
      client->time_registered_monotonic_microseconds;

  // Registers.
  auto result = client_map_.emplace(name, std::move(client));
  // Sets new smallest_time_interval_.
  smallest_time_interval_ = std::min(smallest_time_interval_, time_interval);

  SB_CHECK(SbMutexRelease(&mutex_));

  if (result.second) {
    SB_DLOG(INFO) << "[Watchdog] Registered: " << name;
  } else {
    SB_DLOG(ERROR) << "[Watchdog] Unable to Register: " << name;
  }
  return result.second;
}

bool Watchdog::Unregister(const std::string& name, bool lock) {
  // Watchdog stub
  if (is_stub_) return true;

  if (lock) SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  // Unregisters.
  auto result = client_map_.erase(name);
  // Sets new smallest_time_interval_.
  if (result) {
    smallest_time_interval_ = kWatchdogSmallestTimeInterval;
    for (auto& it : client_map_) {
      smallest_time_interval_ = std::min(smallest_time_interval_,
                                         it.second->time_interval_microseconds);
    }
  }
  if (lock) SB_CHECK(SbMutexRelease(&mutex_));

  if (result) {
    SB_DLOG(INFO) << "[Watchdog] Unregistered: " << name;
  } else {
    SB_DLOG(ERROR) << "[Watchdog] Unable to Unregister: " << name;
  }
  return result;
}

bool Watchdog::Ping(const std::string& name) { return Ping(name, ""); }

bool Watchdog::Ping(const std::string& name, const std::string& info) {
  // Watchdog stub
  if (is_stub_) return true;

  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  auto it = client_map_.find(name);
  bool client_exists = it != client_map_.end();

  if (client_exists) {
    Client* client = it->second.get();
    // Updates last ping.
    client->time_last_pinged_monotonic_microseconds = SbTimeGetMonotonicNow();

    if (info != "") {
      int64_t current_time = SbTimeToPosix(SbTimeGetNow());

      // Creates new ping_info.
      base::Value ping_info(base::Value::Type::DICTIONARY);
      ping_info.SetKey("ping_timestamp_microseconds",
                       base::Value(std::to_string(current_time)));
      ping_info.SetKey("info", base::Value(info));

      client->ping_infos.GetList().emplace_back(ping_info.Clone());
      if (client->ping_infos.GetList().size() > kWatchdogMaxPingInfos)
        client->ping_infos.GetList().erase(
            client->ping_infos.GetList().begin());
    }
  } else {
    SB_DLOG(ERROR) << "[Watchdog] Unable to Ping: " << name;
  }
  SB_CHECK(SbMutexRelease(&mutex_));
  return client_exists;
}

std::string Watchdog::GetWatchdogViolations() {
  // Gets a json string containing the Watchdog violations since the last
  // call (up to the kWatchdogMaxViolations limit).

  // Watchdog stub
  if (is_stub_) return "";

  std::string watchdog_json = "";
  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  starboard::ScopedFile read_file(GetWatchdogFilePath().c_str(),
                                  kSbFileOpenOnly | kSbFileRead);
  if (read_file.IsValid()) {
    int64_t kFileSize = read_file.GetSize();
    std::vector<char> buffer(kFileSize + 1, 0);
    read_file.ReadAll(buffer.data(), kFileSize);
    watchdog_json = std::string(buffer.data());

    // Removes all Watchdog violations.
    if (violations_map_) {
      base::DictionaryValue** dict = nullptr;
      violations_map_->GetAsDictionary(dict);
      if (dict) (*dict)->Clear();
    }
    starboard::SbFileDeleteRecursive(GetWatchdogFilePath().c_str(), true);
    SB_LOG(INFO) << "[Watchdog] Reading violations:\n" << watchdog_json;
  } else {
    SB_LOG(INFO) << "[Watchdog] No violations.";
  }
  SB_CHECK(SbMutexRelease(&mutex_));
  return watchdog_json;
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
