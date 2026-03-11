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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/common/shell_switches.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie.h"
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

using StorageAreaRemote = mojo::Remote<blink::mojom::StorageArea>;

namespace {

struct SharedClosureState : public base::RefCounted<SharedClosureState> {
  base::OnceClosure cb;
  void Run() {
    if (cb) {
      std::move(cb).Run();
    }
  }

 private:
  friend class base::RefCounted<SharedClosureState>;
  ~SharedClosureState() = default;
};

network::mojom::CookieManager* GetCookieManager(
    content::StoragePartition* partition) {
  return partition->GetCookieManagerForBrowserProcess();
}

void GetLocalStorageArea(content::StoragePartition* partition,
                         const url::Origin& origin,
                         mojo::Remote<blink::mojom::StorageArea>& out_area) {
  auto storage_key = blink::StorageKey::CreateFirstParty(origin);

  LOG(INFO) << "ColinL: [Migration-V10-Verify] Binding LocalStorage";
  LOG(INFO) << "ColinL:   Origin: " << origin.Serialize();
  LOG(INFO) << "ColinL:   StorageKey Debug: " << storage_key.GetDebugString();
  LOG(INFO) << "ColinL:   SerializedKey: "
            << storage_key.SerializeForLocalStorage();

  partition->GetLocalStorageControl()->BindStorageArea(
      storage_key, out_area.BindNewPipeAndPassReceiver());
}

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

std::unique_ptr<cobalt::storage::Storage> ReadStorage() {
  constexpr char kRecordHeader[] = "SAV1";
  constexpr size_t kRecordHeaderSize = 4;

  StorageReadResult result = StorageReadResult::kSuccess;
  std::unique_ptr<cobalt::storage::Storage> storage;

  [&]() {
    GURL initial_url(
        switches::GetInitialURL(*base::CommandLine::ForCurrentProcess()));
    if (!initial_url.is_valid()) {
      result = StorageReadResult::kRecordInvalid;
      return;
    }

    std::string partition_key = GetApplicationKey(initial_url);
    auto record =
        std::make_unique<starboard::StorageRecord>(partition_key.c_str());
    if (!record->IsValid()) {
      record->Delete();
      bool fallback = partition_key == GetApplicationKey(GURL(kDefaultURL));
      if (!fallback) {
        result = StorageReadResult::kPartitionKeyNotDefault;
        return;
      }

      record = std::make_unique<starboard::StorageRecord>();
      if (!record->IsValid()) {
        record->Delete();
        result = StorageReadResult::kRecordInvalid;
        return;
      }
    }

    if (record->GetSize() < kRecordHeaderSize) {
      record->Delete();
      result = StorageReadResult::kSizeTooSmall;
      return;
    }

    auto bytes = std::vector<uint8_t>(record->GetSize());
    const int read_result =
        record->Read(reinterpret_cast<char*>(bytes.data()), bytes.size());

    LOG(INFO) << "ColinL: Read " << read_result
              << " bytes from legacy storage.";

    if (!record->Delete()) {
      LOG(ERROR)
          << "Failed to delete legacy storage record. Aborting migration "
             "to prevent overwrite loop.";
      result = StorageReadResult::kDeleteFailed;
      return;
    }
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

  if (storage) {
    for (const auto& local_storage_group : storage->local_storages()) {
      LOG(INFO) << "ColinL: Found legacy LocalStorage group for origin: "
                << local_storage_group.serialized_origin();
    }

    LOG(INFO) << "ColinL: Successfully parsed storage. Cookies: "
              << storage->cookies_size()
              << ", LocalStorage groups: " << storage->local_storages_size();
  } else {
    LOG(INFO) << "ColinL: Legacy storage was not found or failed to parse.";
  }
  return storage;
}

Task RecordMetricsTask(std::unique_ptr<base::ElapsedTimer> elapsed_timer) {
  return base::BindOnce(
      [](std::unique_ptr<base::ElapsedTimer> elapsed_timer,
         base::OnceClosure callback) {
        base::TimeDelta elapsed = elapsed_timer->Elapsed();
        base::UmaHistogramTimes(kDurationHistogram, elapsed);
#if !defined(COBALT_IS_RELEASE_BUILD)
        LOG(INFO) << "Cookie and localStorage migration completed in "
                  << elapsed.InMilliseconds() << " ms.";
#endif  // !defined(COBALT_IS_RELEASE_BUILD)
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

base::FilePath GetMigrationSentinelPath() {
  base::FilePath current_cache_path;
  if (!base::PathService::Get(base::DIR_CACHE, &current_cache_path)) {
    return base::FilePath();
  }
  return current_cache_path.Append("migration_completed.txt");
}

void WriteMigrationSentinelAsync() {
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
      ->PostTask(FROM_HERE, base::BindOnce([]() {
                   base::FilePath sentinel_path = GetMigrationSentinelPath();
                   if (!sentinel_path.empty()) {
                     base::WriteFile(sentinel_path, "1");
                   }
                 }));
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
    WriteMigrationSentinelAsync();
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

void MigrationManager::RunMigration(content::StoragePartition* partition,
                                    base::OnceClosure done_callback) {
  LOG(INFO) << "ColinL: Starting Pre-MainLoop Migration.";

  auto elapsed_timer = std::make_unique<base::ElapsedTimer>();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableStorageMigration)) {
    LOG(INFO) << "ColinL: Storage migration disabled via switch.";
    LOG(INFO) << "ColinL: RunMigration early exit took "
              << elapsed_timer->Elapsed().InMilliseconds() << " ms.";
    std::move(done_callback).Run();
    return;
  }

  // Fast check: if the sentinel file exists, migration is already done.
  base::ElapsedTimer sentinel_timer;
  base::FilePath sentinel_path = GetMigrationSentinelPath();
  bool sentinel_exists =
      !sentinel_path.empty() && base::PathExists(sentinel_path);
  LOG(INFO) << "ColinL: Checking sentinel file took "
            << sentinel_timer.Elapsed().InMilliseconds() << " ms.";
  if (sentinel_exists) {
    LOG(INFO) << "ColinL: Migration sentinel file found. Skipping migration.";
    LOG(INFO) << "ColinL: RunMigration fast path took "
              << elapsed_timer->Elapsed().InMilliseconds() << " ms.";
    std::move(done_callback).Run();
    return;
  }

  GURL initial_url(
      switches::GetInitialURL(*base::CommandLine::ForCurrentProcess()));
  if (!initial_url.is_valid()) {
    LOG(INFO) << "ColinL: RunMigration invalid URL early exit took "
              << elapsed_timer->Elapsed().InMilliseconds() << " ms.";
    std::move(done_callback).Run();
    return;
  }
  url::Origin origin = url::Origin::Create(initial_url);

  auto storage = ReadStorage();
  if (!storage ||
      (storage->cookies_size() == 0 && storage->local_storages_size() == 0)) {
    LOG(INFO) << "ColinL: Nothing to migrate.";
    LOG(INFO) << "ColinL: RunMigration fast path took "
              << elapsed_timer->Elapsed().InMilliseconds() << " ms.";
    std::move(done_callback).Run();
    return;
  }

  base::UmaHistogramCounts1000(kCookiesCountHistogram, storage->cookies_size());
  base::UmaHistogramCounts1000(kLocalStorageCountHistogram,
                               storage->local_storages_size());

  LOG(INFO) << "ColinL: RunMigration synchronous setup took "
            << elapsed_timer->Elapsed().InMilliseconds() << " ms.";

  std::vector<Task> tasks;
  tasks.push_back(CookieTask(partition, ToCanonicalCookies(*storage)));
  tasks.push_back(LocalStorageTask(partition, origin,
                                   ToLocalStorageItems(origin, *storage)));
  tasks.push_back(RecordMetricsTask(std::move(elapsed_timer)));
  tasks.push_back(DeleteOldCacheDirectoryTask());

  std::move(GroupTasks(std::move(tasks))).Run(std::move(done_callback));
}

// static
Task MigrationManager::CookieTask(
    content::StoragePartition* partition,
    std::vector<std::unique_ptr<net::CanonicalCookie>> cookies) {
  return base::BindOnce(
      [](content::StoragePartition* partition,
         std::vector<std::unique_ptr<net::CanonicalCookie>> cookies,
         base::OnceClosure callback) {
        if (cookies.empty()) {
          std::move(callback).Run();
          return;
        }

        auto* cookie_manager = GetCookieManager(partition);
        auto shared_state = base::MakeRefCounted<SharedClosureState>();
        shared_state->cb = std::move(callback);

        base::RepeatingClosure barrier = base::BarrierClosure(
            cookies.size(), base::BindOnce(
                                [](scoped_refptr<SharedClosureState> state) {
                                  if (state->cb) {
                                    LOG(INFO)
                                        << "ColinL: Cookie injection complete.";
                                    state->Run();
                                  }
                                },
                                shared_state));

        for (auto& cookie : cookies) {
          std::string name = cookie->Name();
          GURL source_url("https://" + cookie->Domain() + cookie->Path());
          cookie_manager->SetCanonicalCookie(
              *cookie, source_url, net::CookieOptions::MakeAllInclusive(),
              base::BindOnce(
                  [](base::RepeatingClosure barrier, std::string cookie_name,
                     net::CookieAccessResult result) { barrier.Run(); },
                  barrier, name));
        }

        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                [](scoped_refptr<SharedClosureState> state) {
                  if (state->cb) {
                    LOG(ERROR) << "ColinL: Cookie injection timed out after 2 "
                                  "seconds. Proceeding anyway.";
                    state->Run();
                  }
                },
                shared_state),
            base::Seconds(2));
      },
      partition, std::move(cookies));
}

namespace {

std::vector<uint8_t> FormatStringForLocalStorage(const std::string& input) {
  std::u16string utf16_str = base::UTF8ToUTF16(input);
  bool is_latin1 = true;
  for (char16_t c : utf16_str) {
    if (c > 0xFF) {
      is_latin1 = false;
      break;
    }
  }

  std::vector<uint8_t> result;
  if (is_latin1) {
    result.reserve(utf16_str.size() + 1);
    result.push_back(1);  // StorageFormat::Latin1
    for (char16_t c : utf16_str) {
      result.push_back(static_cast<uint8_t>(c));
    }
  } else {
    result.reserve(utf16_str.size() * sizeof(char16_t) + 1);
    result.push_back(0);  // StorageFormat::UTF16
    const uint8_t* chars = reinterpret_cast<const uint8_t*>(utf16_str.data());
    result.insert(result.end(), chars,
                  chars + utf16_str.size() * sizeof(char16_t));
  }
  return result;
}
}  // namespace

// static
Task MigrationManager::LocalStorageTask(
    content::StoragePartition* partition,
    const url::Origin& origin,
    std::vector<std::unique_ptr<std::pair<std::string, std::string>>> pairs) {
  return base::BindOnce(
      [](content::StoragePartition* partition, url::Origin origin,
         std::vector<std::unique_ptr<std::pair<std::string, std::string>>>
             pairs,
         base::OnceClosure callback) {
        if (pairs.empty()) {
          std::move(callback).Run();
          return;
        }

        auto area = std::make_unique<mojo::Remote<blink::mojom::StorageArea>>();
        GetLocalStorageArea(partition, origin, *area);
        auto* raw_remote_ptr = area.get();

        // 3. Purge memory handles step
        base::OnceClosure purge_memory_step = base::BindOnce(
            [](std::unique_ptr<mojo::Remote<blink::mojom::StorageArea>> area,
               content::StoragePartition* partition, base::OnceClosure cb) {
              // Force the Storage Service to drop its handle for this origin,
              // so that the Renderer process will fetch fresh data.
              LOG(INFO) << "ColinL: Purging Storage Service handles to force "
                           "Renderer synchronization.";
              partition->GetLocalStorageControl()->PurgeMemory();
              std::move(cb).Run();
            },
            std::move(area), base::Unretained(partition), std::move(callback));

        // 2. Flush step
        base::OnceClosure flush_step = base::BindOnce(
            [](content::StoragePartition* partition,
               base::OnceClosure next_step) {
              LOG(INFO) << "ColinL: LocalStorage Puts complete. Flushing...";

              auto shared_state = base::MakeRefCounted<SharedClosureState>();
              shared_state->cb = std::move(next_step);

              partition->GetLocalStorageControl()->Flush(base::BindOnce(
                  [](scoped_refptr<SharedClosureState> state) {
                    if (state->cb) {
                      LOG(INFO)
                          << "ColinL: LocalStorage Flush complete callback "
                             "executed successfully.";
                      state->Run();
                    }
                  },
                  shared_state));

              base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                  FROM_HERE,
                  base::BindOnce(
                      [](scoped_refptr<SharedClosureState> state) {
                        if (state->cb) {
                          LOG(ERROR) << "ColinL: LocalStorage Flush timed out "
                                        "after 2 seconds. Proceeding anyway.";
                          state->Run();
                        }
                      },
                      shared_state),
                  base::Seconds(2));
            },
            base::Unretained(partition), std::move(purge_memory_step));

        // 1. Put keys step
        base::RepeatingClosure barrier =
            base::BarrierClosure(pairs.size(), std::move(flush_step));

        for (const auto& pair : pairs) {
          std::string key_str = pair->first;
          std::vector<uint8_t> key = FormatStringForLocalStorage(pair->first);
          std::vector<uint8_t> value =
              FormatStringForLocalStorage(pair->second);

          std::string print_val = pair->second;
          if (print_val.length() > 40) {
            print_val = print_val.substr(0, 40) + "...";
          }
          LOG(INFO) << "ColinL: Initiating Put for key: " << key_str
                    << ", value: " << print_val;

          (*raw_remote_ptr)
              ->Put(key, value, absl::nullopt, "migration",
                    base::BindOnce(
                        [](base::RepeatingClosure barrier, std::string key_str,
                           bool success) {
                          if (success) {
                            LOG(INFO)
                                << "ColinL: Put SUCCESS for key: " << key_str;
                          } else {
                            LOG(ERROR)
                                << "ColinL: Put FAILED for key: " << key_str;
                          }
                          barrier.Run();
                        },
                        barrier, key_str));
        }
      },
      partition, origin, std::move(pairs));
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
MigrationManager::ToLocalStorageItems(const url::Origin& page_origin,
                                      const cobalt::storage::Storage& storage) {
  std::vector<std::unique_ptr<std::pair<std::string, std::string>>> entries;
  for (const auto& local_storages : storage.local_storages()) {
    GURL local_storage_origin(local_storages.serialized_origin());
    if (!local_storage_origin.SchemeIs("https") ||
        !page_origin.IsSameOriginWith(local_storage_origin)) {
      continue;
    }

    for (const auto& local_storage_entry :
         local_storages.local_storage_entries()) {
      entries.emplace_back(new std::pair<std::string, std::string>(
          local_storage_entry.key(), local_storage_entry.value()));
    }
  }
  return entries;
}

}  // namespace migrate_storage_record
}  // namespace cobalt
