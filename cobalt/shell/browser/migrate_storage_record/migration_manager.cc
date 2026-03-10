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

#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/common/shell_switches.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "starboard/common/storage.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace migrate_storage_record {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Tracks the outcome of attempting to read the legacy Starboard storage record.
enum class StorageReadResult {
  kSuccess = 0,
  kPartitionKeyNotDefault = 1,
  kRecordInvalid = 2,
  kSizeTooSmall = 3,
  kDeleteFailed = 4,
  kReadMismatch = 5,
  kHeaderMismatch = 6,
  kParseError = 7,
  kMaxValue = kParseError,
};

// Tracks the outcome of individual data injection operations into the new
// Chromium-based storage partitions.
enum class InjectionResult {
  kSuccess = 0,
  kError = 1,
  kMaxValue = kError,
};

constexpr char kReadResultHistogram[] = "Cobalt.StorageMigration.ReadResult";
constexpr char kCookieInjectionResultHistogram[] =
    "Cobalt.StorageMigration.CookieInjectionResult";
constexpr char kLocalStorageInjectionResultHistogram[] =
    "Cobalt.StorageMigration.LocalStorageInjectionResult";
constexpr char kCookiesCountHistogram[] =
    "Cobalt.StorageMigration.CookiesCount";
constexpr char kLocalStorageCountHistogram[] =
    "Cobalt.StorageMigration.LocalStorageCount";
constexpr char kDurationHistogram[] = "Cobalt.StorageMigration.Duration";

std::string GetApplicationKey(const GURL& url) {
  std::string encoded_url;
  base::Base64UrlEncode(url_matcher::util::Normalize(url).spec(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_url);
  return encoded_url;
}

std::unique_ptr<starboard::StorageRecord> GetStorageRecord() {
  GURL initial_url(
      switches::GetInitialURL(*base::CommandLine::ForCurrentProcess()));
  if (!initial_url.is_valid()) {
    return nullptr;
  }

  std::string partition_key = GetApplicationKey(initial_url);
  auto record =
      std::make_unique<starboard::StorageRecord>(partition_key.c_str());
  if (!record->IsValid()) {
    bool fallback = partition_key == GetApplicationKey(GURL(kDefaultURL));
    if (!fallback) {
      return nullptr;
    }

    record = std::make_unique<starboard::StorageRecord>();
    if (!record->IsValid()) {
      return nullptr;
    }
  }
  return record;
}

std::unique_ptr<cobalt::storage::Storage> ReadStorage(starboard::StorageRecord* record) {
  constexpr char kRecordHeader[] = "SAV1";
  constexpr size_t kRecordHeaderSize = 4;

  StorageReadResult result = StorageReadResult::kSuccess;
  std::unique_ptr<cobalt::storage::Storage> storage;

  [&]() {
    if (!record || !record->IsValid()) {
      result = StorageReadResult::kRecordInvalid;
      return;
    }

    if (record->GetSize() < kRecordHeaderSize) {
      result = StorageReadResult::kSizeTooSmall;
      return;
    }

    auto bytes = std::vector<uint8_t>(record->GetSize());
    const int read_result =
        record->Read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (read_result != bytes.size()) {
      result = StorageReadResult::kReadMismatch;
      return;
    }

    std::string version(reinterpret_cast<const char*>(bytes.data()),
                        kRecordHeaderSize);
    if (version != kRecordHeader) {
      result = StorageReadResult::kHeaderMismatch;
      return;
    }

    storage = std::make_unique<cobalt::storage::Storage>();
    if (!storage->ParseFromArray(
            reinterpret_cast<const char*>(bytes.data() + kRecordHeaderSize),
            bytes.size() - kRecordHeaderSize)) {
      result = StorageReadResult::kParseError;
      storage.reset();
      return;
    }
  }();

  base::UmaHistogramEnumeration(kReadResultHistogram, result);
  return storage;
}

Task RecordMetricsTask(std::unique_ptr<base::ElapsedTimer> elapsed_timer) {
  return base::BindOnce(
      [](std::unique_ptr<base::ElapsedTimer> elapsed_timer,
         base::OnceClosure callback) {
        base::TimeDelta elapsed = elapsed_timer->Elapsed();
        base::UmaHistogramTimes(kDurationHistogram, elapsed);
        std::move(callback).Run();
      },
      std::move(elapsed_timer));
}

base::FilePath GetOldCachePath() {
  std::vector<char> path(kSbFileMaxPath, 0);
  bool success =
      SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(), path.size());
  if (!success) {
    return base::FilePath();
  }
  return base::FilePath(path.data());
}

void DeleteOldCacheDirectoryAsync() {
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
      ->PostTask(
          FROM_HERE, base::BindOnce([]() {
            base::FilePath old_cache_path = GetOldCachePath();
            if (old_cache_path.empty() || !base::PathExists(old_cache_path)) {
              return;
            }

            base::FilePath current_cache_path;
            base::PathService::Get(base::DIR_CACHE, &current_cache_path);
            if (!old_cache_path.IsParent(current_cache_path) &&
                old_cache_path != current_cache_path) {
              base::DeletePathRecursively(old_cache_path);
              return;
            }

            std::vector<std::string> old_cache_subpaths = {
                "cache_settings.json",
                "compiled_js",
                "css",
                "font",
                "html",
                "image",
                "other",
                "service_worker_settings.json",
                "settings.json",
                "splash",
                "splash_screen",
                "uncompiled_js",
            };
            for (const auto& subpath : old_cache_subpaths) {
              base::FilePath old_cache_subpath = old_cache_path.Append(subpath);
              if (base::PathExists(old_cache_subpath)) {
                base::DeletePathRecursively(old_cache_subpath);
              }
            }
          }));
}

Task DeleteOldCacheDirectoryTask() {
  return base::BindOnce([](base::OnceClosure callback) {
    DeleteOldCacheDirectoryAsync();
    std::move(callback).Run();
  });
}

}  // namespace

// static
std::atomic_flag MigrationManager::migration_attempted_ = ATOMIC_FLAG_INIT;

// static
Task MigrationManager::GroupTasks(std::vector<Task> tasks) {
  return base::BindOnce(
      [](std::vector<Task> tasks, base::OnceClosure callback) {
        base::OnceClosure all_tasks_closure = std::move(callback);
        for (Task& task : base::Reversed(tasks)) {
          all_tasks_closure =
              base::BindOnce(std::move(task), std::move(all_tasks_closure));
        }
        std::move(all_tasks_closure).Run();
      },
      std::move(tasks));
}

// TODO(b/399165612): Add metrics.
// TODO(b/399165796): Disable and delete migration code when possible.
// TODO(b/399166308): Add unit and/or integration tests for migration.
void MigrationManager::DoMigrationTasksOnce(
    content::BrowserContext* browser_context) {
  if (migration_attempted_.test_and_set()) {
    return;
  }

  auto elapsed_timer = std::make_unique<base::ElapsedTimer>();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableStorageMigration)) {
    LOG(INFO) << "Storage migration disabled via switch.";
    return;
  }

  auto record = GetStorageRecord();
  if (!record) {
    return;
  }

  auto storage = ReadStorage(record.get());
  if (!storage ||
      (storage->cookies_size() == 0 && storage->local_storages_size() == 0)) {
    record->Delete();
    return;
  }

  base::UmaHistogramCounts1000(kCookiesCountHistogram, storage->cookies_size());
  base::UmaHistogramCounts1000(kLocalStorageCountHistogram,
                               storage->local_storages_size());

  std::vector<Task> tasks;
  tasks.push_back(CookieTask(browser_context, ToCanonicalCookies(*storage)));

  for (const auto& local_storage : storage->local_storages()) {
    GURL origin_url(local_storage.serialized_origin());
    if (origin_url.is_valid() && origin_url.SchemeIs("https")) {
      url::Origin origin = url::Origin::Create(origin_url);
      tasks.push_back(LocalStorageTask(browser_context, origin,
                                       ToLocalStorageItems(local_storage)));
    }
  }

  tasks.push_back(RecordMetricsTask(std::move(elapsed_timer)));
  tasks.push_back(DeleteOldCacheDirectoryTask());

  base::RunLoop run_loop;
  std::move(GroupTasks(std::move(tasks))).Run(run_loop.QuitClosure());
  run_loop.Run();

  if (!record->Delete()) {
    LOG(ERROR) << "Failed to delete legacy storage record after migration.";
  }
}

// static
Task MigrationManager::CookieTask(
    content::BrowserContext* browser_context,
    std::vector<std::unique_ptr<net::CanonicalCookie>> cookies) {
  auto* cookie_manager = browser_context->GetDefaultStoragePartition()
                             ->GetCookieManagerForBrowserProcess();
  std::vector<Task> tasks;
  tasks.push_back(base::BindOnce(
      [](network::mojom::CookieManager* cookie_manager,
         base::OnceClosure callback) {
        cookie_manager->DeleteCookies(
            network::mojom::CookieDeletionFilter::New(),
            base::IgnoreArgs<uint32_t>(std::move(callback)));
      },
      cookie_manager));
  for (auto& cookie : cookies) {
    tasks.push_back(base::BindOnce(
        [](network::mojom::CookieManager* cookie_manager,
           std::unique_ptr<net::CanonicalCookie> cookie,
           base::OnceClosure callback) {
          GURL source_url("https://" + cookie->Domain() + cookie->Path());
          cookie_manager->SetCanonicalCookie(
              *cookie, source_url, net::CookieOptions::MakeAllInclusive(),
              base::BindOnce(
                  [](base::OnceClosure callback,
                     net::CookieAccessResult result) {
                    bool success = result.status.IsInclude();
                    if (!success) {
                      LOG(WARNING)
                          << "Cookie injection failed: "
                          << result.status.GetDebugString();
                    }
                    base::UmaHistogramEnumeration(
                        kCookieInjectionResultHistogram,
                        success ? InjectionResult::kSuccess
                                : InjectionResult::kError);
                    std::move(callback).Run();
                  },
                  std::move(callback)));
        },
        cookie_manager, std::move(cookie)));
  }
  tasks.push_back(base::BindOnce(
      [](network::mojom::CookieManager* cookie_manager,
         base::OnceClosure callback) {
        cookie_manager->FlushCookieStore(std::move(callback));
      },
      cookie_manager));
  return GroupTasks(std::move(tasks));
}

// static
Task MigrationManager::LocalStorageTask(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    std::vector<std::unique_ptr<std::pair<std::string, std::string>>> pairs) {
  auto* storage_control = browser_context->GetDefaultStoragePartition()
                              ->GetLocalStorageControl();
  blink::StorageKey storage_key = blink::StorageKey::CreateFirstParty(origin);

  std::vector<Task> tasks;
  tasks.push_back(base::BindOnce(
      [](::storage::mojom::LocalStorageControl* storage_control,
         blink::StorageKey storage_key, base::OnceClosure callback) {
        storage_control->DeleteStorage(storage_key, std::move(callback));
      },
      storage_control, storage_key));

  for (auto& pair : pairs) {
    tasks.push_back(base::BindOnce(
        [](::storage::mojom::LocalStorageControl* storage_control,
           blink::StorageKey storage_key,
           std::unique_ptr<std::pair<std::string, std::string>> pair,
           base::OnceClosure callback) {
          mojo::Remote<blink::mojom::StorageArea> area;
          storage_control->BindStorageArea(storage_key,
                                           area.BindNewPipeAndPassReceiver());
          std::string ls_key = pair->first;
          std::string ls_value = pair->second;

          auto* area_ptr = area.get();
          area_ptr->Put(
              std::vector<uint8_t>(ls_key.begin(), ls_key.end()),
              std::vector<uint8_t>(ls_value.begin(), ls_value.end()),
              absl::nullopt, "migration",
              base::BindOnce(
                  [](mojo::Remote<blink::mojom::StorageArea> area,
                     base::OnceClosure callback, bool success) {
                    if (!success) {
                      LOG(WARNING)
                          << "LocalStorage injection failed.";
                    }
                    base::UmaHistogramEnumeration(
                        kLocalStorageInjectionResultHistogram,
                        success ? InjectionResult::kSuccess
                                : InjectionResult::kError);
                    std::move(callback).Run();
                  },
                  std::move(area), std::move(callback)));
        },
        storage_control, storage_key, std::move(pair)));
  }
  tasks.push_back(base::BindOnce(
      [](::storage::mojom::LocalStorageControl* storage_control,
         base::OnceClosure callback) {
        storage_control->Flush(std::move(callback));
      },
      storage_control));
  return GroupTasks(std::move(tasks));
}

// static
std::vector<std::unique_ptr<net::CanonicalCookie>>
MigrationManager::ToCanonicalCookies(const cobalt::storage::Storage& storage) {
  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies;
  for (auto& c : storage.cookies()) {
    cookies.push_back(net::CanonicalCookie::FromStorage(
        c.name(), c.value(), c.domain(), c.path(),
        base::Time::FromInternalValue(c.creation_time_us()),
        base::Time::FromInternalValue(c.expiration_time_us()),
        base::Time::FromInternalValue(c.last_access_time_us()),
        base::Time::FromInternalValue(c.creation_time_us()), true,
        c.http_only(), net::CookieSameSite::NO_RESTRICTION,
        net::COOKIE_PRIORITY_DEFAULT, false, absl::nullopt,
        net::CookieSourceScheme::kUnset, url::PORT_UNSPECIFIED));
  }
  return cookies;
}

// static
std::vector<std::unique_ptr<std::pair<std::string, std::string>>>
MigrationManager::ToLocalStorageItems(
    const cobalt::storage::LocalStorage& local_storage) {
  std::vector<std::unique_ptr<std::pair<std::string, std::string>>> entries;
  for (const auto& local_storage_entry :
       local_storage.local_storage_entries()) {
    entries.emplace_back(new std::pair<std::string, std::string>(
        local_storage_entry.key(), local_storage_entry.value()));
  }
  return entries;
}

}  // namespace migrate_storage_record
}  // namespace cobalt
