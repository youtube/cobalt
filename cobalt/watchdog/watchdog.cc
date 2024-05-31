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

#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/time/time.h"
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
const int kWatchdogMaxPingInfos = 60;
// The maximum length of each ping info.
const int kWatchdogMaxPingInfoLength = 1024;
// The maximum number of milliseconds old of an unfetched Watchdog violation.
const int64_t kWatchdogMaxViolationsAge = 86400000;

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
  is_logtrace_disabled_ = !GetPersistentSettingLogtraceEnable();

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

  int res =
      pthread_create(&watchdog_thread_, nullptr, &Watchdog::Monitor, this);
  SB_DCHECK(res == 0);
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
  pthread_join(watchdog_thread_, nullptr);
}

std::shared_ptr<base::Value> Watchdog::GetViolationsMap() {
  // Gets the Watchdog violations map with lazy initialization which loads the
  // previous Watchdog violations file containing violations before app start,
  // if it exists.
  if (violations_map_ == nullptr) {
    base::FilePath file_path(GetWatchdogFilePath());
    base::File read_file(
        file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
    if (read_file.IsValid()) {
      int64_t kFileSize = read_file.GetLength();
      std::vector<char> buffer(kFileSize + 1, 0);
      read_file.ReadAtCurrentPos(buffer.data(), kFileSize);
      violations_map_ = base::Value::ToUniquePtrValue(
          base::JSONReader::Read(std::string(buffer.data()))
              .value_or(base::Value(base::Value::Type::DICT)));
    }

    if (violations_map_ == nullptr) {
      LOG(INFO) << "[Watchdog] No previous violations JSON.";
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
    LOG(INFO) << "[Watchdog] Violations filepath: " << watchdog_file_path_;
  }
  return watchdog_file_path_;
}

std::vector<std::string> Watchdog::GetWatchdogViolationClientNames() {
  std::vector<std::string> names;

  if (is_disabled_) return names;

  base::AutoLock scoped_lock(mutex_);
  auto violation_map = GetViolationsMap()->GetDict().Clone();
  for (base::Value::Dict::iterator it = violation_map.begin();
       it != violation_map.end(); ++it) {
    names.push_back(it->first);
  }
  return names;
}

void Watchdog::UpdateState(base::ApplicationState state) {
  if (is_disabled_) return;

  base::AutoLock scoped_lock(mutex_);
  state_ = state;
}

void Watchdog::WriteWatchdogViolations() {
  // Writes Watchdog violations to persistent storage as a json file.
  std::string watchdog_json;
  base::JSONWriter::Write(*GetViolationsMap(), &watchdog_json);
  LOG(INFO) << "[Watchdog] Writing violations to JSON:\n" << watchdog_json;
  starboard::ScopedFile watchdog_file(GetWatchdogFilePath().c_str(),
                                      kSbFileCreateAlways | kSbFileWrite);
  watchdog_file.WriteAll(watchdog_json.c_str(),
                         static_cast<int>(watchdog_json.size()));
  pending_write_ = false;
  time_last_written_microseconds_ =
      (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
}

void* Watchdog::Monitor(void* context) {
  pthread_setname_np(pthread_self(), "Watchdog");
  base::AutoLock scoped_lock(static_cast<Watchdog*>(context)->mutex_);
  while (1) {
    int64_t current_monotonic_time =
        (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
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
    base::TimeDelta timeout = base::Microseconds(
        static_cast<Watchdog*>(context)->watchdog_monitor_frequency_);
    static_cast<Watchdog*>(context)->monitor_wait_.TimedWait(timeout);

    // Shutdown
    if (!(static_cast<Watchdog*>(context)->is_monitoring_.load())) break;
  }
  return nullptr;
}

bool Watchdog::MonitorClient(void* context, Client* client,
                             int64_t current_monotonic_time) {
  // Ignores and resets clients in idle states, clients whose monitor_state
  // is below the current application state. Resets time_wait_microseconds
  // and time_interval_microseconds start values.
  if (static_cast<Watchdog*>(context)->state_ > client->monitor_state) {
    client->time_registered_monotonic_microseconds = current_monotonic_time;
    client->time_last_updated_monotonic_microseconds = current_monotonic_time;
    return false;
  }

  int64_t time_delta =
      current_monotonic_time - client->time_last_updated_monotonic_microseconds;
  int64_t time_wait =
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
                                   int64_t time_delta) {
  // Gets violation dictionary with key client name from violations map.
  Watchdog* watchdog_instance = static_cast<Watchdog*>(context);

  auto violation_dict =
      (watchdog_instance->GetViolationsMap())->GetDict().FindDict(client->name);

  // Checks if new unique violation.
  bool new_violation = false;
  if (violation_dict == nullptr) {
    new_violation = true;
  } else {
    // Compares against last_pinged_timestamp_microsecond of last violation.
    base::Value::List* violations = violation_dict->FindList("violations");
    if (violations) {
      int last_index = violations->size() - 1;
      std::string* timestamp_last_pinged_milliseconds =
          (*violations)[last_index].GetDict().FindString(
              "timestampLastPingedMilliseconds");
      if (timestamp_last_pinged_milliseconds &&
          *timestamp_last_pinged_milliseconds !=
              std::to_string(client->time_last_pinged_microseconds / 1000))
        new_violation = true;
    }
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
    int64_t current_timestamp_millis =
        (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();
    violation.SetKey("timestampViolationMilliseconds",
                     base::Value(std::to_string(current_timestamp_millis)));
    violation.SetKey(
        "violationDurationMilliseconds",
        base::Value(std::to_string(
            (time_delta - client->time_interval_microseconds) / 1000)));
    base::Value registered_clients(base::Value::Type::LIST);
    for (auto& it : watchdog_instance->client_map_) {
      registered_clients.GetList().Append(base::Value(it.first));
    }
    for (auto& it : watchdog_instance->client_list_) {
      registered_clients.GetList().Append(base::Value(it->name));
    }
    violation.SetKey("registeredClients", registered_clients.Clone());

    violation.SetKey(
        "logTrace",
        watchdog_instance->instrumentation_log_.GetLogTraceAsValue());

    // Adds new violation to violations map.
    if (violation_dict == nullptr) {
      base::Value dict(base::Value::Type::DICT);
      dict.SetKey("description", base::Value(client->description));
      base::Value list(base::Value::Type::LIST);
      list.GetList().Append(violation.Clone());
      dict.SetKey("violations", list.Clone());
      (watchdog_instance->GetViolationsMap())
          ->SetKey(client->name, dict.Clone());
    } else {
      base::Value::List* violations = violation_dict->FindList("violations");
      if (violations) {
        violations->Append(violation.Clone());
      }
    }
  } else {
    // Consecutive non-unique violation, updates violation in violations map.
    base::Value::List* violations = violation_dict->FindList("violations");
    if (violations) {
      int last_index = violations->size() - 1;
      std::string* timestamp_duration_milliseconds =
          (*violations)[last_index].GetDict().FindString(
              "violationDurationMilliseconds");
      if (timestamp_duration_milliseconds) {
        int64_t violation_duration =
            std::stoll(*timestamp_duration_milliseconds);
        (*violations)[last_index].SetKey(
            "violationDurationMilliseconds",
            base::Value(
                std::to_string(violation_duration + (time_delta / 1000))));
      }
    }
  }
  watchdog_instance->pending_write_ = true;

  int violations_count = 0;
  auto violation_map =
      (static_cast<Watchdog*>(context)->GetViolationsMap())->GetDict().Clone();
  for (base::Value::Dict::iterator it = violation_map.begin();
       it != violation_map.end(); ++it) {
    base::Value& violation_dict = it->second;
    base::Value::List* violations =
        violation_dict.GetDict().FindList("violations");
    if (violations) {
      violations_count += violations->size();
    }
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
  auto violation_map =
      (static_cast<Watchdog*>(context)->GetViolationsMap())->GetDict().Clone();
  for (base::Value::Dict::iterator it = violation_map.begin();
       it != violation_map.end(); ++it) {
    std::string name = it->first;
    base::Value& violation_dict = it->second;
    base::Value::List* violations =
        violation_dict.GetDict().FindList("violations");
    if (violations) {
      int count = violations->size();
      std::string* timestamp_duration_milliseconds =
          (*violations)[0].GetDict().FindString(
              "violationDurationMilliseconds");
      if (timestamp_duration_milliseconds) {
        int64_t violation_timestamp_millis =
            std::stoll(*timestamp_duration_milliseconds);

        if ((evicted_name == "") || (count > evicted_count) ||
            ((count == evicted_count) &&
             (violation_timestamp_millis < evicted_timestamp_millis))) {
          evicted_name = name;
          evicted_count = count;
          evicted_timestamp_millis = violation_timestamp_millis;
        }
      }
    }
  }

  auto violation_dict = (static_cast<Watchdog*>(context)->GetViolationsMap())
                            ->GetDict()
                            .FindDict(evicted_name);

  if (violation_dict != nullptr) {
    base::Value::List* violations = violation_dict->FindList("violations");
    if (violations) {
      violations->erase(violations->begin());
      static_cast<Watchdog*>(context)->pending_write_ = true;

      // Removes empty violations.
      if (violations->empty()) {
        (static_cast<Watchdog*>(context)->GetViolationsMap())
            ->RemoveKey(evicted_name);
      }
      if (static_cast<Watchdog*>(context)->GetViolationsMap()->DictSize() ==
          0) {
        starboard::SbFileDeleteRecursive(
            static_cast<Watchdog*>(context)->GetWatchdogFilePath().c_str(),
            true);
        static_cast<Watchdog*>(context)->pending_write_ = false;
      }
    }
  }
}

void Watchdog::MaybeWriteWatchdogViolations(void* context) {
  if ((base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds() >
      static_cast<Watchdog*>(context)->time_last_written_microseconds_ +
          static_cast<Watchdog*>(context)->write_wait_time_microseconds_) {
    static_cast<Watchdog*>(context)->WriteWatchdogViolations();
  }
}

void Watchdog::MaybeTriggerCrash(void* context) {
  if (static_cast<Watchdog*>(context)->GetPersistentSettingWatchdogCrash()) {
    if (static_cast<Watchdog*>(context)->pending_write_)
      static_cast<Watchdog*>(context)->WriteWatchdogViolations();
    LOG(ERROR) << "[Watchdog] Triggering violation Crash!";
    *(reinterpret_cast<volatile char*>(0)) = 0;
  }
}

bool Watchdog::Register(std::string name, std::string description,
                        base::ApplicationState monitor_state,
                        int64_t time_interval_microseconds,
                        int64_t time_wait_microseconds, Replace replace) {
  if (is_disabled_) return true;

  base::AutoLock scoped_lock(mutex_);

  int64_t current_time =
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  int64_t current_monotonic_time =
      (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();

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
    DLOG(INFO) << "[Watchdog] Registered: " << name;
  } else {
    DLOG(ERROR) << "[Watchdog] Unable to Register: " << name;
  }
  return result.second;
}

std::shared_ptr<Client> Watchdog::RegisterByClient(
    std::string name, std::string description,
    base::ApplicationState monitor_state, int64_t time_interval_microseconds,
    int64_t time_wait_microseconds) {
  if (is_disabled_) return nullptr;

  base::AutoLock scoped_lock(mutex_);

  int64_t current_time =
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  int64_t current_monotonic_time =
      (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();

  // Creates new client.
  std::shared_ptr<Client> client = CreateClient(
      name, description, monitor_state, time_interval_microseconds,
      time_wait_microseconds, current_time, current_monotonic_time);
  if (client == nullptr) return nullptr;

  // Registers.
  client_list_.emplace_back(client);

  DLOG(INFO) << "[Watchdog] Registered: " << name;
  return client;
}

std::unique_ptr<Client> Watchdog::CreateClient(
    std::string name, std::string description,
    base::ApplicationState monitor_state, int64_t time_interval_microseconds,
    int64_t time_wait_microseconds, int64_t current_time,
    int64_t current_monotonic_time) {
  // Validates parameters.
  if (time_interval_microseconds < watchdog_monitor_frequency_ ||
      time_wait_microseconds < 0) {
    DLOG(ERROR) << "[Watchdog] Unable to Register: " << name;
    if (time_interval_microseconds < watchdog_monitor_frequency_) {
      DLOG(ERROR) << "[Watchdog] Time interval less than min: "
                  << watchdog_monitor_frequency_;
    } else {
      DLOG(ERROR) << "[Watchdog] Time wait is negative.";
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
  int result = 0;
  if (lock) {
    base::AutoLock lock(mutex_);
    result = client_map_.erase(name);
  } else {
    result = client_map_.erase(name);
  }

  if (result) {
    DLOG(INFO) << "[Watchdog] Unregistered: " << name;
  } else {
    DLOG(ERROR) << "[Watchdog] Unable to Unregister: " << name;
  }
  return result;
}

bool Watchdog::UnregisterByClient(std::shared_ptr<Client> client) {
  if (is_disabled_) return true;

  base::AutoLock scoped_lock(mutex_);

  std::string name = "";
  if (client) name = client->name;

  // Unregisters.
  for (auto it = client_list_.begin(); it != client_list_.end(); it++) {
    if (client == *it) {
      client_list_.erase(it);
      DLOG(INFO) << "[Watchdog] Unregistered: " << name;
      return true;
    }
  }
  DLOG(ERROR) << "[Watchdog] Unable to Unregister: " << name;
  return false;
}

bool Watchdog::Ping(const std::string& name) { return Ping(name, ""); }

bool Watchdog::Ping(const std::string& name, const std::string& info) {
  if (is_disabled_) return true;

  base::AutoLock scoped_lock(mutex_);

  auto it = client_map_.find(name);
  bool client_exists = it != client_map_.end();

  if (client_exists) {
    Client* client = it->second.get();
    return PingHelper(client, name, info);
  }
  DLOG(ERROR) << "[Watchdog] Unable to Ping: " << name;
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

  base::AutoLock scoped_lock(mutex_);

  for (auto it = client_list_.begin(); it != client_list_.end(); it++) {
    if (client == *it) {
      return PingHelper(client.get(), name, info);
    }
  }
  DLOG(ERROR) << "[Watchdog] Unable to Ping: " << name;
  return false;
}

bool Watchdog::PingHelper(Client* client, const std::string& name,
                          const std::string& info) {
  // Validates parameters.
  if (info.length() > kWatchdogMaxPingInfoLength) {
    DLOG(ERROR) << "[Watchdog] Unable to Ping: " << name;
    DLOG(ERROR) << "[Watchdog] Ping info length exceeds max: "
                << kWatchdogMaxPingInfoLength;
    return false;
  }

  int64_t current_time =
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  int64_t current_monotonic_time =
      (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();

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

  base::AutoLock scoped_lock(mutex_);

  if (GetViolationsMap()->DictSize() != 0) {
    if (clients.empty()) {
      // Gets all Watchdog violations if no clients are given.
      base::JSONWriter::Write(*GetViolationsMap(), &fetched_violations_json);
      if (clear) {
        GetViolationsMap()->GetDict().clear();
        starboard::SbFileDeleteRecursive(GetWatchdogFilePath().c_str(), true);
      }
    } else {
      // Gets all Watchdog violations of the given clients.
      base::Value::Dict fetched_violations;
      for (std::string name : clients) {
        auto violation_dict = GetViolationsMap()->GetDict().FindDict(name);
        if (violation_dict != nullptr) {
          fetched_violations.Set(name, (*violation_dict).Clone());
          if (clear) {
            GetViolationsMap()->RemoveKey(name);
            pending_write_ = true;
          }
        }
      }
      if (fetched_violations.size() != 0) {
        base::JSONWriter::Write(fetched_violations, &fetched_violations_json);
      }
      if (clear) {
        EvictOldWatchdogViolations();
      }
    }
    LOG(INFO) << "[Watchdog] Reading violations:\n" << fetched_violations_json;
  } else {
    LOG(INFO) << "[Watchdog] No violations.";
  }
  return fetched_violations_json;
}

void Watchdog::EvictOldWatchdogViolations() {
  int64_t current_timestamp_millis =
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();
  int64_t cutoff_timestamp_millis =
      current_timestamp_millis - kWatchdogMaxViolationsAge;
  std::vector<std::string> empty_violations;

  // Iterates through map removing old violations.
  auto violation_map = GetViolationsMap()->GetDict().Clone();
  for (base::Value::Dict::iterator map_it = violation_map.begin();
       map_it != violation_map.end(); ++map_it) {
    std::string name = map_it->first;
    base::Value& violation_dict = map_it->second;
    base::Value::List* violations =
        violation_dict.GetDict().FindList("violations");
    if (violations) {
      for (auto list_it = violations->begin(); list_it != violations->end();) {
        int64_t violation_timestamp_millis = std::stoll(
            *list_it->GetDict().FindString("timestampViolationMilliseconds"));

        if (violation_timestamp_millis < cutoff_timestamp_millis) {
          list_it = violations->erase(list_it);
          pending_write_ = true;
        } else {
          list_it++;
        }
      }
      if (violations->empty()) {
        empty_violations.push_back(name);
      }
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
  base::Value value;
  persistent_settings_->Get(kPersistentSettingWatchdogEnable, &value);
  return value.GetIfBool().value_or(kDefaultSettingWatchdogEnable);
}

void Watchdog::SetPersistentSettingWatchdogEnable(bool enable_watchdog) {
  // Watchdog stub
  if (!persistent_settings_) return;

  // Sets the boolean that controls whether or not Watchdog is enabled.
  persistent_settings_->Set(kPersistentSettingWatchdogEnable,
                            base::Value(enable_watchdog));
}

bool Watchdog::GetPersistentSettingWatchdogCrash() {
  // Watchdog stub
  if (!persistent_settings_) return kDefaultSettingWatchdogCrash;

  // Gets the boolean that controls whether or not crashes can be triggered.
  base::Value value;
  persistent_settings_->Get(kPersistentSettingWatchdogCrash, &value);
  return value.GetIfBool().value_or(kDefaultSettingWatchdogCrash);
}

void Watchdog::SetPersistentSettingWatchdogCrash(bool can_trigger_crash) {
  // Watchdog stub
  if (!persistent_settings_) return;

  // Sets the boolean that controls whether or not crashes can be triggered.
  persistent_settings_->Set(kPersistentSettingWatchdogCrash,
                            base::Value(can_trigger_crash));
}

bool Watchdog::LogEvent(const std::string& event) {
  if (is_logtrace_disabled_) {
    return true;
  }

  return instrumentation_log_.LogEvent(event);
}

std::vector<std::string> Watchdog::GetLogTrace() {
  if (is_logtrace_disabled_) {
    return {};
  }

  return instrumentation_log_.GetLogTrace();
}

void Watchdog::ClearLog() {
  if (is_logtrace_disabled_) {
    return;
  }

  instrumentation_log_.ClearLog();
}

bool Watchdog::GetPersistentSettingLogtraceEnable() {
  if (!persistent_settings_) return kDefaultSettingLogtraceEnable;

  // Gets the boolean that controls whether or not LogTrace is enabled.
  base::Value value;
  persistent_settings_->Get(kPersistentSettingLogtraceEnable, &value);
  return value.GetIfBool().value_or(kDefaultSettingLogtraceEnable);
}

void Watchdog::SetPersistentSettingLogtraceEnable(bool enable_logtrace) {
  if (!persistent_settings_) return;

  // Sets the boolean that controls whether or not LogTrace is enabled.
  persistent_settings_->Set(kPersistentSettingLogtraceEnable,
                            base::Value(enable_logtrace));
}

#if defined(_DEBUG)
// Sleeps threads for Watchdog debugging.
void Watchdog::MaybeInjectDebugDelay(const std::string& name) {
  if (is_disabled_) return;

  base::AutoLock scoped_lock(delay_mutex_);

  if (name != delay_name_) return;

  if ((base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds() >
      time_last_delayed_microseconds_ + delay_wait_time_microseconds_) {
    usleep(delay_sleep_time_microseconds_);
    time_last_delayed_microseconds_ =
        (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
  }
}
#endif  // defined(_DEBUG)

}  // namespace watchdog
}  // namespace cobalt
