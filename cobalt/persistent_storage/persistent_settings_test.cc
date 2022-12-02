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
    SbSystemGetPath(kSbSystemPathCacheDirectory, storage_dir.data(),
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
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
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
      persistent_settings.get(), &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(4.2), std::move(closure));
  test_done_.Wait();
}

TEST_F(PersistentSettingTest, GetSetBool) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
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
      persistent_settings.get(), &test_done_);

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
      persistent_settings.get(), &test_done_);

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(false), std::move(closure));

  test_done_.Wait();
}

TEST_F(PersistentSettingTest, GetDefaultInt) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
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
      persistent_settings.get(), &test_done_);

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(4.2), std::move(closure));
  test_done_.Wait();
}

TEST_F(PersistentSettingTest, GetSetInt) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ(-1, persistent_settings->GetPersistentSettingAsInt("key", 8));
        test_done->Signal();
      },
      persistent_settings.get(), &test_done_);
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
      persistent_settings.get(), &test_done_);
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
      persistent_settings.get(), &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(42), std::move(closure));
  test_done_.Wait();
}

TEST_F(PersistentSettingTest, GetDefaultString) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
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
      persistent_settings.get(), &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(4.2), std::move(closure));
  test_done_.Wait();
}

TEST_F(PersistentSettingTest, GetSetString) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        ASSERT_EQ("", persistent_settings->GetPersistentSettingAsString(
                          "key", "hello"));
        test_done->Signal();
      },
      persistent_settings.get(), &test_done_);

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
      persistent_settings.get(), &test_done_);

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
      persistent_settings.get(), &test_done_);
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
      persistent_settings.get(), &test_done_);
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
      persistent_settings.get(), &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("\\n"), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();
}

TEST_F(PersistentSettingTest, GetSetList) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        auto test_list = persistent_settings->GetPersistentSettingAsList("key");
        ASSERT_FALSE(test_list.empty());
        ASSERT_EQ(1, test_list.size());
        ASSERT_TRUE(test_list[0].is_string());
        ASSERT_EQ("hello", test_list[0].GetString());
        test_done->Signal();
      },
      persistent_settings.get(), &test_done_);

  std::vector<base::Value> list;
  list.emplace_back("hello");
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(list), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        auto test_list = persistent_settings->GetPersistentSettingAsList("key");
        ASSERT_FALSE(test_list.empty());
        ASSERT_EQ(2, test_list.size());
        ASSERT_TRUE(test_list[0].is_string());
        ASSERT_EQ("hello", test_list[0].GetString());
        ASSERT_TRUE(test_list[1].is_string());
        ASSERT_EQ("there", test_list[1].GetString());
        test_done->Signal();
      },
      persistent_settings.get(), &test_done_);

  list.emplace_back("there");
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(list), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        auto test_list = persistent_settings->GetPersistentSettingAsList("key");
        ASSERT_FALSE(test_list.empty());
        ASSERT_EQ(3, test_list.size());
        ASSERT_TRUE(test_list[0].is_string());
        ASSERT_EQ("hello", test_list[0].GetString());
        ASSERT_TRUE(test_list[1].is_string());
        ASSERT_EQ("there", test_list[1].GetString());
        ASSERT_TRUE(test_list[2].is_int());
        ASSERT_EQ(42, test_list[2].GetInt());
        test_done->Signal();
      },
      persistent_settings.get(), &test_done_);

  list.emplace_back(42);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(list), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();
}

TEST_F(PersistentSettingTest, GetSetDictionary) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        auto test_dict =
            persistent_settings->GetPersistentSettingAsDictionary("key");
        ASSERT_FALSE(test_dict.empty());
        ASSERT_EQ(1, test_dict.size());
        ASSERT_TRUE(test_dict["key_string"]->is_string());
        ASSERT_EQ("hello", test_dict["key_string"]->GetString());
        test_done->Signal();
      },
      persistent_settings.get(), &test_done_);

  base::flat_map<std::string, std::unique_ptr<base::Value>> dict;
  dict.try_emplace("key_string", std::make_unique<base::Value>("hello"));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(dict), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        auto test_dict =
            persistent_settings->GetPersistentSettingAsDictionary("key");
        ASSERT_FALSE(test_dict.empty());
        ASSERT_EQ(2, test_dict.size());
        ASSERT_TRUE(test_dict["key_string"]->is_string());
        ASSERT_EQ("hello", test_dict["key_string"]->GetString());
        ASSERT_TRUE(test_dict["key_int"]->is_int());
        ASSERT_EQ(42, test_dict["key_int"]->GetInt());
        test_done->Signal();
      },
      persistent_settings.get(), &test_done_);

  dict.try_emplace("key_int", std::make_unique<base::Value>(42));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(dict), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();
}

TEST_F(PersistentSettingTest, URLAsKey) {
  // Tests that json_pref_store has the correct SetValue and
  // RemoveValue changes for using a URL as a PersistentSettings
  // Key.
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::OnceClosure closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        auto test_dict = persistent_settings->GetPersistentSettingAsDictionary(
            "http://127.0.0.1:45019/");
        ASSERT_FALSE(test_dict.empty());
        ASSERT_EQ(1, test_dict.size());
        ASSERT_TRUE(test_dict["http://127.0.0.1:45019/"]->is_string());
        ASSERT_EQ("Dictionary URL Key Works!",
                  test_dict["http://127.0.0.1:45019/"]->GetString());
        test_done->Signal();
      },
      persistent_settings.get(), &test_done_);

  // Test that json_pref_store uses SetKey instead of Set, making URL
  // keys viable.
  base::flat_map<std::string, std::unique_ptr<base::Value>> dict;
  dict.try_emplace("http://127.0.0.1:45019/",
                   std::make_unique<base::Value>("Dictionary URL Key Works!"));
  persistent_settings->SetPersistentSetting("http://127.0.0.1:45019/",
                                            std::make_unique<base::Value>(dict),
                                            std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  closure = base::BindOnce(
      [](PersistentSettings* persistent_settings,
         base::WaitableEvent* test_done) {
        auto test_dict = persistent_settings->GetPersistentSettingAsDictionary(
            "http://127.0.0.1:45019/");
        ASSERT_TRUE(test_dict.empty());
        test_done->Signal();
      },
      persistent_settings.get(), &test_done_);
  persistent_settings->RemovePersistentSetting("http://127.0.0.1:45019/",
                                               std::move(closure));
  test_done_.Wait();
  test_done_.Reset();
}

TEST_F(PersistentSettingTest, RemoveSetting) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
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
      persistent_settings.get(), &test_done_);
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
      persistent_settings.get(), &test_done_);
  persistent_settings->RemovePersistentSetting("key", std::move(closure));
  test_done_.Wait();
  test_done_.Reset();
}

TEST_F(PersistentSettingTest, DeleteSettings) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
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
      persistent_settings.get(), &test_done_);
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
      persistent_settings.get(), &test_done_);
  persistent_settings->DeletePersistentSettings(std::move(closure));
  test_done_.Wait();
  test_done_.Reset();
}

TEST_F(PersistentSettingTest, InvalidSettings) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
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
      persistent_settings.get(), &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  // Sleep for one second to allow for the previous persistent_setting's
  // JsonPrefStore instance time to write to disk before creating a new
  // persistent_settings and JsonPrefStore instance.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
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
      persistent_settings.get(), &test_done_);
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(false), std::move(closure));
  test_done_.Wait();
  test_done_.Reset();

  persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
}

}  // namespace persistent_storage
}  // namespace cobalt
