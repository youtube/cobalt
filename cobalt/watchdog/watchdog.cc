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

#include "cobalt/watchdog/watchdog.h"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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
// The frequency in microseconds of monitor loops.
const int64_t kWatchdogMonitorFrequency = 500000;
// The maximum number of Watchdog violations.
const int kWatchdogMaxViolations = 200;
// The minimum number of microseconds between writes.
const int64_t kWatchdogWriteWaitTime = 300000000;
// The maximum number of most recent ping infos.
const int kWatchdogMaxPingInfos = 20;
// The maximum length of each ping info.
const int kWatchdogMaxPingInfoLength = 128;
// The maximum number of milliseconds old of an unfetched Watchdog violation.
const int64_t kWatchdogMaxViolationsAge = 86400000;

// Persistent setting name and default setting for the boolean that controls
// whether or not Watchdog is enabled. When disabled, Watchdog behaves like a
// stub except that persistent settings can still be get/set. Requires a
// restart to take effect.
const char kPersistentSettingWatchdogEnable[] =
    "kPersistentSettingWatchdogEnable";
const bool kDefaultSettingWatchdogEnable = true;
// Persistent setting name and default setting for the boolean that controls
// whether or not a Watchdog violation will trigger a crash.
const char kPersistentSettingWatchdogCrash[] =
    "kPersistentSettingWatchdogCrash";
const bool kDefaultSettingWatchdogCrash = false;

}  // namespace

bool Watchdog::Initialize(
    persistent_storage::PersistentSettings* persistent_settings) {
  return InitializeCustom(persistent_settings,
                          std::string(kWatchdogViolationsJson),
                          kWatchdogMonitorFrequency);
}

bool Watchdog::InitializeCustom(
    persistent_storage::PersistentSettings* persistent_settings,
    std::string watchdog_file_name, int64_t watchdog_monitor_frequency) {
  persistent_settings_ = persistent_settings;
  is_disabled_ = !GetPersistentSettingWatchdogEnable();

  if (is_disabled_) return true;

  watchdog_file_name_ = watchdog_file_name;
  watchdog_monitor_frequency_ = watchdog_monitor_frequency;
  pending_write_ = false;
  write_wait_time_microseconds_ = kWatchdogWriteWaitTime;

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
  is_monitoring_.store(true);
  SB_DCHECK(!SbThreadIsValid(watchdog_thread_));
  watchdog_thread_ = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                    true, "Watchdog", &Watchdog::Monitor, this);
  SB_DCHECK(SbThreadIsValid(watchdog_thread_));
  return true;
}

bool Watchdog::InitializeStub() {
  is_disabled_ = true;
  return true;
}

void Watchdog::Uninitialize() {
  if (is_disabled_) return;

  mutex_.Acquire();
  if (pending_write_) WriteWatchdogViolations();
  is_monitoring_.store(false);
  monitor_wait_.Signal();
  mutex_.Release();
  SbThreadJoin(watchdog_thread_, nullptr);
}

std::shared_ptr<base::Value> Watchdog::GetViolationsMap() {
  // Gets the Watchdog violations map with lazy initialization which loads the
  // previous Watchdog violations file containing violations before app start,
  // if it exists.
  if (violations_map_ == nullptr) {
    starboard::ScopedFile read_file(GetWatchdogFilePath().c_str(),
                                    kSbFileOpenOnly | kSbFileRead);
#ifndef USE_HACKY_COBALT_CHANGES
//     // if (read_file.IsValid()) {
//     //   int64_t kFileSize = read_file.GetSize();
//     //   std::vector<char> buffer(kFileSize + 1, 0);
//     //   read_file.ReadAll(buffer.data(), kFileSize);
//     //   violations_map_ =
//     base::JSONReader::Read(std::string(buffer.data()));
//     // }
#endif

    if (violations_map_ == nullptr) {
      SB_LOG(INFO) << "[Watchdog] No previous violations JSON.";
      violations_map_ = std::make_unique<base::Value>(base::Value::Type::DICT);
    }
  }
  return violations_map_;
}

std::string Watchdog::GetWatchdogFilePath() {
  // Gets the Watchdog violations file path with lazy initialization.
  if (watchdog_file_path_ == "") {
    // Sets Watchdog violations file path.
    std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
    SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                    kSbFileMaxPath);
    watchdog_file_path_ =
        std::string(cache_dir.data()) + kSbFileSepString + watchdog_file_name_;
    SB_LOG(INFO) << "[Watchdog] Violations filepath: " << watchdog_file_path_;
  }
  return watchdog_file_path_;
}

std::vector<std::string> Watchdog::GetWatchdogViolationClientNames() {
  std::vector<std::string> names;

  if (is_disabled_) return names;

  starboard::ScopedLock scoped_lock(mutex_);
  for (const auto& it : GetViolationsMap()->DictItems()) {
    names.push_back(it.first);
  }
  return names;
}

void Watchdog::UpdateState(base::ApplicationState state) {
  if (is_disabled_) return;

  starboard::ScopedLock scoped_lock(mutex_);
  state_ = state;
}

void Watchdog::WriteWatchdogViolations() {
  // Writes Watchdog violations to persistent storage as a json file.
  std::string watchdog_json;
  base::JSONWriter::Write(*GetViolationsMap(), &watchdog_json);
  SB_LOG(INFO) << "[Watchdog] Writing violations to JSON:\n" << watchdog_json;
  starboard::ScopedFile watchdog_file(GetWatchdogFilePath().c_str(),
                                      kSbFileCreateAlways | kSbFileWrite);
  watchdog_file.WriteAll(watchdog_json.c_str(),
                         static_cast<int>(watchdog_json.size()));
  pending_write_ = false;
  time_last_written_microseconds_ = SbTimeGetMonotonicNow();
}

void* Watchdog::Monitor(void* context) {
  starboard::ScopedLock scoped_lock(static_cast<Watchdog*>(context)->mutex_);
  while (1) {
    SbTimeMonotonic current_monotonic_time = SbTimeGetMonotonicNow();
    bool watchdog_violation = false;

    // Iterates through client map to monitor all name registered clients.
    for (auto& it : static_cast<Watchdog*>(context)->client_map_) {
      Client* client = it.second.get();
      if (MonitorClient(context, client, current_monotonic_time)) {
        watchdog_violation = true;
      }
    }

    // Iterates through client list to monitor all client registered clients.
    for (auto& it : static_cast<Watchdog*>(context)->client_list_) {
      Client* client = it.get();
      if (MonitorClient(context, client, current_monotonic_time)) {
        watchdog_violation = true;
      }
    }

    if (static_cast<Watchdog*>(context)->pending_write_)
      MaybeWriteWatchdogViolations(context);
    if (watchdog_violation) MaybeTriggerCrash(context);

    // Wait
    static_cast<Watchdog*>(context)->monitor_wait_.WaitTimed(
        static_cast<Watchdog*>(context)->watchdog_monitor_frequency_);

    // Shutdown
    if (!(static_cast<Watchdog*>(context)->is_monitoring_.load())) break;
  }
  return nullptr;
}

bool Watchdog::MonitorClient(void* context, Client* client,
                             SbTimeMonotonic current_monotonic_time) {
  // Ignores and resets clients in idle states, clients whose monitor_state
  // is below the current application state. Resets time_wait_microseconds
  // and time_interval_microseconds start values.
  if (static_cast<Watchdog*>(context)->state_ > client->monitor_state) {
    client->time_registered_monotonic_microseconds = current_monotonic_time;
    client->time_last_updated_monotonic_microseconds = current_monotonic_time;
    return false;
  }

  SbTimeMonotonic time_delta =
      current_monotonic_time - client->time_last_updated_monotonic_microseconds;
  SbTimeMonotonic time_wait =
      current_monotonic_time - client->time_registered_monotonic_microseconds;

  // Watchdog violation
  if (time_delta > client->time_interval_microseconds &&
      time_wait > client->time_wait_microseconds) {
    UpdateViolationsMap(context, client, time_delta);
    // Resets time last updated.
    client->time_last_updated_monotonic_microseconds = current_monotonic_time;
    return true;
  }
  return false;
}

void Watchdog::UpdateViolationsMap(void* context, Client* client,
                                   SbTimeMonotonic time_delta) {
  // Gets violation dictionary with key client name from violations map.
  base::Value* violation_dict =
      (static_cast<Watchdog*>(context)->GetViolationsMap())
          ->FindKey(client->name);

  // Checks if new unique violation.
  bool new_violation = false;
  if (violation_dict == nullptr) {
    new_violation = true;
  } else {
    // Compares against last_pinged_timestamp_microsecond of last violation.
    base::Value* violations = violation_dict->FindKey("violations");
    int last_index = violations->GetList().size() - 1;
    std::string timestamp_last_pinged_milliseconds =
        violations->GetList()[last_index]
            .FindKey("timestampLastPingedMilliseconds")
            ->GetString();
    if (timestamp_last_pinged_milliseconds !=
        std::to_string(client->time_last_pinged_microseconds / 1000))
      new_violation = true;
  }

  if (new_violation) {
    // New unique violation, creates violation in violations map.
    base::Value violation(base::Value::Type::DICT);
    violation.SetKey("pingInfos", client->ping_infos.Clone());
    violation.SetKey("monitorState",
                     base::Value(std::string(
                         GetApplicationStateString(client->monitor_state))));
    violation.SetKey(
        "timeIntervalMilliseconds",
        base::Value(std::to_string(client->time_interval_microseconds / 1000)));
    violation.SetKey(
        "timeWaitMilliseconds",
        base::Value(std::to_string(client->time_wait_microseconds / 1000)));
    violation.SetKey("timestampRegisteredMilliseconds",
                     base::Value(std::to_string(
                         client->time_registered_microseconds / 1000)));
    violation.SetKey("timestampLastPingedMilliseconds",
                     base::Value(std::to_string(
                         client->time_last_pinged_microseconds / 1000)));
    violation.SetKey(
        "timestampViolationMilliseconds",
        base::Value(std::to_string(SbTimeToPosix(SbTimeGetNow()) / 1000)));
    violation.SetKey(
        "violationDurationMilliseconds",
        base::Value(std::to_string(
            (time_delta - client->time_interval_microseconds) / 1000)));
    base::Value registered_clients(base::Value::Type::LIST);
    for (auto& it : static_cast<Watchdog*>(context)->client_map_) {
      registered_clients.GetList().Append(base::Value(it.first));
    }
    for (auto& it : static_cast<Watchdog*>(context)->client_list_) {
      registered_clients.GetList().Append(base::Value(it->name));
    }
    violation.SetKey("registeredClients", registered_clients.Clone());

    // Adds new violation to violations map.
    if (violation_dict == nullptr) {
      base::Value dict(base::Value::Type::DICT);
      dict.SetKey("description", base::Value(client->description));
      base::Value list(base::Value::Type::LIST);
      list.GetList().Append(violation.Clone());
      dict.SetKey("violations", list.Clone());
      (static_cast<Watchdog*>(context)->GetViolationsMap())
          ->SetKey(client->name, dict.Clone());
    } else {
      base::Value* violations = violation_dict->FindKey("violations");
      violations->GetList().Append(violation.Clone());
    }
  } else {
    // Consecutive non-unique violation, updates violation in violations map.
    base::Value* violations = violation_dict->FindKey("violations");
    int last_index = violations->GetList().size() - 1;
    int64_t violation_duration =
        std::stoll(violations->GetList()[last_index]
                       .FindKey("violationDurationMilliseconds")
                       ->GetString());
    violations->GetList()[last_index].SetKey(
        "violationDurationMilliseconds",
        base::Value(std::to_string(violation_duration + (time_delta / 1000))));
  }
  static_cast<Watchdog*>(context)->pending_write_ = true;

  int violations_count = 0;
  for (auto& it :
       (static_cast<Watchdog*>(context)->GetViolationsMap())->DictItems()) {
    base::Value& violation_dict = it.second;
    base::Value* violations = violation_dict.FindKey("violations");
    violations_count += violations->GetList().size();
  }
  if (violations_count > kWatchdogMaxViolations) {
    EvictWatchdogViolation(context);
  }
}

void Watchdog::EvictWatchdogViolation(void* context) {
  // Evicts a violation in violations map prioritizing first the most frequent
  // violations (largest violations count by client name) and second the oldest
  // violation.
  std::string evicted_name = "";
  int evicted_count = 0;
  int64_t evicted_timestamp_millis = 0;

  for (auto& it :
       (static_cast<Watchdog*>(context)->GetViolationsMap())->DictItems()) {
    std::string name = it.first;
    base::Value& violation_dict = it.second;
    base::Value* violations = violation_dict.FindKey("violations");
    int count = violations->GetList().size();
    int64_t violation_timestamp_millis =
        std::stoll(violations->GetList()[0]
                       .FindKey("timestampViolationMilliseconds")
                       ->GetString());

    if ((evicted_name == "") || (count > evicted_count) ||
        ((count == evicted_count) &&
         (violation_timestamp_millis < evicted_timestamp_millis))) {
      evicted_name = name;
      evicted_count = count;
      evicted_timestamp_millis = violation_timestamp_millis;
    }
  }

  base::Value* violation_dict =
      (static_cast<Watchdog*>(context)->GetViolationsMap())
          ->FindKey(evicted_name);

  if (violation_dict != nullptr) {
    base::Value* violations = violation_dict->FindKey("violations");
    violations->GetList().erase(violations->GetList().begin());
    static_cast<Watchdog*>(context)->pending_write_ = true;

    // Removes empty violations.
    if (violations->GetList().empty()) {
      (static_cast<Watchdog*>(context)->GetViolationsMap())
          ->RemoveKey(evicted_name);
    }
    if (static_cast<Watchdog*>(context)->GetViolationsMap()->DictSize() == 0) {
      starboard::SbFileDeleteRecursive(
          static_cast<Watchdog*>(context)->GetWatchdogFilePath().c_str(), true);
      static_cast<Watchdog*>(context)->pending_write_ = false;
    }
  }
}

void Watchdog::MaybeWriteWatchdogViolations(void* context) {
  if (SbTimeGetMonotonicNow() >
      static_cast<Watchdog*>(context)->time_last_written_microseconds_ +
          static_cast<Watchdog*>(context)->write_wait_time_microseconds_) {
    static_cast<Watchdog*>(context)->WriteWatchdogViolations();
  }
}

void Watchdog::MaybeTriggerCrash(void* context) {
  if (static_cast<Watchdog*>(context)->GetPersistentSettingWatchdogCrash()) {
    if (static_cast<Watchdog*>(context)->pending_write_)
      static_cast<Watchdog*>(context)->WriteWatchdogViolations();
    SB_LOG(ERROR) << "[Watchdog] Triggering violation Crash!";
    CHECK(false);
  }
}

bool Watchdog::Register(std::string name, std::string description,
                        base::ApplicationState monitor_state,
                        int64_t time_interval_microseconds,
                        int64_t time_wait_microseconds, Replace replace) {
  if (is_disabled_) return true;

  starboard::ScopedLock scoped_lock(mutex_);

  int64_t current_time = SbTimeToPosix(SbTimeGetNow());
  SbTimeMonotonic current_monotonic_time = SbTimeGetMonotonicNow();

  // If replace is PING or ALL, handles already registered cases.
  if (replace != NONE) {
    auto it = client_map_.find(name);
    bool already_registered = it != client_map_.end();

    if (already_registered) {
      if (replace == PING) {
        it->second->time_last_pinged_microseconds = current_time;
        it->second->time_last_updated_monotonic_microseconds =
            current_monotonic_time;
        return true;
      }
      if (replace == ALL) Unregister(name, false);
    }
  }

  // Creates new client.
  std::unique_ptr<Client> client = CreateClient(
      name, description, monitor_state, time_interval_microseconds,
      time_wait_microseconds, current_time, current_monotonic_time);
  if (client == nullptr) return false;

  // Registers.
  auto result = client_map_.emplace(name, std::move(client));

  if (result.second) {
    SB_DLOG(INFO) << "[Watchdog] Registered: " << name;
  } else {
    SB_DLOG(ERROR) << "[Watchdog] Unable to Register: " << name;
  }
  return result.second;
}

std::shared_ptr<Client> Watchdog::RegisterByClient(
    std::string name, std::string description,
    base::ApplicationState monitor_state, int64_t time_interval_microseconds,
    int64_t time_wait_microseconds) {
  if (is_disabled_) return nullptr;

  starboard::ScopedLock scoped_lock(mutex_);

  int64_t current_time = SbTimeToPosix(SbTimeGetNow());
  SbTimeMonotonic current_monotonic_time = SbTimeGetMonotonicNow();

  // Creates new client.
  std::shared_ptr<Client> client = CreateClient(
      name, description, monitor_state, time_interval_microseconds,
      time_wait_microseconds, current_time, current_monotonic_time);
  if (client == nullptr) return nullptr;

  // Registers.
  client_list_.emplace_back(client);

  SB_DLOG(INFO) << "[Watchdog] Registered: " << name;
  return client;
}

std::unique_ptr<Client> Watchdog::CreateClient(
    std::string name, std::string description,
    base::ApplicationState monitor_state, int64_t time_interval_microseconds,
    int64_t time_wait_microseconds, int64_t current_time,
    SbTimeMonotonic current_monotonic_time) {
  // Validates parameters.
  if (time_interval_microseconds < watchdog_monitor_frequency_ ||
      time_wait_microseconds < 0) {
    SB_DLOG(ERROR) << "[Watchdog] Unable to Register: " << name;
    if (time_interval_microseconds < watchdog_monitor_frequency_) {
      SB_DLOG(ERROR) << "[Watchdog] Time interval less than min: "
                     << watchdog_monitor_frequency_;
    } else {
      SB_DLOG(ERROR) << "[Watchdog] Time wait is negative.";
    }
    return nullptr;
  }

  // Creates new Client.
  std::unique_ptr<Client> client(new Client);
  client->name = name;
  client->description = description;
  client->ping_infos = base::Value(base::Value::Type::LIST);
  client->monitor_state = monitor_state;
  client->time_interval_microseconds = time_interval_microseconds;
  client->time_wait_microseconds = time_wait_microseconds;
  client->time_registered_microseconds = current_time;
  client->time_registered_monotonic_microseconds = current_monotonic_time;
  client->time_last_pinged_microseconds = current_time;
  client->time_last_updated_monotonic_microseconds = current_monotonic_time;

  return std::move(client);
}

bool Watchdog::Unregister(const std::string& name, bool lock) {
  if (is_disabled_) return true;

  // Unregisters.
  if (lock) mutex_.Acquire();
  auto result = client_map_.erase(name);
  if (lock) mutex_.Release();

  if (result) {
    SB_DLOG(INFO) << "[Watchdog] Unregistered: " << name;
  } else {
    SB_DLOG(ERROR) << "[Watchdog] Unable to Unregister: " << name;
  }
  return result;
}

bool Watchdog::UnregisterByClient(std::shared_ptr<Client> client) {
  if (is_disabled_) return true;

  starboard::ScopedLock scoped_lock(mutex_);

  std::string name = "";
  if (client) name = client->name;

  // Unregisters.
  for (auto it = client_list_.begin(); it != client_list_.end(); it++) {
    if (client == *it) {
      client_list_.erase(it);
      SB_DLOG(INFO) << "[Watchdog] Unregistered: " << name;
      return true;
    }
  }
  SB_DLOG(ERROR) << "[Watchdog] Unable to Unregister: " << name;
  return false;
}

bool Watchdog::Ping(const std::string& name) { return Ping(name, ""); }

bool Watchdog::Ping(const std::string& name, const std::string& info) {
  if (is_disabled_) return true;

  starboard::ScopedLock scoped_lock(mutex_);

  auto it = client_map_.find(name);
  bool client_exists = it != client_map_.end();

  if (client_exists) {
    Client* client = it->second.get();
    return PingHelper(client, name, info);
  }
  SB_DLOG(ERROR) << "[Watchdog] Unable to Ping: " << name;
  return false;
}

bool Watchdog::PingByClient(std::shared_ptr<Client> client) {
  return PingByClient(client, "");
}

bool Watchdog::PingByClient(std::shared_ptr<Client> client,
                            const std::string& info) {
  if (is_disabled_) return true;

  std::string name = "";
  if (client) name = client->name;

  starboard::ScopedLock scoped_lock(mutex_);

  for (auto it = client_list_.begin(); it != client_list_.end(); it++) {
    if (client == *it) {
      return PingHelper(client.get(), name, info);
    }
  }
  SB_DLOG(ERROR) << "[Watchdog] Unable to Ping: " << name;
  return false;
}

bool Watchdog::PingHelper(Client* client, const std::string& name,
                          const std::string& info) {
  // Validates parameters.
  if (info.length() > kWatchdogMaxPingInfoLength) {
    SB_DLOG(ERROR) << "[Watchdog] Unable to Ping: " << name;
    SB_DLOG(ERROR) << "[Watchdog] Ping info length exceeds max: "
                   << kWatchdogMaxPingInfoLength;
    return false;
  }

  int64_t current_time = SbTimeToPosix(SbTimeGetNow());
  SbTimeMonotonic current_monotonic_time = SbTimeGetMonotonicNow();

  // Updates last ping.
  client->time_last_pinged_microseconds = current_time;
  client->time_last_updated_monotonic_microseconds = current_monotonic_time;

  if (info != "") {
    // Creates new ping_info.
    base::Value ping_info(base::Value::Type::DICT);
    ping_info.SetKey("timestampMilliseconds",
                     base::Value(std::to_string(current_time / 1000)));
    ping_info.SetKey("info", base::Value(info));

    client->ping_infos.GetList().Append(ping_info.Clone());
    if (client->ping_infos.GetList().size() > kWatchdogMaxPingInfos)
      client->ping_infos.GetList().erase(client->ping_infos.GetList().begin());
  }
  return true;
}

std::string Watchdog::GetWatchdogViolations(
    const std::vector<std::string>& clients, bool clear) {
  // Gets a json string containing the Watchdog violations of the given clients
  // since the last call (up to the kWatchdogMaxViolations limit).
  if (is_disabled_) return "";

  std::string fetched_violations_json = "";

  starboard::ScopedLock scoped_lock(mutex_);

  if (GetViolationsMap()->DictSize() != 0) {
    if (clients.empty()) {
      // Gets all Watchdog violations if no clients are given.
      base::JSONWriter::Write(*GetViolationsMap(), &fetched_violations_json);
      if (clear) {
        GetViolationsMap()->GetDict()->clear();
        starboard::SbFileDeleteRecursive(GetWatchdogFilePath().c_str(), true);
      }
    } else {
      // Gets all Watchdog violations of the given clients.
      base::Value fetched_violations(base::Value::Type::DICT);
      for (std::string name : clients) {
        base::Value* violation_dict = GetViolationsMap()->FindKey(name);
        if (violation_dict != nullptr) {
          fetched_violations.SetKey(name, (*violation_dict).Clone());
          if (clear) {
            GetViolationsMap()->RemoveKey(name);
            pending_write_ = true;
          }
        }
      }
      if (fetched_violations.DictSize() != 0) {
        base::JSONWriter::Write(fetched_violations, &fetched_violations_json);
      }
      if (clear) {
        EvictOldWatchdogViolations();
      }
    }
    SB_LOG(INFO) << "[Watchdog] Reading violations:\n"
                 << fetched_violations_json;
  } else {
    SB_LOG(INFO) << "[Watchdog] No violations.";
  }
  return fetched_violations_json;
}

void Watchdog::EvictOldWatchdogViolations() {
  int64_t current_timestamp_millis = SbTimeToPosix(SbTimeGetNow()) / 1000;
  int64_t cutoff_timestamp_millis =
      current_timestamp_millis - kWatchdogMaxViolationsAge;
  std::vector<std::string> empty_violations;

  // Iterates through map removing old violations.
  for (auto& map_it : GetViolationsMap()->DictItems()) {
    std::string name = map_it.first;
    base::Value& violation_dict = map_it.second;
    base::Value* violations = violation_dict.FindKey("violations");
    for (auto list_it = violations->GetList().begin();
         list_it != violations->GetList().end();) {
      int64_t violation_timestamp_millis = std::stoll(
          list_it->FindKey("timestampViolationMilliseconds")->GetString());

      if (violation_timestamp_millis < cutoff_timestamp_millis) {
        list_it = violations->GetList().erase(list_it);
        pending_write_ = true;
      } else {
        list_it++;
      }
    }
    if (violations->GetList().empty()) {
      empty_violations.push_back(name);
    }
  }

  // Removes empty violations.
  for (std::string name : empty_violations) {
    GetViolationsMap()->RemoveKey(name);
  }
  if (GetViolationsMap()->DictSize() == 0) {
    starboard::SbFileDeleteRecursive(GetWatchdogFilePath().c_str(), true);
    pending_write_ = false;
  }
}

bool Watchdog::GetPersistentSettingWatchdogEnable() {
  // Watchdog stub
  if (!persistent_settings_) return kDefaultSettingWatchdogEnable;

  // Gets the boolean that controls whether or not Watchdog is enabled.
  return persistent_settings_->GetPersistentSettingAsBool(
      kPersistentSettingWatchdogEnable, kDefaultSettingWatchdogEnable);
}

void Watchdog::SetPersistentSettingWatchdogEnable(bool enable_watchdog) {
  // Watchdog stub
  if (!persistent_settings_) return;

  // Sets the boolean that controls whether or not Watchdog is enabled.
  persistent_settings_->SetPersistentSetting(
      kPersistentSettingWatchdogEnable,
      std::make_unique<base::Value>(enable_watchdog));
}

bool Watchdog::GetPersistentSettingWatchdogCrash() {
  // Watchdog stub
  if (!persistent_settings_) return kDefaultSettingWatchdogCrash;

  // Gets the boolean that controls whether or not crashes can be triggered.
  return persistent_settings_->GetPersistentSettingAsBool(
      kPersistentSettingWatchdogCrash, kDefaultSettingWatchdogCrash);
}

void Watchdog::SetPersistentSettingWatchdogCrash(bool can_trigger_crash) {
  // Watchdog stub
  if (!persistent_settings_) return;

  // Sets the boolean that controls whether or not crashes can be triggered.
  persistent_settings_->SetPersistentSetting(
      kPersistentSettingWatchdogCrash,
      std::make_unique<base::Value>(can_trigger_crash));
}

#if defined(_DEBUG)
// Sleeps threads for Watchdog debugging.
void Watchdog::MaybeInjectDebugDelay(const std::string& name) {
  if (is_disabled_) return;

  starboard::ScopedLock scoped_lock(delay_mutex_);

  if (name != delay_name_) return;

  if (SbTimeGetMonotonicNow() >
      time_last_delayed_microseconds_ + delay_wait_time_microseconds_) {
    SbThreadSleep(delay_sleep_time_microseconds_);
    time_last_delayed_microseconds_ = SbTimeGetMonotonicNow();
  }
}
#endif  // defined(_DEBUG)

}  // namespace watchdog
}  // namespace cobalt
