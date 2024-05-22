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

#include "base/test/task_environment.h"
#include "base/values.h"
#include "starboard/common/file.h"
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
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
  }

  void TearDown() final {
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
  }

  base::test::TaskEnvironment scoped_task_environment_;
  std::string persistent_settings_file_;
};

TEST_F(PersistentSettingTest, GetDefaultBool) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  // does not exist
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  // exists but invalid
  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(4.2));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
}

TEST_F(PersistentSettingTest, GetSetBool) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(false));
  EXPECT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", true));
  EXPECT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
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
  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(4.2));
  EXPECT_EQ(8, persistent_settings->GetPersistentSettingAsInt("key", 8));
}

TEST_F(PersistentSettingTest, GetSetInt) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(-1));
  EXPECT_EQ(-1, persistent_settings->GetPersistentSettingAsInt("key", 8));
  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(0));
  EXPECT_EQ(0, persistent_settings->GetPersistentSettingAsInt("key", 8));
  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(42));
  EXPECT_EQ(42, persistent_settings->GetPersistentSettingAsInt("key", 8));
}

TEST_F(PersistentSettingTest, GetDefaultDouble) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  // does not exist
  ASSERT_EQ(-1.1,
            persistent_settings->GetPersistentSettingAsDouble("key", -1.1));
  ASSERT_EQ(0.1, persistent_settings->GetPersistentSettingAsDouble("key", 0.1));
  ASSERT_EQ(42.1,
            persistent_settings->GetPersistentSettingAsDouble("key", 42.1));

  // exists but invalid
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true));
  EXPECT_EQ(8.1, persistent_settings->GetPersistentSettingAsDouble("key", 8.1));
}

TEST_F(PersistentSettingTest, GetSetDouble) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(-1.1));
  EXPECT_EQ(-1.1,
            persistent_settings->GetPersistentSettingAsDouble("key", 8.1));
  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(0.1));
  EXPECT_EQ(0.1, persistent_settings->GetPersistentSettingAsDouble("key", 8.1));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(42.1));
  EXPECT_EQ(42.1,
            persistent_settings->GetPersistentSettingAsDouble("key", 8.1));
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
  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(4.2));
  EXPECT_EQ("hello",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));
}

TEST_F(PersistentSettingTest, GetSetString) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  persistent_settings->SetPersistentSetting("key",
                                            std::make_unique<base::Value>(""));
  EXPECT_EQ("",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("hello there"));
  EXPECT_EQ("hello there",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("42"));
  EXPECT_EQ("42",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("\n"));
  EXPECT_EQ("\n",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>("\\n"));
  EXPECT_EQ("\\n",
            persistent_settings->GetPersistentSettingAsString("key", "hello"));
}

TEST_F(PersistentSettingTest, GetSetList) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::Value list(base::Value::Type::LIST);
  list.GetList().Append("hello");
  persistent_settings->SetPersistentSetting(
      "key", base::Value::ToUniquePtrValue(list.Clone()));
  auto test_list = persistent_settings->GetPersistentSettingAsList("key");
  EXPECT_FALSE(test_list.empty());
  EXPECT_EQ(1, test_list.size());
  EXPECT_TRUE(test_list[0].is_string());
  EXPECT_EQ("hello", test_list[0].GetString());

  list.GetList().Append("there");
  persistent_settings->SetPersistentSetting(
      "key", base::Value::ToUniquePtrValue(list.Clone()));
  test_list = persistent_settings->GetPersistentSettingAsList("key");
  EXPECT_FALSE(test_list.empty());
  EXPECT_EQ(2, test_list.size());
  EXPECT_TRUE(test_list[0].is_string());
  EXPECT_EQ("hello", test_list[0].GetString());
  EXPECT_TRUE(test_list[1].is_string());
  EXPECT_EQ("there", test_list[1].GetString());

  list.GetList().Append(42);
  persistent_settings->SetPersistentSetting(
      "key", base::Value::ToUniquePtrValue(list.Clone()));
  test_list = persistent_settings->GetPersistentSettingAsList("key");
  EXPECT_FALSE(test_list.empty());
  EXPECT_EQ(3, test_list.size());
  EXPECT_TRUE(test_list[0].is_string());
  EXPECT_EQ("hello", test_list[0].GetString());
  EXPECT_TRUE(test_list[1].is_string());
  EXPECT_EQ("there", test_list[1].GetString());
  EXPECT_TRUE(test_list[2].is_int());
  EXPECT_EQ(42, test_list[2].GetInt());
}

TEST_F(PersistentSettingTest, GetSetDictionary) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  base::Value dict(base::Value::Type::DICT);
  dict.GetDict().Set("key_string", "hello");
  persistent_settings->SetPersistentSetting(
      "key", base::Value::ToUniquePtrValue(dict.Clone()));
  auto test_dict = persistent_settings->GetPersistentSettingAsDictionary("key");
  EXPECT_FALSE(test_dict.empty());
  EXPECT_EQ(1, test_dict.size());
  EXPECT_TRUE(test_dict["key_string"]->is_string());
  EXPECT_EQ("hello", test_dict["key_string"]->GetString());

  dict.GetDict().Set("key_int", 42);
  persistent_settings->SetPersistentSetting(
      "key", base::Value::ToUniquePtrValue(dict.Clone()));
  test_dict = persistent_settings->GetPersistentSettingAsDictionary("key");
  EXPECT_FALSE(test_dict.empty());
  EXPECT_EQ(2, test_dict.size());
  EXPECT_TRUE(test_dict["key_string"]->is_string());
  EXPECT_EQ("hello", test_dict["key_string"]->GetString());
  EXPECT_TRUE(test_dict["key_int"]->is_int());
  EXPECT_EQ(42, test_dict["key_int"]->GetInt());

  dict.GetDict().Set("http://127.0.0.1:45019/", "Dictionary URL Key Works!");
  persistent_settings->SetPersistentSetting(
      "key", base::Value::ToUniquePtrValue(dict.Clone()));
  test_dict = persistent_settings->GetPersistentSettingAsDictionary("key");
  EXPECT_FALSE(test_dict.empty());
  EXPECT_EQ(3, test_dict.size());
  EXPECT_TRUE(test_dict["key_string"]->is_string());
  EXPECT_EQ("hello", test_dict["key_string"]->GetString());
  EXPECT_TRUE(test_dict["key_int"]->is_int());
  EXPECT_EQ(42, test_dict["key_int"]->GetInt());
  EXPECT_TRUE(test_dict["http://127.0.0.1:45019/"]->is_string());
  EXPECT_EQ("Dictionary URL Key Works!",
            test_dict["http://127.0.0.1:45019/"]->GetString());
}

TEST_F(PersistentSettingTest, RemoveSetting) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));
  persistent_settings->RemovePersistentSetting("key");
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  EXPECT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
}

TEST_F(PersistentSettingTest, DeleteSettings) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));
  persistent_settings->DeletePersistentSettings();
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  EXPECT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
}

TEST_F(PersistentSettingTest, InvalidSettings) {
  auto persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();

  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true), true);
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(false), true);
  EXPECT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", true));
  EXPECT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));

  persistent_settings =
      std::make_unique<PersistentSettings>(kPersistentSettingsJson);
  persistent_settings->ValidatePersistentSettings();
  ASSERT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  ASSERT_FALSE(persistent_settings->GetPersistentSettingAsBool("key", false));
  persistent_settings->SetPersistentSetting(
      "key", std::make_unique<base::Value>(true));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", true));
  EXPECT_TRUE(persistent_settings->GetPersistentSettingAsBool("key", false));
}

}  // namespace persistent_storage
}  // namespace cobalt
