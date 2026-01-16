// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/migrate_storage_record/migration_manager.h"

#include <numeric>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "cobalt/shell/browser/migrate_storage_record/storage.pb.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::Return;

namespace cobalt {
namespace migrate_storage_record {

class MigrationManagerTest : public testing::Test {
 public:
  Task GroupTasks(std::vector<Task> tasks) {
    return MigrationManager::GroupTasks(std::move(tasks));
  }

  std::vector<std::unique_ptr<net::CanonicalCookie>> ToCanonicalCookies(
      const cobalt::storage::Storage& storage) {
    return MigrationManager::ToCanonicalCookies(storage);
  }

  std::vector<std::unique_ptr<std::pair<std::string, std::string>>>
  ToLocalStorageItems(const url::Origin& page_origin,
                      const cobalt::storage::Storage& storage) {
    return MigrationManager::ToLocalStorageItems(page_origin, storage);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// TODO(b/399166308): Add more test cases for specific migration tasks.
TEST_F(MigrationManagerTest, VerifyGroupTasksRunsCallbacksSequentially) {
  constexpr uint32_t kTaskCount = 100;
  std::vector<Task> tasks;
  std::vector<int> collected_values;
  for (size_t i = 0; i < kTaskCount; i++) {
    tasks.push_back(base::BindOnce(
        [](std::vector<int>& collected_values, int i,
           base::OnceClosure callback) {
          collected_values.push_back(i);
          std::move(callback).Run();
        },
        std::ref(collected_values), i));
  }
  EXPECT_TRUE(collected_values.empty());
  Task grouped_task = GroupTasks(std::move(tasks));
  std::move(grouped_task).Run(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  std::vector<int> expected_values(kTaskCount);
  std::iota(expected_values.begin(), expected_values.end(), 0);
  EXPECT_THAT(collected_values, ::testing::ElementsAreArray(expected_values));
}

TEST_F(MigrationManagerTest, ToCanonicalCookiesTest) {
  cobalt::storage::Storage storage;
  std::vector<cobalt::storage::Cookie*> source_cookies;
  auto* cookie1 = storage.add_cookies();
  cookie1->set_name("name");
  cookie1->set_value("value");
  cookie1->set_domain(".example.com");
  cookie1->set_path("/path");
  cookie1->set_creation_time_us(1);
  cookie1->set_expiration_time_us(3);
  cookie1->set_last_access_time_us(5);
  cookie1->set_secure(true);
  cookie1->set_http_only(true);
  source_cookies.push_back(cookie1);

  auto* cookie2 = storage.add_cookies();
  cookie2->set_name("name2");
  cookie2->set_value("value2");
  cookie2->set_domain(".example2.com");
  cookie2->set_path("/path2");
  cookie2->set_creation_time_us(7);
  cookie2->set_expiration_time_us(11);
  cookie2->set_last_access_time_us(13);
  cookie2->set_secure(true);
  cookie2->set_http_only(true);
  source_cookies.push_back(cookie2);

  auto cookies = ToCanonicalCookies(storage);
  EXPECT_EQ(2u, cookies.size());
  for (size_t i = 0; i < 2; i++) {
    EXPECT_EQ(source_cookies[i]->name(), cookies[i]->Name());
    EXPECT_EQ(source_cookies[i]->value(), cookies[i]->Value());
    EXPECT_EQ(source_cookies[i]->domain(), cookies[i]->Domain());
    EXPECT_EQ(source_cookies[i]->path(), cookies[i]->Path());
    EXPECT_EQ(
        source_cookies[i]->creation_time_us(),
        cookies[i]->CreationDate().ToDeltaSinceWindowsEpoch().InMicroseconds());
    EXPECT_EQ(
        source_cookies[i]->expiration_time_us(),
        cookies[i]->ExpiryDate().ToDeltaSinceWindowsEpoch().InMicroseconds());
    EXPECT_EQ(source_cookies[i]->last_access_time_us(),
              cookies[i]
                  ->LastAccessDate()
                  .ToDeltaSinceWindowsEpoch()
                  .InMicroseconds());
    EXPECT_EQ(source_cookies[i]->secure(), cookies[i]->IsSecure());
    EXPECT_EQ(source_cookies[i]->http_only(), cookies[i]->IsHttpOnly());
    EXPECT_EQ(net::CookieSameSite::NO_RESTRICTION, cookies[i]->SameSite());
    EXPECT_TRUE(cookies[i]->Priority());
    EXPECT_FALSE(cookies[i]->IsPartitioned());
    EXPECT_FALSE(cookies[i]->PartitionKey().has_value());
    EXPECT_EQ(net::CookieSourceScheme::kUnset, cookies[i]->SourceScheme());
    EXPECT_EQ(-1, cookies[i]->SourcePort());
    EXPECT_TRUE(cookies[i]->IsDomainCookie());
    EXPECT_FALSE(cookies[i]->IsHostCookie());
  }
}

TEST_F(MigrationManagerTest, ToCanonicalCookiesWithLeadingDotTest) {
  cobalt::storage::Storage storage;
  auto* cookie = storage.add_cookies();
  cookie->set_name("name");
  cookie->set_value("value");
  cookie->set_domain(".example.com");
  cookie->set_path("/");

  auto cookies = ToCanonicalCookies(storage);
  EXPECT_EQ(1u, cookies.size());
  EXPECT_EQ(".example.com", cookies[0]->Domain());
  EXPECT_TRUE(cookies[0]->IsDomainCookie());
}

TEST_F(MigrationManagerTest, ToLocalStorageItemsTest) {
  cobalt::storage::Storage storage;

  auto* local_storage1 = storage.add_local_storages();
  local_storage1->set_serialized_origin("https://example1.com");
  auto* local_storage1_entry1 = local_storage1->add_local_storage_entries();
  local_storage1_entry1->set_key("local_storage1_entry1_key");
  local_storage1_entry1->set_value("local_storage1_entry1_value");
  auto* local_storage1_entry2 = local_storage1->add_local_storage_entries();
  local_storage1_entry2->set_key("local_storage1_entry2_key");
  local_storage1_entry2->set_value("local_storage1_entry2_value");

  auto* local_storage2 = storage.add_local_storages();
  local_storage2->set_serialized_origin("https://example2.com");
  auto* local_storage2_entry1 = local_storage2->add_local_storage_entries();
  local_storage2_entry1->set_key("local_storage2_entry1_key");
  local_storage2_entry1->set_value("local_storage2_entry1_value");
  auto* local_storage2_entry2 = local_storage2->add_local_storage_entries();
  local_storage2_entry2->set_key("local_storage2_entry2_key");
  local_storage2_entry2->set_value("local_storage2_entry2_value");

  auto* local_storage3 = storage.add_local_storages();
  local_storage3->set_serialized_origin("http://example2.com");
  auto* local_storage3_entry1 = local_storage3->add_local_storage_entries();
  local_storage3_entry1->set_key("local_storage3_entry1_key");
  local_storage3_entry1->set_value("local_storage3_entry1_value");
  auto* local_storage3_entry2 = local_storage3->add_local_storage_entries();
  local_storage3_entry2->set_key("local_storage3_entry2_key");
  local_storage3_entry2->set_value("local_storage3_entry2_value");

  auto* local_storage4 = storage.add_local_storages();
  local_storage4->set_serialized_origin("https://example1.com");
  auto* local_storage4_entry1 = local_storage4->add_local_storage_entries();
  local_storage4_entry1->set_key("local_storage4_entry1_key");
  local_storage4_entry1->set_value("local_storage4_entry1_value");
  auto* local_storage4_entry2 = local_storage4->add_local_storage_entries();
  local_storage4_entry2->set_key("local_storage4_entry2_key");
  local_storage4_entry2->set_value("local_storage4_entry2_value");

  auto actual1 = ToLocalStorageItems(
      url::Origin::Create(GURL("https://example1.com")), storage);
  EXPECT_EQ(4u, actual1.size());
  EXPECT_EQ("local_storage1_entry1_key", actual1[0]->first);
  EXPECT_EQ("local_storage1_entry1_value", actual1[0]->second);
  EXPECT_EQ("local_storage1_entry2_key", actual1[1]->first);
  EXPECT_EQ("local_storage1_entry2_value", actual1[1]->second);
  EXPECT_EQ("local_storage4_entry1_key", actual1[2]->first);
  EXPECT_EQ("local_storage4_entry1_value", actual1[2]->second);
  EXPECT_EQ("local_storage4_entry2_key", actual1[3]->first);
  EXPECT_EQ("local_storage4_entry2_value", actual1[3]->second);

  auto actual2 = ToLocalStorageItems(
      url::Origin::Create(GURL("https://example2.com")), storage);
  EXPECT_EQ(2u, actual2.size());
  EXPECT_EQ("local_storage2_entry1_key", actual2[0]->first);
  EXPECT_EQ("local_storage2_entry1_value", actual2[0]->second);
  EXPECT_EQ("local_storage2_entry2_key", actual2[1]->first);
  EXPECT_EQ("local_storage2_entry2_value", actual2[1]->second);

  auto actual3 = ToLocalStorageItems(
      url::Origin::Create(GURL("http://example2.com")), storage);
  EXPECT_TRUE(actual3.empty());
}

}  // namespace migrate_storage_record
}  // namespace cobalt
