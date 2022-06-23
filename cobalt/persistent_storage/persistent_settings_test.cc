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

#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace persistent_storage {

namespace {

const char kPersistentSettingsJson[] = "test_settings.json";

}  // namespace

class PersistentSettingTest : public testing::Test {
 protected:
  PersistentSettingTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
            base::test::ScopedTaskEnvironment::ExecutionMode::ASYNC) {
    std::vector<char> storage_dir(kSbFileMaxPath + 1, 0);
    SbSystemGetPath(kSbSystemPathStorageDirectory, storage_dir.data(),
                    kSbFileMaxPath);

    persistent_settings_file_ = std::string(storage_dir.data()) +
                                kSbFileSepString + kPersistentSettingsJson;
    SB_LOG(INFO) << "Persistent settings test file path: "
                 << persistent_settings_file_;
  }

  void SetUp() final {
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
  }

  void TearDown() final {
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::string persistent_settings_file_;
};

TEST_F(PersistentSettingTest, GetDefaultBool) {
  auto persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  // does not exist
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  // exists but invalid
  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(4.2));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsInt("key", true));
}

TEST_F(PersistentSettingTest, GetSetBool) {
  auto persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(false));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
}

TEST_F(PersistentSettingTest, GetDefaultInt) {
  auto persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  // does not exist
  ASSERT_EQ(-1, persistent_settings->GetPersistentSettingAsInt("key", -1));
  ASSERT_EQ(0, persistent_settings->GetPersistentSettingAsInt("key", 0));
  ASSERT_EQ(42, persistent_settings->GetPersistentSettingAsInt("key", 42));

  // exists but invalid
  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(4.2));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ(8, persistent_settings->GetPersistentSettingAsInt("key", 8));
}

TEST_F(PersistentSettingTest, GetSetInt) {
  auto persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(-1));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ(-1, persistent_settings->GetPersistentSettingAsInt("key", 8));

  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(0));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ(0, persistent_settings->GetPersistentSettingAsInt("key", 8));

  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(42));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ(42, persistent_settings->GetPersistentSettingAsInt("key", 8));
}

TEST_F(PersistentSettingTest, GetDefaultString) {
  auto persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  // does not exist
  ASSERT_EQ("", persistent_settings->GetPersistentSettingAsString("key", ""));
  ASSERT_EQ("hello there", persistent_settings->GetPersistentSettingAsString(
                               "key", "hello there"));
  ASSERT_EQ("42",
            persistent_settings->GetPersistentSettingAsString("key", "42"));
  ASSERT_EQ("\n",
            persistent_settings->GetPersistentSettingAsString("key", "\n"));
  ASSERT_EQ("\\n",
            persistent_settings->GetPersistentSettingAsString("key", "\\n"));

  // exists but invalid
  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(4.2));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ("hello",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));
}

TEST_F(PersistentSettingTest, GetSetString) {
  auto persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(""));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ("",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("hello there"));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ("hello there",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("42"));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ("42",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("\n"));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ("\n",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("\\n"));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ("\\n",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));
}

TEST_F(PersistentSettingTest, RemoveSetting) {
  auto persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings->RemovePersistentSetting("key");
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
}

TEST_F(PersistentSettingTest, DeleteSettings) {
  auto persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings->DeletePersistentSettings();
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
}

TEST_F(PersistentSettingTest, InvalidSettings) {
  auto persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(false));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings = std::make_unique<PersistentSettings>(
      kPersistentSettingsJson,
      scoped_task_environment_.GetMainThreadTaskRunner());
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
}

}  // namespace persistent_storage
}  // namespace cobalt
