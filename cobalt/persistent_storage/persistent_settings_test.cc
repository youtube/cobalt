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

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_internal.h"
#include "base/callback_forward.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/platform_thread.h"
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
  }

  void SetUp() final {
    test_done_.Reset();
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
  }

  void TearDown() final {
    test_done_.Reset();
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::string persistent_settings_file_;
  base::WaitableEvent test_done_ = {
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
};

TEST_F(PersistentSettingTest, GetDefaultBool) {
  auto persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  // does not exist
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  // exists but invalid
  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsInt("key", true));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(4.2), std::move(closure));
  test_done_.Wait();
  delete persistent_settings;
}

TEST_F(PersistentSettingTest, GetSetBool) {
  auto persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", true));
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", false));
        test_done->Signal();
      },
      persistent_settings, &test_done_);

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_FALSE(
            persistent_settings->GetPersistentSettingAsBool("key", true));
        ASSERT_FALSE(
            persistent_settings->GetPersistentSettingAsBool("key", false));
        test_done->Signal();
      },
      persistent_settings, &test_done_);

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(false), std::move(closure));

  test_done_.Wait();
  delete persistent_settings;
}

TEST_F(PersistentSettingTest, GetDefaultInt) {
  auto persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  // does not exist
  ASSERT_EQ(-1, persistent_settings->GetPersistentSettingAsInt("key", -1));
  ASSERT_EQ(0, persistent_settings->GetPersistentSettingAsInt("key", 0));
  ASSERT_EQ(42, persistent_settings->GetPersistentSettingAsInt("key", 42));

  // exists but invalid
  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ(8, persistent_settings->GetPersistentSettingAsInt("key", 8));
        test_done->Signal();
      },
      persistent_settings, &test_done_);

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(4.2), std::move(closure));
  test_done_.Wait();
}

TEST_F(PersistentSettingTest, GetSetInt) {
  auto persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ(-1, persistent_settings->GetPersistentSettingAsInt("key", 8));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(-1), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ(0, persistent_settings->GetPersistentSettingAsInt("key", 8));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(0), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ(42, persistent_settings->GetPersistentSettingAsInt("key", 8));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(42), std::move(closure));
  test_done_.Wait();
}

TEST_F(PersistentSettingTest, GetDefaultString) {
  auto persistent_settings = new PersistentSettings(kPersistentSettingsJson);
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
  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ("hello", persistent_settings->GetPersistentSettingAsString(
                               "key", "hello"));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(4.2), std::move(closure));
  test_done_.Wait();
  delete persistent_settings;
}

TEST_F(PersistentSettingTest, GetSetString) {
  auto persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ("", persistent_settings->GetPersistentSettingAsString(
                          "key", "hello"));
        test_done->Signal();
      },
      persistent_settings, &test_done_);

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(""), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ(
            "hello there",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));
        test_done->Signal();
      },
      persistent_settings, &test_done_);

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("hello there"), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ("42", persistent_settings->GetPersistentSettingAsString(
                            "key", "hello"));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("42"), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ("\n", persistent_settings->GetPersistentSettingAsString(
                            "key", "hello"));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("\n"), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ("\\n", persistent_settings->GetPersistentSettingAsString(
                             "key", "hello"));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("\\n"), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();
}

TEST_F(PersistentSettingTest, RemoveSetting) {
  auto persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", true));
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", false));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", true));
        ASSERT_FALSE(
            persistent_settings->GetPersistentSettingAsBool("key", false));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->RemovePersistentSetting("key", std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  delete persistent_settings;
}

TEST_F(PersistentSettingTest, DeleteSettings) {
  auto persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", true));
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", false));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", true));
        ASSERT_FALSE(
            persistent_settings->GetPersistentSettingAsBool("key", false));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->DeletePersistentSettings(std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  delete persistent_settings;
}

TEST_F(PersistentSettingTest, InvalidSettings) {
  auto persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", true));
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", false));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  delete persistent_settings;
  // Sleep for one second to allow for the previous persistent_setting's
  // JsonPrefStore instance time to write to disk before creating a new
  // persistent_settings and JsonPrefStore instance.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", true));
        ASSERT_TRUE(
            persistent_settings->GetPersistentSettingAsBool("key", false));
        test_done->Signal();
      },
      persistent_settings, &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(false), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  delete persistent_settings;
  persistent_settings = new PersistentSettings(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  delete persistent_settings;
}

}  // namespace persistent_storage
}  // namespace cobalt
