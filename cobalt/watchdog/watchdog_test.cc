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

#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "cobalt/watchdog/watchdog.h"
#include "starboard/common/file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace watchdog {

namespace {

const char kWatchdogViolationsJson[] = "watchdog_test.json";
const int64_t kWatchdogMonitorFrequency = 100000;

}  // namespace

class WatchdogTest : public testing::Test {
 protected:
  WatchdogTest() {}

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
  }

  base::Value CreateDummyViolationDict(std::string desc, int begin, int end) {
    base::Value violation_dict(base::Value::Type::DICTIONARY);
    violation_dict.SetKey("description", base::Value(desc));
    base::Value list(base::Value::Type::LIST);
    for (int i = begin; i < end; i++)
      list.GetList().emplace_back(CreateDummyViolation(i));
    violation_dict.SetKey("violations", list.Clone());
    return violation_dict.Clone();
  }

  base::Value CreateDummyViolation(int timestamp_violation) {
    base::Value violation(base::Value::Type::DICTIONARY);
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

  watchdog::Watchdog* watchdog_;
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
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
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

TEST_F(WatchdogTest, UnmatchedUnregistersShouldFail) {
  ASSERT_FALSE(watchdog_->Unregister("test-name"));
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  ASSERT_FALSE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, UnmatchedPingsShouldFail) {
  ASSERT_FALSE(watchdog_->Ping("test-name"));
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Ping("test-name"));
  ASSERT_TRUE(watchdog_->Ping("test-name"));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  ASSERT_FALSE(watchdog_->Ping("test-name"));
}

TEST_F(WatchdogTest, PingOnlyAcceptsValidParameters) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Ping("test-name", "42"));
  ASSERT_FALSE(
      watchdog_->Ping("test-name",
                      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                      "xxxxxxxxxxxxxxxxxxxxxxxxxxx"));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, ViolationsJsonShouldPersistAndBeValid) {
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Ping("test-name", "test-ping"));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  TearDown();
  watchdog_ = new watchdog::Watchdog();
  watchdog_->InitializeCustom(nullptr, std::string(kWatchdogViolationsJson),
                              kWatchdogMonitorFrequency);

  // Validates Violation json file.
  std::string json = watchdog_->GetWatchdogViolations();
  ASSERT_NE(json, "");
  std::unique_ptr<base::Value> violations_map = base::JSONReader::Read(json);
  ASSERT_NE(violations_map, nullptr);
  base::Value* violation_dict = violations_map->FindKey("test-name");
  ASSERT_NE(violation_dict, nullptr);
  base::Value* description = violation_dict->FindKey("description");
  ASSERT_NE(description, nullptr);
  ASSERT_EQ(description->GetString(), "test-desc");
  base::Value* violations = violation_dict->FindKey("violations");
  ASSERT_NE(violations, nullptr);
  ASSERT_EQ(violations->GetList().size(), 1);
  base::Value* monitor_state = violations->GetList()[0].FindKey("monitorState");
  ASSERT_NE(monitor_state, nullptr);
  ASSERT_EQ(
      monitor_state->GetString(),
      std::string(GetApplicationStateString(base::kApplicationStateStarted)));
  base::Value* ping_infos = violations->GetList()[0].FindKey("pingInfos");
  ASSERT_NE(ping_infos, nullptr);
  ASSERT_EQ(ping_infos->GetList().size(), 1);
  base::Value* info = ping_infos->GetList()[0].FindKey("info");
  ASSERT_NE(info, nullptr);
  ASSERT_EQ(info->GetString(), "test-ping");
  base::Value* timestamp_milliseconds =
      ping_infos->GetList()[0].FindKey("timestampMilliseconds");
  ASSERT_NE(timestamp_milliseconds, nullptr);
  std::stoll(timestamp_milliseconds->GetString());
  base::Value* registered_clients =
      violations->GetList()[0].FindKey("registeredClients");
  ASSERT_NE(registered_clients, nullptr);
  ASSERT_EQ(registered_clients->GetList().size(), 1);
  ASSERT_EQ(registered_clients->GetList()[0].GetString(), "test-name");
  base::Value* time_interval_milliseconds =
      violations->GetList()[0].FindKey("timeIntervalMilliseconds");
  ASSERT_NE(time_interval_milliseconds, nullptr);
  std::stoll(time_interval_milliseconds->GetString());
  base::Value* time_wait_milliseconds =
      violations->GetList()[0].FindKey("timeWaitMilliseconds");
  ASSERT_NE(time_wait_milliseconds, nullptr);
  std::stoll(time_wait_milliseconds->GetString());
  base::Value* timestamp_last_pinged_milliseconds =
      violations->GetList()[0].FindKey("timestampLastPingedMilliseconds");
  ASSERT_NE(timestamp_last_pinged_milliseconds, nullptr);
  std::stoll(timestamp_last_pinged_milliseconds->GetString());
  base::Value* timestamp_registered_milliseconds =
      violations->GetList()[0].FindKey("timestampRegisteredMilliseconds");
  ASSERT_NE(timestamp_registered_milliseconds, nullptr);
  std::stoll(timestamp_registered_milliseconds->GetString());
  base::Value* timestamp_violation_milliseconds =
      violations->GetList()[0].FindKey("timestampViolationMilliseconds");
  ASSERT_NE(timestamp_violation_milliseconds, nullptr);
  std::stoll(timestamp_violation_milliseconds->GetString());
  base::Value* violation_duration_milliseconds =
      violations->GetList()[0].FindKey("violationDurationMilliseconds");
  ASSERT_NE(violation_duration_milliseconds, nullptr);
  std::stoll(violation_duration_milliseconds->GetString());
}

TEST_F(WatchdogTest, RedundantViolationsShouldStack) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  std::string json = watchdog_->GetWatchdogViolations(false);
  ASSERT_NE(json, "");
  std::unique_ptr<base::Value> uncleared_violations_map =
      base::JSONReader::Read(json);
  ASSERT_NE(uncleared_violations_map, nullptr);
  base::Value* violation_dict = uncleared_violations_map->FindKey("test-name");
  base::Value* violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 1);
  std::string uncleared_timestamp =
      violations->GetList()[0]
          .FindKey("timestampLastPingedMilliseconds")
          ->GetString();
  int64_t uncleared_duration =
      std::stoll(violations->GetList()[0]
                     .FindKey("violationDurationMilliseconds")
                     ->GetString());
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  json = watchdog_->GetWatchdogViolations(false);
  ASSERT_NE(json, "");
  std::unique_ptr<base::Value> violations_map = base::JSONReader::Read(json);
  ASSERT_NE(violations_map, nullptr);
  violation_dict = violations_map->FindKey("test-name");
  violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 1);
  std::string timestamp = violations->GetList()[0]
                              .FindKey("timestampLastPingedMilliseconds")
                              ->GetString();
  int64_t duration = std::stoll(violations->GetList()[0]
                                    .FindKey("violationDurationMilliseconds")
                                    ->GetString());
  ASSERT_EQ(uncleared_timestamp, timestamp);
  ASSERT_LT(uncleared_duration, duration);
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, ViolationsShouldResetAfterFetch) {
  ASSERT_TRUE(watchdog_->Register("test-name-1", "test-desc-1",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  ASSERT_TRUE(watchdog_->Unregister("test-name-1"));
  std::string json = watchdog_->GetWatchdogViolations();
  ASSERT_NE(json.find("test-name-1"), std::string::npos);
  ASSERT_EQ(json.find("test-name-2"), std::string::npos);
  ASSERT_TRUE(watchdog_->Register("test-name-2", "test-desc-2",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  ASSERT_TRUE(watchdog_->Unregister("test-name-2"));
  json = watchdog_->GetWatchdogViolations();
  ASSERT_EQ(json.find("test-name-1"), std::string::npos);
  ASSERT_NE(json.find("test-name-2"), std::string::npos);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
}

TEST_F(WatchdogTest, PingInfosAreEvictedAfterMax) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test_desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  for (int i = 0; i < 21; i++) {
    ASSERT_TRUE(watchdog_->Ping("test-name", std::to_string(i)));
  }
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  std::string json = watchdog_->GetWatchdogViolations();
  ASSERT_NE(json, "");
  std::unique_ptr<base::Value> violations_map = base::JSONReader::Read(json);
  ASSERT_NE(violations_map, nullptr);
  base::Value* violation_dict = violations_map->FindKey("test-name");
  base::Value* violations = violation_dict->FindKey("violations");
  base::Value* pingInfos = violations->GetList()[0].FindKey("pingInfos");
  ASSERT_EQ(pingInfos->GetList().size(), 20);
  ASSERT_EQ(pingInfos->GetList()[0].FindKey("info")->GetString(), "1");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, ViolationsAreEvictedAfterMax) {
  // Creates maxed Violation json file.
  std::unique_ptr<base::Value> dummy_map =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  dummy_map->SetKey("test-name-1",
                    CreateDummyViolationDict("test-desc-1", 0, 99));
  dummy_map->SetKey("test-name-2",
                    CreateDummyViolationDict("test-desc-2", 1, 102));
  std::string json;
  base::JSONWriter::Write(*dummy_map, &json);
  starboard::ScopedFile file(watchdog_->GetWatchdogFilePath().c_str(),
                             kSbFileCreateAlways | kSbFileWrite);
  file.WriteAll(json.c_str(), static_cast<int>(json.size()));

  ASSERT_TRUE(watchdog_->Register("test-name-3", "test-desc-3",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Register("test-name-4", "test-desc-4",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);

  json = watchdog_->GetWatchdogViolations(false);
  ASSERT_NE(json, "");
  std::unique_ptr<base::Value> uncleared_violations_map =
      base::JSONReader::Read(json);
  ASSERT_NE(uncleared_violations_map, nullptr);
  base::Value* violation_dict =
      uncleared_violations_map->FindKey("test-name-1");
  base::Value* violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 99);
  violation_dict = uncleared_violations_map->FindKey("test-name-2");
  violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 99);
  ASSERT_EQ(violations->GetList()[0]
                .FindKey("timestampViolationMilliseconds")
                ->GetString(),
            "3");
  violation_dict = uncleared_violations_map->FindKey("test-name-3");
  violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 1);
  violation_dict = uncleared_violations_map->FindKey("test-name-4");
  violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 1);

  ASSERT_TRUE(watchdog_->Ping("test-name-3"));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);

  json = watchdog_->GetWatchdogViolations();
  ASSERT_NE(json, "");
  std::unique_ptr<base::Value> violations_map = base::JSONReader::Read(json);
  ASSERT_NE(violations_map, nullptr);
  violation_dict = violations_map->FindKey("test-name-1");
  violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 98);
  ASSERT_EQ(violations->GetList()[0]
                .FindKey("timestampViolationMilliseconds")
                ->GetString(),
            "1");
  violation_dict = violations_map->FindKey("test-name-2");
  violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 99);
  violation_dict = violations_map->FindKey("test-name-3");
  violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 2);
  violation_dict = violations_map->FindKey("test-name-4");
  violations = violation_dict->FindKey("violations");
  ASSERT_EQ(violations->GetList().size(), 1);

  ASSERT_TRUE(watchdog_->Unregister("test-name-3"));
  ASSERT_TRUE(watchdog_->Unregister("test-name-4"));
}

TEST_F(WatchdogTest, UpdateStateShouldPreventViolations) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  watchdog_->UpdateState(base::kApplicationStateBlurred);
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, TimeWaitShouldPreventViolations) {
  ASSERT_TRUE(watchdog_->Register(
      "test-name", "test-desc", base::kApplicationStateStarted,
      kWatchdogMonitorFrequency, kWatchdogMonitorFrequency * 3));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, PingsShouldPreventViolations) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  SbThreadSleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Ping("test-name"));
  SbThreadSleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Ping("test-name"));
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  SbThreadSleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, PING));
  SbThreadSleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, PING));
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  SbThreadSleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, ALL));
  SbThreadSleep(kWatchdogMonitorFrequency / 2);
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency, 0, ALL));
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, UnregisterShouldPreventViolations) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
}

TEST_F(WatchdogTest, KillSwitchShouldPreventViolations) {
  TearDown();
  watchdog_ = new watchdog::Watchdog();
  watchdog_->InitializeStub();
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
  ASSERT_EQ(watchdog_->GetWatchdogViolations(), "");
  ASSERT_TRUE(watchdog_->Unregister("test-name"));
}

TEST_F(WatchdogTest, FrequentConsecutiveViolationsShouldNotWrite) {
  ASSERT_TRUE(watchdog_->Register("test-name", "test-desc",
                                  base::kApplicationStateStarted,
                                  kWatchdogMonitorFrequency));
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
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
  SbThreadSleep(kWatchdogMonitorFrequency * 2);
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

}  // namespace watchdog
}  // namespace cobalt
