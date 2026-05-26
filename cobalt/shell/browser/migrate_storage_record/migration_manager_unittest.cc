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

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/migrate_storage_record/storage.pb.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class MockCookieManager : public network::TestCookieManager {
 public:
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& source_url,
                          const net::CookieOptions& cookie_options,
                          SetCanonicalCookieCallback callback) override {
    set_canonical_cookie_call_count_++;
    if (should_hang_) {
      hanging_callbacks_.push_back(std::move(callback));
      return;
    }
    if (callback) {
      net::CookieAccessResult result;
      if (should_fail_) {
        result.status.AddExclusionReason(
            net::CookieInclusionStatus::ExclusionReason::EXCLUDE_UNKNOWN_ERROR);
      }
      std::move(callback).Run(result);
    }
  }

  int set_canonical_cookie_call_count() const {
    return set_canonical_cookie_call_count_;
  }

  void set_should_fail(bool fail) { should_fail_ = fail; }
  void set_should_hang(bool hang) { should_hang_ = hang; }

 private:
  int set_canonical_cookie_call_count_ = 0;
  bool should_fail_ = false;
  bool should_hang_ = false;
  std::vector<SetCanonicalCookieCallback> hanging_callbacks_;
};

class MockStorageArea : public blink::mojom::StorageArea {
 public:
  MockStorageArea() = default;

  void Bind(mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void AddObserver(mojo::PendingRemote<blink::mojom::StorageAreaObserver>
                       observer) override {}
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           const absl::optional<std::vector<uint8_t>>& client_old_value,
           const std::string& source,
           PutCallback callback) override {
    put_call_count_++;
    if (should_hang_) {
      hanging_callbacks_.push_back(std::move(callback));
      return;
    }
    if (callback) {
      std::move(callback).Run(!should_fail_);
    }
  }
  void Delete(const std::vector<uint8_t>& key,
              const absl::optional<std::vector<uint8_t>>& client_old_value,
              const std::string& source,
              DeleteCallback callback) override {}
  void DeleteAll(
      const std::string& source,
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
      DeleteAllCallback callback) override {}
  void Get(const std::vector<uint8_t>& key, GetCallback callback) override {}
  void GetAll(
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
      GetAllCallback callback) override {}
  void Checkpoint() override {}

  int put_call_count() const { return put_call_count_; }
  void set_should_fail(bool fail) { should_fail_ = fail; }
  void set_should_hang(bool hang) { should_hang_ = hang; }

 private:
  int put_call_count_ = 0;
  bool should_fail_ = false;
  bool should_hang_ = false;
  std::vector<PutCallback> hanging_callbacks_;
  mojo::ReceiverSet<blink::mojom::StorageArea> receivers_;
};

class MockLocalStorageControl : public ::storage::mojom::LocalStorageControl {
 public:
  MockLocalStorageControl() = default;

  void BindStorageArea(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override {
    storage_area_.Bind(std::move(receiver));
  }
  void GetUsage(GetUsageCallback callback) override {}
  void DeleteStorage(const blink::StorageKey& storage_key,
                     DeleteStorageCallback callback) override {}
  void CleanUpStorage(CleanUpStorageCallback callback) override {}
  void Flush() override { flush_call_count_++; }
  void PurgeMemory() override { purge_memory_call_count_++; }
  void ApplyPolicyUpdates(std::vector<::storage::mojom::StoragePolicyUpdatePtr>
                              policy_updates) override {}
  void ForceKeepSessionState() override {}
  void NeedsFlushForTesting(NeedsFlushForTestingCallback callback) override {}

  int flush_call_count() const { return flush_call_count_; }
  int purge_memory_call_count() const { return purge_memory_call_count_; }
  int put_call_count() const { return storage_area_.put_call_count(); }

  void set_should_fail(bool fail) { storage_area_.set_should_fail(fail); }

 private:
  int flush_call_count_ = 0;
  int purge_memory_call_count_ = 0;
  MockStorageArea storage_area_;
};

class MockStoragePartition : public content::TestStoragePartition {
 public:
  MockStoragePartition() = default;

  network::mojom::CookieManager* GetCookieManagerForBrowserProcess() override {
    return &cookie_manager_;
  }

  ::storage::mojom::LocalStorageControl* GetLocalStorageControl() override {
    return &local_storage_control_;
  }

  MockCookieManager& cookie_manager() { return cookie_manager_; }
  MockLocalStorageControl& local_storage_control() {
    return local_storage_control_;
  }

 private:
  MockCookieManager cookie_manager_;
  MockLocalStorageControl local_storage_control_;
};

}  // namespace

namespace cobalt {
namespace migrate_storage_record {

class MigrationManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    MigrationManager::ResetMigrationStatusForTesting();
    base::FilePath cache_dir;
    base::PathService::Get(base::DIR_CACHE, &cache_dir);
    base::DeleteFile(cache_dir.Append("migration_completed.txt"));
  }

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

  Task CookieTask(content::StoragePartition* partition,
                  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies,
                  scoped_refptr<MigrationState> state) {
    return MigrationManager::CookieTask(partition, std::move(cookies), state);
  }

  Task LocalStorageTask(
      content::StoragePartition* partition,
      const url::Origin& origin,
      std::vector<std::unique_ptr<std::pair<std::string, std::string>>> pairs,
      scoped_refptr<MigrationState> state) {
    return MigrationManager::LocalStorageTask(partition, origin,
                                              std::move(pairs), state);
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedPathOverride cache_dir_override_{base::DIR_CACHE};
};

// --- General State and Utility Tests ---

TEST_F(MigrationManagerTest, MigrationStateGetOutcomeTest) {
  // Test no data to migrate.
  auto state = base::MakeRefCounted<MigrationState>();
  state->has_data_to_migrate = false;
  state->read_result = StorageReadResult::kSuccess;
  EXPECT_EQ(MigrationOutcome::kSkipped, state->GetOutcome());

  state->read_result = StorageReadResult::kRecordInvalid;
  EXPECT_EQ(MigrationOutcome::kSkipped, state->GetOutcome());

  state->read_result = StorageReadResult::kParseError;
  EXPECT_EQ(MigrationOutcome::kFailed, state->GetOutcome());

  // Test data to migrate.
  state->has_data_to_migrate = true;

  // Initial success state.
  EXPECT_EQ(MigrationOutcome::kSuccess, state->GetOutcome());

  // Error taking precedence over Success.
  state->UpdateCookieResult(InjectionResult::kError);
  EXPECT_EQ(MigrationOutcome::kFailed, state->GetOutcome());

  // Timeout taking precedence over Error.
  state->UpdateLocalStorageResult(InjectionResult::kTimeout);
  EXPECT_EQ(MigrationOutcome::kTimeout, state->GetOutcome());

  // Verify that an attempt to log a Success doesn't downgrade a Timeout.
  state->UpdateCookieResult(InjectionResult::kSuccess);
  state->UpdateLocalStorageResult(InjectionResult::kSuccess);
  EXPECT_EQ(MigrationOutcome::kTimeout, state->GetOutcome());
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
    EXPECT_EQ(net::CookieSourceScheme::kSecure, cookies[i]->SourceScheme());
    EXPECT_EQ(443, cookies[i]->SourcePort());
    EXPECT_TRUE(cookies[i]->IsDomainCookie());
    EXPECT_FALSE(cookies[i]->IsHostCookie());
  }
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

TEST_F(MigrationManagerTest, VerifyGroupTasksRunsCallbacksSequentially) {
  constexpr uint32_t kTaskCount = 100;
  std::vector<Task> tasks;
  std::vector<int> collected_values;
  for (uint32_t i = 0; i < kTaskCount; i++) {
    tasks.push_back(base::BindOnce(
        [](std::vector<int>& collected_values, uint32_t i,
           base::OnceClosure callback) {
          collected_values.push_back(static_cast<int>(i));
          std::move(callback).Run();
        },
        std::ref(collected_values), i));
  }
  EXPECT_TRUE(collected_values.empty());
  Task grouped_task = GroupTasks(std::move(tasks));
  std::move(grouped_task).Run(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  std::vector<int> expected_values(static_cast<size_t>(kTaskCount));
  std::iota(expected_values.begin(), expected_values.end(), 0);
  EXPECT_THAT(collected_values, ::testing::ElementsAreArray(expected_values));
}

// --- Cookie Task Tests ---

TEST_F(MigrationManagerTest, CookieTaskTest) {
  MockStoragePartition partition;
  auto state = base::MakeRefCounted<MigrationState>();

  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies;
  cookies.push_back(net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "name", "value", ".example.com", "/path", base::Time(), base::Time(),
      base::Time(), base::Time(), true, true,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT));

  Task task = CookieTask(static_cast<content::StoragePartition*>(&partition),
                         std::move(cookies), state);

  base::RunLoop run_loop;
  std::move(task).Run(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(InjectionResult::kSuccess, state->cookie_result.load());
  EXPECT_EQ(1, partition.cookie_manager().set_canonical_cookie_call_count());
}

TEST_F(MigrationManagerTest, CookieTaskFailureTest) {
  MockStoragePartition partition;
  partition.cookie_manager().set_should_fail(true);
  auto state = base::MakeRefCounted<MigrationState>();

  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies;
  cookies.push_back(net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "name", "value", ".example.com", "/path", base::Time(), base::Time(),
      base::Time(), base::Time(), true, true,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT));

  Task task = CookieTask(static_cast<content::StoragePartition*>(&partition),
                         std::move(cookies), state);

  base::RunLoop run_loop;
  std::move(task).Run(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(InjectionResult::kError, state->cookie_result.load());
}

TEST_F(MigrationManagerTest, CookieTaskTimeoutTest) {
  MockStoragePartition partition;
  partition.cookie_manager().set_should_hang(true);
  auto state = base::MakeRefCounted<MigrationState>();

  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies;
  cookies.push_back(net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "name", "value", ".example.com", "/path", base::Time(), base::Time(),
      base::Time(), base::Time(), true, true,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT));

  Task task = CookieTask(static_cast<content::StoragePartition*>(&partition),
                         std::move(cookies), state);

  base::RunLoop run_loop;
  std::move(task).Run(run_loop.QuitClosure());

  task_environment().FastForwardBy(base::Seconds(3));

  EXPECT_EQ(InjectionResult::kTimeout, state->cookie_result.load());
}

// --- Local Storage Task Tests ---

TEST_F(MigrationManagerTest, LocalStorageTaskTest) {
  MockStoragePartition partition;
  auto state = base::MakeRefCounted<MigrationState>();

  std::vector<std::unique_ptr<std::pair<std::string, std::string>>> pairs;
  pairs.push_back(
      std::make_unique<std::pair<std::string, std::string>>("key1", "value1"));
  pairs.push_back(
      std::make_unique<std::pair<std::string, std::string>>("key2", "value2"));

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  Task task =
      LocalStorageTask(static_cast<content::StoragePartition*>(&partition),
                       origin, std::move(pairs), state);

  base::RunLoop run_loop;
  std::move(task).Run(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(InjectionResult::kSuccess, state->local_storage_result.load());
  EXPECT_EQ(2, partition.local_storage_control().put_call_count());
  EXPECT_EQ(1, partition.local_storage_control().flush_call_count());
}

TEST_F(MigrationManagerTest, LocalStorageTaskFailureTest) {
  MockStoragePartition partition;
  partition.local_storage_control().set_should_fail(true);
  auto state = base::MakeRefCounted<MigrationState>();

  std::vector<std::unique_ptr<std::pair<std::string, std::string>>> pairs;
  pairs.push_back(
      std::make_unique<std::pair<std::string, std::string>>("key1", "value1"));

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  Task task =
      LocalStorageTask(static_cast<content::StoragePartition*>(&partition),
                       origin, std::move(pairs), state);

  base::RunLoop run_loop;
  std::move(task).Run(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(InjectionResult::kError, state->local_storage_result.load());
}

}  // namespace migrate_storage_record
}  // namespace cobalt
