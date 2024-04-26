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

#include <set>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/task_environment.h"
#include "starboard/common/file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace watchdog {

using persistent_storage::PersistentSettings;

namespace {

const char kWatchdogViolationsJson[] = "watchdog_test.json";
const int64_t kWatchdogMonitorFrequency = 100000;
const int64_t kWatchdogSleepDuration = kWatchdogMonitorFrequency * 4;

}  // namespace

class WatchdogTest : public testing::Test {
 protected:
  WatchdogTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT) {}

  void SetUp() final {
    watchdog_ = new watchdog::Watchdog();
    watchdog_->InitializeCustom(nullptr, std::string(kWatchdogViolationsJson),
                                kWatchdogMonitorFrequency);
    watchdog_->GetWatchdogViolations();
  }

  void TearDown() final {
    watchdog_->Uninitialize();
    delete watchdog_;
    watchdog_ = nullptr;

    DeletePersistentSettingsFile();
  }

  base::Value CreateDummyViolationDict(std::string desc, int begin, int end) {
    base::Value violation_dict(base::Value::Type::DICT);
    violation_dict.SetKey("description", base::Value(desc));
    base::Value list(base::Value::Type::LIST);
    for (int i = begin; i < end; i++)
      list.GetList().Append(CreateDummyViolation(i));
    violation_dict.SetKey("violations", list.Clone());
    return violation_dict.Clone();
  }

  base::Value CreateDummyViolation(int timestamp_violation) {
    base::Value violation(base::Value::Type::DICT);
    base::Value ping_infos(base::Value::Type::LIST);
    violation.SetKey("pingInfos", ping_infos.Clone());
    violation.SetKey("monitorState",
                     base::Value(std::string(GetApplicationStateString(
                         base::kApplicationStateStarted))));
    violation.SetKey("timeIntervalMilliseconds", base::Value("0"));
    violation.SetKey("timeWaitMilliseconds", base::Value("0"));
    violation.SetKey("timestampRegisteredMilliseconds", base::Value("0"));
    violation.SetKey("timestampLastPingedMilliseconds", base::Value("0"));
    violation.SetKey("timestampViolationMilliseconds",
                     base::Value(std::to_string(timestamp_violation)));
    violation.SetKey("violationDurationMilliseconds", base::Value("0"));
    base::Value registered_clients(base::Value::Type::LIST);
    violation.SetKey("registeredClients", registered_clients.Clone());
    return violation.Clone();
  }

  void DeletePersistentSettingsFile() {
    std::vector<char> storage_dir(kSbFileMaxPath + 1, 0);
    SbSystemGetPath(kSbSystemPathCacheDirectory, storage_dir.data(),
                    kSbFileMaxPath);
    std::string path =
        std::string(storage_dir.data()) + kSbFileSepString + kSettingsFileName;

    starboard::SbFileDeleteRecursive(path.c_str(), true);
  }

  const std::string kSettingsFileName = "test-settings.json";

  watchdog::Watchdog* watchdog_;
  base::test::TaskEnvironment task_environment_;
  base::WaitableEvent task_done_ = {
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
};

TEST_F(WatchdogTest, RedundantRegistersShouldFail) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_FALSE(watchdog_->Register("test-name", "test-desc",
                                   base::kApplicationStateStarted,
                                   kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, PING));
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, ALL));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, RegisterOnlyAcceptsValidParameters) {
  ASSERT_FALSE(watchdog_->Register("test-name-1", "test-desc-1",
                                   base::kApplicationStateStarted, 1, 0));
  ASSERT_FALSE(watchdog_->Unregister("test-name-1"));
  ASSERT_FALSE(watchdog_->Register("test-name-2", "test-desc-2",
                                   base::kApplicationStateStarted, 0, 0));
  ASSERT_FALSE(watchdog_->Unregister("test-name-2"));
  ASSERT_FALSE(watchdog_->Register("test-name-3", "test-desc-3",
                                   base::kApplicationStateStarted, -1, 0));
  ASSERT_FALSE(watchdog_->Unregister("test-name-3"));
  ASSERT_TRUE(watchdog_->Register("test-name-4", "test-desc-4",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 1));
  ASSERT_TRUE(watchdog_->Unregister("test-name-4"));
  ASSERT_TRUE(watchdog_->Register("test-name-5", "test-desc-5",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0));
  ASSERT_TRUE(watchdog_->Unregister("test-name-5"));
  ASSERT_FALSE(watchdog_->Register("test-name-6", "test-desc-6",
                                   base::kApplicationStateStarted,
                                   kWatchdogMonitorFrequency, -1));
  ASSERT_FALSE(watchdog_->Unregister("test-name-6"));
}

TEST_F(WatchdogTest, RegisterByClientOnlyAcceptsValidParameters) {
  std::shared_ptr<Client> client = watchdog_->RegisterByClient(
      "test-name", "test-desc", base::kApplicationStateStarted, 1, 0);
  ASSERT_EQ(client, nullptr);
  ASSERT_FALSE(watchdog_->UnregisterByClient(client));
  client = watchdog_->RegisterByClient("test-name", "test-desc",
                                       base::kApplicationStateStarted, 0, 0);
  ASSERT_EQ(client, nullptr);
  ASSERT_FALSE(watchdog_->UnregisterByClient(client));
  client = watchdog_->RegisterByClient("test-name", "test-desc",
                                       base::kApplicationStateStarted, -1, 0);
  ASSERT_EQ(client, nullptr);
  ASSERT_FALSE(watchdog_->UnregisterByClient(client));
  client = watchdog_->RegisterByClient("test-name", "test-desc",
                                       base::kApplicationStateStarted,
                                       kWatchdogMonitorFrequency, 1);
  ASSERT_NE(client, nullptr);
  ASSERT_TRUE(watchdog_->UnregisterByClient(client));
  client = watchdog_->RegisterByClient("test-name", "test-desc",
                                       base::kApplicationStateStarted,
                                       kWatchdogMonitorFrequency, 0);
  ASSERT_NE(client, nullptr);
  ASSERT_TRUE(watchdog_->UnregisterByClient(client));
  client = watchdog_->RegisterByClient("test-name", "test-desc",
                                       base::kApplicationStateStarted,
                                       kWatchdogMonitorFrequency, -1);
  ASSERT_EQ(client, nullptr);
  ASSERT_FALSE(watchdog_->UnregisterByClient(client));
}

TEST_F(WatchdogTest, UnmatchedUnregisterShouldFail) {
  ASSERT_FALSE(watchdog_->Unregister("test-name"));
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  ASSERT_FALSE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, UnmatchedUnregisterByClientShouldFail) {
  std::shared_ptr<Client> client_test(new Client);
  ASSERT_FALSE(watchdog_->UnregisterByClient(client_test));
  std::shared_ptr<Client> client = watchdog_->RegisterByClient(
      "test-name", "test-desc", base::kApplicationStateStarted,
      kWatchdogMonitorFrequency);
  ASSERT_NE(client, nullptr);
  ASSERT_TRUE(watchdog_->UnregisterByClient(client));
  ASSERT_FALSE(watchdog_->UnregisterByClient(client));
}

TEST_F(WatchdogTest, UnmatchedPingShouldFail) {
  ASSERT_FALSE(watchdog_->Ping("test-name"));
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Ping("test-name"));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  ASSERT_FALSE(watchdog_->Ping("test-name"));
}

TEST_F(WatchdogTest, UnmatchedPingByClientShouldFail) {
  std::shared_ptr<Client> client_test(new Client);
  ASSERT_FALSE(watchdog_->PingByClient(client_test));
  std::shared_ptr<Client> client = watchdog_->RegisterByClient(
      "test-name", "test-desc", base::kApplicationStateStarted,
      kWatchdogMonitorFrequency);
  ASSERT_NE(client, nullptr);
  ASSERT_TRUE(watchdog_->PingByClient(client));
  ASSERT_TRUE(watchdog_->UnregisterByClient(client));
  ASSERT_FALSE(watchdog_->PingByClient(client));
}

TEST_F(WatchdogTest, PingOnlyAcceptsValidParameters) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Ping("test-name", "42"));
  ASSERT_FALSE(watchdog_->Ping("test-name", std::string(1025, 'x')));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, PingByClientOnlyAcceptsValidParameters) {
  std::shared_ptr<Client> client = watchdog_->RegisterByClient(
      "test-name", "test-desc", base::kApplicationStateStarted,
      kWatchdogMonitorFrequency);
  ASSERT_NE(client, nullptr);
  ASSERT_TRUE(watchdog_->PingByClient(client, "42"));
  ASSERT_FALSE(watchdog_->PingByClient(client, std::string(1025, 'x')));
  ASSERT_TRUE(watchdog_->UnregisterByClient(client));
}

TEST_F(WatchdogTest, ViolationsJsonShouldPersistAndBeValid) {
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Ping("test-name", "test-ping"));
  usleep(kWatchdogSleepDuration);
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  TearDown();
  watchdog_ = new watchdog::Watchdog();
  watchdog_->InitializeCustom(nullptr, std::string(kWatchdogViolationsJson),
                              kWatchdogMonitorFrequency);

  // Validates Violation json file.
  std::string json = watchdog_->GetWatchdogViolations();
  ASSERT_NE(json, "");
  absl::optional<base::Value> violations_map_optional =
      base::JSONReader::Read(json);
  ASSERT_TRUE(violations_map_optional.has_value());
  std::unique_ptr<base::Value::Dict> violations_map =
      std::make_unique<base::Value::Dict>(
          std::move(*violations_map_optional.value().GetIfDict()));
  ASSERT_NE(violations_map, nullptr);
  base::Value::Dict* violation_dict = violations_map->FindDict("test-name");
  ASSERT_NE(violation_dict, nullptr);
  std::string* description = violation_dict->FindString("description");
  ASSERT_NE(description, nullptr);
  ASSERT_EQ(*description, "test-desc");
  base::Value::List* violations = violation_dict->FindList("violations");
  ASSERT_NE(violations, nullptr);
  ASSERT_EQ(violations->size(), 1);
  std::string* monitor_state =
      (*violations)[0].GetDict().FindString("monitorState");
  ASSERT_NE(monitor_state, nullptr);
  ASSERT_EQ(
      *monitor_state,
      std::string(GetApplicationStateString(base::kApplicationStateStarted)));
  base::Value::List* ping_infos =
      (*violations)[0].GetDict().FindList("pingInfos");
  ASSERT_NE(ping_infos, nullptr);
  ASSERT_EQ(ping_infos->size(), 1);
  std::string* info = (*ping_infos)[0].GetDict().FindString("info");
  ASSERT_NE(info, nullptr);
  ASSERT_EQ(*info, "test-ping");
  std::string* timestamp_milliseconds =
      (*ping_infos)[0].GetDict().FindString("timestampMilliseconds");
  ASSERT_NE(timestamp_milliseconds, nullptr);
  std::stoll(*timestamp_milliseconds);
  base::Value::List* registered_clients =
      (*violations)[0].GetDict().FindList("registeredClients");
  ASSERT_NE(registered_clients, nullptr);
  ASSERT_EQ(registered_clients->size(), 1);
  ASSERT_EQ((*registered_clients)[0].GetString(), "test-name");
  std::string* time_interval_milliseconds =
      (*violations)[0].GetDict().FindString("timeIntervalMilliseconds");
  ASSERT_NE(time_interval_milliseconds, nullptr);
  std::stoll(*time_interval_milliseconds);
  std::string* time_wait_milliseconds =
      (*violations)[0].GetDict().FindString("timeWaitMilliseconds");
  ASSERT_NE(time_wait_milliseconds, nullptr);
  std::stoll(*time_wait_milliseconds);
  std::string* timestamp_last_pinged_milliseconds =
      (*violations)[0].GetDict().FindString("timestampLastPingedMilliseconds");
  ASSERT_NE(timestamp_last_pinged_milliseconds, nullptr);
  std::stoll(*timestamp_last_pinged_milliseconds);
  std::string* timestamp_registered_milliseconds =
      (*violations)[0].GetDict().FindString("timestampRegisteredMilliseconds");
  ASSERT_NE(timestamp_registered_milliseconds, nullptr);
  std::stoll(*timestamp_registered_milliseconds);
  std::string* timestamp_violation_milliseconds =
      (*violations)[0].GetDict().FindString("timestampViolationMilliseconds");
  ASSERT_NE(timestamp_violation_milliseconds, nullptr);
  std::stoll(*timestamp_violation_milliseconds);
  std::string* violation_duration_milliseconds =
      (*violations)[0].GetDict().FindString("violationDurationMilliseconds");
  ASSERT_NE(violation_duration_milliseconds, nullptr);
  std::stoll(*violation_duration_milliseconds);
}

TEST_F(WatchdogTest, RedundantViolationsShouldStack) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  usleep(kWatchdogSleepDuration);
  std::string json = watchdog_->GetWatchdogViolations({}, false);
  ASSERT_NE(json, "");
  absl::optional<base::Value> uncleared_violations_map_optional =
      base::JSONReader::Read(json);
  base::Value::Dict uncleared_violations_map =
      uncleared_violations_map_optional.value().GetDict().Clone();
  ASSERT_NE(uncleared_violations_map.size(), 0);
  base::Value::Dict* violation_dict =
      uncleared_violations_map.FindDict("test-name");
  base::Value::List* violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 1);
  std::string uncleared_timestamp =
      *(*violations)[0].GetDict().FindString("timestampLastPingedMilliseconds");
  int64_t uncleared_duration = std::stoll(
      *(*violations)[0].GetDict().FindString("violationDurationMilliseconds"));
  usleep(kWatchdogSleepDuration);
  json = watchdog_->GetWatchdogViolations({}, false);
  ASSERT_NE(json, "");
  absl::optional<base::Value> violations_map_optional =
      base::JSONReader::Read(json);
  base::Value::Dict violations_map =
      violations_map_optional.value().GetDict().Clone();
  ASSERT_NE(violations_map.size(), 0);
  violation_dict = violations_map.FindDict("test-name");
  violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 1);
  std::string timestamp =
      *(*violations)[0].GetDict().FindString("timestampLastPingedMilliseconds");
  int64_t duration = std::stoll(
      *(*violations)[0].GetDict().FindString("violationDurationMilliseconds"));
  ASSERT_EQ(uncleared_timestamp, timestamp);
  ASSERT_LT(uncleared_duration, duration);
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, ViolationsShouldResetAfterFetch) {
  ASSERT_TRUE(watchdog_->Register("test-name-1", "test-desc-1",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  usleep(kWatchdogSleepDuration);
  ASSERT_TRUE(watchdog_->Unregister("test-name-1"));
  std::string json = watchdog_->GetWatchdogViolations();
  ASSERT_NE(json.find("test-name-1"), std::string::npos);
  ASSERT_EQ(json.find("test-name-2"), std::string::npos);
  std::shared_ptr<Client> client = watchdog_->RegisterByClient(
      "test-name-2", "test-desc-2", base::kApplicationStateStarted,
      kWatchdogMonitorFrequency);
  ASSERT_NE(client, nullptr);
  usleep(kWatchdogSleepDuration);
  ASSERT_TRUE(watchdog_->UnregisterByClient(client));
  json = watchdog_->GetWatchdogViolations();
  ASSERT_EQ(json.find("test-name-1"), std::string::npos);
  ASSERT_NE(json.find("test-name-2"), std::string::npos);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
}

TEST_F(WatchdogTest, PingInfosAreEvictedAfterMax) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test_desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  for (int i = 0; i < 61; i++) {
    ASSERT_TRUE(watchdog_->Ping("test-name", std::to_string(i)));
  }
  usleep(kWatchdogSleepDuration);
  std::string json = watchdog_->GetWatchdogViolations();
  ASSERT_NE(json, "");
  absl::optional<base::Value> violations_map_optional =
      base::JSONReader::Read(json);
  ASSERT_TRUE(violations_map_optional.has_value());
  base::Value::Dict violations_map =
      violations_map_optional.value().GetDict().Clone();
  base::Value::Dict* violation_dict = violations_map.FindDict("test-name");
  base::Value::List* violations = violation_dict->FindList("violations");
  base::Value::List* pingInfos =
      (*violations)[0].GetDict().FindList("pingInfos");
  ASSERT_EQ(pingInfos->size(), 60);
  ASSERT_EQ(*(*pingInfos)[0].GetDict().FindString("info"), "1");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, ViolationsAreEvictedAfterMax) {
  // Creates maxed Violation json file.
  std::unique_ptr<base::Value> dummy_map =
      std::make_unique<base::Value>(base::Value::Type::DICT);
  dummy_map->SetKey("test-name-1",
                    CreateDummyViolationDict("test-desc-1", 0, 99));
  dummy_map->SetKey("test-name-2",
                    CreateDummyViolationDict("test-desc-2", 1, 102));
  std::string json;
  base::JSONWriter::Write(*dummy_map, &json);
  starboard::ScopedFile file(watchdog_->GetWatchdogFilePath().c_str(),
                             kSbFileCreateAlways | kSbFileWrite);
  file.WriteAll(json.c_str(), static_cast<int>(json.size()));
  TearDown();
  watchdog_ = new watchdog::Watchdog();
  watchdog_->InitializeCustom(nullptr, std::string(kWatchdogViolationsJson),
                              kWatchdogMonitorFrequency);
  ASSERT_TRUE(watchdog_->Register("test-name-3", "test-desc-3",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Register("test-name-4", "test-desc-4",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  usleep(kWatchdogSleepDuration);

  json = watchdog_->GetWatchdogViolations({}, false);
  ASSERT_NE(json, "");
  absl::optional<base::Value> uncleared_violations_map_optional =
      base::JSONReader::Read(json);
  ASSERT_TRUE(uncleared_violations_map_optional.has_value());
  base::Value::Dict uncleared_violations_map =
      uncleared_violations_map_optional.value().GetDict().Clone();
  ASSERT_NE(uncleared_violations_map.size(), 0);
  base::Value::Dict* violation_dict =
      uncleared_violations_map.FindDict("test-name-1");
  base::Value::List* violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 99);
  violation_dict = uncleared_violations_map.FindDict("test-name-2");
  violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 99);
  ASSERT_EQ(
      *(*violations)[0].GetDict().FindString("timestampViolationMilliseconds"),
      "3");
  violation_dict = uncleared_violations_map.FindDict("test-name-3");
  violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 1);
  violation_dict = uncleared_violations_map.FindDict("test-name-4");
  violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 1);

  ASSERT_TRUE(watchdog_->Ping("test-name-3"));
  usleep(kWatchdogSleepDuration);

  json = watchdog_->GetWatchdogViolations();
  ASSERT_NE(json, "");
  absl::optional<base::Value> violations_map_optional =
      base::JSONReader::Read(json);
  ASSERT_TRUE(violations_map_optional.has_value());
  base::Value::Dict violations_map =
      violations_map_optional.value().GetDict().Clone();
  ASSERT_NE(violations_map.size(), 0);
  violation_dict = violations_map.FindDict("test-name-1");
  violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 98);
  ASSERT_EQ(
      *(*violations)[0].GetDict().FindString("timestampViolationMilliseconds"),
      "1");
  violation_dict = violations_map.FindDict("test-name-2");
  violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 99);
  violation_dict = violations_map.FindDict("test-name-3");
  violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 2);
  violation_dict = violations_map.FindDict("test-name-4");
  violations = violation_dict->FindList("violations");
  ASSERT_EQ(violations->size(), 1);

  ASSERT_TRUE(watchdog_->Unregister("test-name-3"));
  ASSERT_TRUE(watchdog_->Unregister("test-name-4"));
}

TEST_F(WatchdogTest, UpdateStateShouldPreventViolations) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  watchdog_->UpdateState(base::kApplicationStateBlurred);
  usleep(kWatchdogSleepDuration);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, TimeWaitShouldPreventViolations) {
  ASSERT_TRUE(watchdog_->Register(
      "test-name", "test-desc", base::kApplicationStateStarted,
      kWatchdogMonitorFrequency,
      kWatchdogSleepDuration + kWatchdogMonitorFrequency));
  usleep(kWatchdogSleepDuration);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, PingShouldPreventViolations) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  usleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Ping("test-name"));
  usleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Ping("test-name"));
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  usleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, PING));
  usleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, PING));
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  usleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, ALL));
  usleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, ALL));
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, PingByClientShouldPreventViolations) {
  std::shared_ptr<Client> client = watchdog_->RegisterByClient(
      "test-name", "test-desc", base::kApplicationStateStarted,
      kWatchdogMonitorFrequency);
  usleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->PingByClient(client));
  usleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->PingByClient(client));
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  usleep(kWatchdogSleepDuration);
  ASSERT_NE(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->UnregisterByClient(client));
}

TEST_F(WatchdogTest, UnregisterShouldPreventViolations) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  usleep(kWatchdogSleepDuration);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
}

TEST_F(WatchdogTest, UnregisterByClientShouldPreventViolations) {
  std::shared_ptr<Client> client = watchdog_->RegisterByClient(
      "test-name", "test-desc", base::kApplicationStateStarted,
      kWatchdogMonitorFrequency);
  ASSERT_TRUE(watchdog_->UnregisterByClient(client));
  usleep(kWatchdogSleepDuration);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
}

TEST_F(WatchdogTest, KillSwitchShouldPreventViolations) {
  TearDown();
  watchdog_ = new watchdog::Watchdog();
  watchdog_->InitializeStub();
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  usleep(kWatchdogSleepDuration);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, FrequentConsecutiveViolationsShouldNotWrite) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  usleep(kWatchdogSleepDuration);
  std::string write_json = "";
  starboard::ScopedFile read_file(watchdog_->GetWatchdogFilePath().c_str(),
                                  kSbFileOpenOnly | kSbFileRead);
  if (read_file.IsValid()) {
    int64_t kFileSize = read_file.GetSize();
    std::vector<char> buffer(kFileSize + 1, 0);
    read_file.ReadAll(buffer.data(), kFileSize);
    write_json = std::string(buffer.data());
  }
  ASSERT_NE(write_json, "");
  ASSERT_TRUE(watchdog_->Ping("test-name"));
  usleep(kWatchdogSleepDuration);
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  std::string no_write_json = "";
  starboard::ScopedFile read_file_again(
      watchdog_->GetWatchdogFilePath().c_str(), kSbFileOpenOnly | kSbFileRead);
  if (read_file_again.IsValid()) {
    int64_t kFileSize = read_file_again.GetSize();
    std::vector<char> buffer(kFileSize + 1, 0);
    read_file_again.ReadAll(buffer.data(), kFileSize);
    no_write_json = std::string(buffer.data());
  }
  ASSERT_NE(no_write_json, "");
  ASSERT_EQ(write_json, no_write_json);
  std::string json = watchdog_->GetWatchdogViolations();
  ASSERT_NE(write_json, json);
}

TEST_F(WatchdogTest, GetViolationClientNames) {
  ASSERT_TRUE(watchdog_->Register("test-name-1", "test-desc-1",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Register("test-name-2", "test-desc-2",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  usleep(kWatchdogSleepDuration);
  ASSERT_TRUE(watchdog_->Unregister("test-name-1"));
  ASSERT_TRUE(watchdog_->Unregister("test-name-2"));

  std::vector<std::string> names = watchdog_->GetWatchdogViolationClientNames();
  ASSERT_EQ(names.size(), 2);
  ASSERT_TRUE(std::find(names.begin(), names.end(), "test-name-1") !=
              names.end());
  ASSERT_TRUE(std::find(names.begin(), names.end(), "test-name-2") !=
              names.end());
  watchdog_->GetWatchdogViolations();
  names = watchdog_->GetWatchdogViolationClientNames();
  ASSERT_EQ(names.size(), 0);
}

TEST_F(WatchdogTest, GetPartialViolationsByClients) {
  ASSERT_TRUE(watchdog_->Register("test-name-1", "test-desc-1",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Register("test-name-2", "test-desc-2",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  usleep(kWatchdogSleepDuration);
  ASSERT_TRUE(watchdog_->Unregister("test-name-1"));
  ASSERT_TRUE(watchdog_->Unregister("test-name-2"));

  const std::vector<std::string> clients = {"test-name-1"};
  std::string json = watchdog_->GetWatchdogViolations(clients);
  ASSERT_NE(json, "");
  absl::optional<base::Value> violations_map_optional =
      base::JSONReader::Read(json);
  base::Value::Dict violations_map =
      violations_map_optional.value().GetDict().Clone();
  ASSERT_NE(violations_map.size(), 0);
  base::Value::Dict* violation_dict = violations_map.FindDict("test-name-1");
  ASSERT_NE(violation_dict, nullptr);
  violation_dict = violations_map.FindDict("test-name-2");
  ASSERT_EQ(violation_dict, nullptr);
  json = watchdog_->GetWatchdogViolations(clients);
  ASSERT_EQ(json, "");
}

TEST_F(WatchdogTest, EvictOldWatchdogViolations) {
  // Creates old Violation json file.
  std::unique_ptr<base::Value> dummy_map =
      std::make_unique<base::Value>(base::Value::Type::DICT);
  dummy_map->SetKey("test-name-old",
                    CreateDummyViolationDict("test-desc-old", 0, 1));
  std::string json;
  base::JSONWriter::Write(*dummy_map, &json);
  starboard::ScopedFile file(watchdog_->GetWatchdogFilePath().c_str(),
                             kSbFileCreateAlways | kSbFileWrite);
  TearDown();
  file.WriteAll(json.c_str(), static_cast<int>(json.size()));
  watchdog_ = new watchdog::Watchdog();
  watchdog_->InitializeCustom(nullptr, std::string(kWatchdogViolationsJson),
                              kWatchdogMonitorFrequency);

  ASSERT_NE(watchdog_->GetWatchdogViolations({}, false), "");
  ASSERT_EQ(watchdog_->GetWatchdogViolations({"test-name-new"}), "");
  ASSERT_EQ(watchdog_->GetWatchdogViolations({}, false), "");
}

TEST_F(WatchdogTest, CanGetLogTrace) {
  watchdog_->LogEvent("1");
  watchdog_->LogEvent("2");

  std::vector<std::string> expected = {"1", "2"};
  ASSERT_EQ(watchdog_->GetLogTrace(), expected);
}

TEST_F(WatchdogTest, CanClearLog) {
  watchdog_->LogEvent("1");
  watchdog_->LogEvent("2");

  watchdog_->ClearLog();

  ASSERT_EQ(watchdog_->GetLogTrace().size(), 0);
}

TEST_F(WatchdogTest, ViolationContainsLogTrace) {
  watchdog_->Register("test-name", "test-desc", base::kApplicationStateStarted,
                      kWatchdogMonitorFrequency);
  watchdog_->Ping("test-name", "test-ping");

  watchdog_->LogEvent("1");
  watchdog_->LogEvent("2");
  watchdog_->LogEvent("3");

  usleep(kWatchdogSleepDuration);

  std::string json = watchdog_->GetWatchdogViolations();
  absl::optional<base::Value> violations_map_optional =
      base::JSONReader::Read(json);
  ASSERT_TRUE(violations_map_optional.has_value());
  base::Value::Dict violations_map =
      violations_map_optional.value().GetDict().Clone();
  base::Value::List* violations =
      violations_map.FindDict("test-name")->FindList("violations");
  base::Value::List* logTrace = (*violations)[0].GetDict().FindList("logTrace");

  ASSERT_EQ(logTrace->size(), 3);
}

TEST_F(WatchdogTest, ViolationContainsEmptyLogTrace) {
  watchdog_->Register("test-name", "test-desc", base::kApplicationStateStarted,
                      kWatchdogMonitorFrequency);
  watchdog_->Ping("test-name", "test-ping");

  usleep(kWatchdogSleepDuration);

  std::string json = watchdog_->GetWatchdogViolations();
  absl::optional<base::Value> violations_map_optional =
      base::JSONReader::Read(json);
  ASSERT_TRUE(violations_map_optional.has_value());
  base::Value::Dict violations_map =
      violations_map_optional.value().GetDict().Clone();
  base::Value::List* violations =
      violations_map.FindDict("test-name")->FindList("violations");
  base::Value::List* logTrace = (*violations)[0].GetDict().FindList("logTrace");

  ASSERT_EQ(logTrace->size(), 0);
}

TEST_F(WatchdogTest, WatchdogMethodsAreNoopWhenWatchdogIsDisabled) {
  // init and destroy existing watchdog to re-initialize it later
  watchdog_->Register("test-name", "test-desc", base::kApplicationStateStarted,
                      kWatchdogMonitorFrequency);
  TearDown();

  // PersistentSettings doesn't have interface so it's not mockable
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kSettingsFileName);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](base::WaitableEvent* task_done) { task_done->Signal(); }, &task_done_);
  persistent_settings->SetPersistentSetting(
      kPersistentSettingWatchdogEnable, std::make_unique<base::Value>(false),
      std::move(closure), true);
  task_done_.Wait();

  watchdog_ = new watchdog::Watchdog();
  watchdog_->InitializeCustom(persistent_settings.get(),
                              std::string(kWatchdogViolationsJson),
                              kWatchdogMonitorFrequency);

  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Ping("test-name"));
  ASSERT_TRUE(watchdog_->PingByClient(nullptr));

  usleep(kWatchdogSleepDuration);

  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  ASSERT_TRUE(watchdog_->Unregister(""));
}

TEST_F(WatchdogTest, LogtraceMethodsAreNoopWhenLogtraceIsDisabled) {
  // init and destroy existing watchdog to re-initialize it later
  watchdog_->Register("test-name", "test-desc", base::kApplicationStateStarted,
                      kWatchdogMonitorFrequency);
  TearDown();

  // PersistentSettings doesn't have interface so it's not mockable
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kSettingsFileName);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](base::WaitableEvent* task_done) { task_done->Signal(); }, &task_done_);
  persistent_settings->SetPersistentSetting(
      kPersistentSettingLogtraceEnable, std::make_unique<base::Value>(false),
      std::move(closure), true);
  task_done_.Wait();

  watchdog_ = new watchdog::Watchdog();
  watchdog_->InitializeCustom(persistent_settings.get(),
                              std::string(kWatchdogViolationsJson),
                              kWatchdogMonitorFrequency);

  ASSERT_TRUE(watchdog_->LogEvent("foo"));
  ASSERT_EQ(watchdog_->GetLogTrace().size(), 0);
  ASSERT_NO_FATAL_FAILURE(watchdog_->ClearLog());
}

}  // namespace watchdog
}  // namespace cobalt
