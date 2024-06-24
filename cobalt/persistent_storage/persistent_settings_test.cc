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

#include "cobalt/persistent_storage/persistent_settings.h"

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/test/task_environment.h"
#include "base/values.h"
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
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC) {
    std::vector<char> storage_dir(kSbFileMaxPath + 1, 0);
    SbSystemGetPath(kSbSystemPathCacheDirectory, storage_dir.data(),
                    kSbFileMaxPath);
    persistent_settings_file_ = std::string(storage_dir.data()) +
                                kSbFileSepString + kPersistentSettingsJson;
  }

  void SetUp() final {
    base::DeletePathRecursively(
        base::FilePath(persistent_settings_file_.c_str()));
    persistent_settings_ =
        std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  }

  void TearDown() final {
    base::DeletePathRecursively(
        base::FilePath(persistent_settings_file_.c_str()));
  }

  void Fence(const base::Location& location) {
    persistent_settings_->Fence(location);
  }

  void WaitForPendingFileWrite() {
    persistent_settings_->WaitForPendingFileWrite();
  }

  base::test::TaskEnvironment scoped_task_environment_;
  std::string persistent_settings_file_;
  std::unique_ptr<PersistentSettings> persistent_settings_;
};

TEST_F(PersistentSettingTest, Set) {
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_TRUE(value.is_none());
  }
  persistent_settings_->Set("key", base::Value(4.2));
  Fence(FROM_HERE);
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_EQ(4.2, value.GetIfDouble().value_or(0));
  }
}

TEST_F(PersistentSettingTest, Remove) {
  persistent_settings_->Set("key", base::Value(true));
  Fence(FROM_HERE);
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_EQ(true, value.GetIfBool().value_or(false));
  }
  persistent_settings_->Remove("key");
  Fence(FROM_HERE);
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_TRUE(value.is_none());
  }
}

TEST_F(PersistentSettingTest, RemoveAll) {
  persistent_settings_->Set("key", base::Value(true));
  Fence(FROM_HERE);
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_EQ(true, value.GetIfBool().value_or(false));
  }
  persistent_settings_->RemoveAll();
  Fence(FROM_HERE);
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_TRUE(value.is_none());
  }
}

TEST_F(PersistentSettingTest, PersistValidatedSettings) {
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_TRUE(value.is_none());
  }
  persistent_settings_->Set("key", base::Value(true));
  Fence(FROM_HERE);
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_EQ(true, value.GetIfBool().value_or(false));
  }

  persistent_settings_ =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings_->Validate();
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_TRUE(value.is_none());
  }
  persistent_settings_->Set("key", base::Value(true));
  Fence(FROM_HERE);
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_EQ(true, value.GetIfBool().value_or(false));
  }
  WaitForPendingFileWrite();

  persistent_settings_ =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  {
    base::Value value;
    persistent_settings_->Get("key", &value);
    EXPECT_EQ(true, value.GetIfBool().value_or(false));
  }
}

}  // namespace persistent_storage
}  // namespace cobalt
