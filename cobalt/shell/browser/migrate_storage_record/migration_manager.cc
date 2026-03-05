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
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/common/shell_switches.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "starboard/common/storage.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "url/gurl.h"

#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/dom_storage_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

namespace cobalt {
namespace migrate_storage_record {

using StorageAreaRemote = mojo::Remote<blink::mojom::StorageArea>;

namespace {

// Helper to get CookieManager from Partition
network::mojom::CookieManager* GetCookieManager(
    content::StoragePartition* partition) {
  return partition->GetCookieManagerForBrowserProcess();
}

// Helper to get LocalStorage Area for a specific origin

void GetLocalStorageArea(content::StoragePartition* partition,
                         const url::Origin& origin,
                         StorageAreaRemote& out_area) {
  // 1. Get the Local Storage Control from the partition's Storage Service
  // This bypasses the limited DOMStorageContext interface.
  partition->GetLocalStorageControl()->BindStorageArea(
      blink::StorageKey::CreateFirstParty(origin),
      out_area.BindNewPipeAndPassReceiver());
}

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

  GURL initial_url(
      switches::GetInitialURL(*base::CommandLine::ForCurrentProcess()));
  CHECK(initial_url.is_valid());
  std::string partition_key = GetApplicationKey(initial_url);
  LOG(INFO) << "ColinL: ReadStorage attempting to open key: " << partition_key;

  auto record =
      std::make_unique<starboard::StorageRecord>(partition_key.c_str());
  if (!record->IsValid()) {
    LOG(INFO)
        << "ColinL: StorageRecord invalid for partition, trying fallback.";
    record->Delete();
    bool fallback = partition_key == GetApplicationKey(GURL(kDefaultURL));
    if (!fallback) {
      LOG(WARNING) << "ColinL: No fallback available. ReadStorage failed.";
      return nullptr;
    }

    record = std::make_unique<starboard::StorageRecord>();
    if (!record->IsValid()) {
      LOG(ERROR) << "ColinL: Fallback StorageRecord also invalid.";
      record->Delete();
      return nullptr;
    }
  }

  if (record->GetSize() < kRecordHeaderSize) {
    LOG(WARNING) << "ColinL: Record size too small: " << record->GetSize();
    record->Delete();
    return nullptr;
  }

  auto bytes = std::vector<uint8_t>(record->GetSize());
  const int read_result =
      record->Read(reinterpret_cast<char*>(bytes.data()), bytes.size());

  LOG(INFO) << "ColinL: Read " << read_result << " bytes from legacy storage.";

  if (!record->Delete()) {
    LOG(ERROR) << "Failed to delete legacy storage record. Aborting migration "
                  "to prevent overwrite loop.";
    return nullptr;
  }
  if (read_result != bytes.size()) {
    return nullptr;
  }

  std::string version(reinterpret_cast<const char*>(bytes.data()),
                      kRecordHeaderSize);
  if (version != kRecordHeader) {
    LOG(ERROR) << "ColinL: Version mismatch. Expected SAV1, got: " << version;
    return nullptr;
  }

  auto storage = std::make_unique<cobalt::storage::Storage>();
  if (!storage->ParseFromArray(
          reinterpret_cast<const char*>(bytes.data() + kRecordHeaderSize),
          bytes.size() - kRecordHeaderSize)) {
    LOG(ERROR) << "ColinL: Failed to parse Storage protobuf.";
    return nullptr;
  }
  LOG(INFO) << "ColinL: Successfully parsed storage. Cookies: "
            << storage->cookies_size()
            << ", LocalStorage groups: " << storage->local_storages_size();
  return storage;
}

content::RenderFrameHost* RenderFrameHost(
    content::WeakDocumentPtr weak_document_ptr) {
  auto* render_frame_host = weak_document_ptr.AsRenderFrameHostIfValid();
  CHECK(render_frame_host);
  return render_frame_host;
}

network::mojom::CookieManager* CookieManager(
    content::WeakDocumentPtr weak_document_ptr) {
  auto* storage_partition =
      RenderFrameHost(weak_document_ptr)->GetStoragePartition();
  CHECK(storage_partition);
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  CHECK(cookie_manager);
  return cookie_manager;
}

#if !defined(COBALT_IS_RELEASE_BUILD)
Task LogElapsedTimeTask() {
  return base::BindOnce(
      [](std::unique_ptr<base::ElapsedTimer> elapsed_timer,
         base::OnceClosure callback) {
        LOG(INFO) << "Cookie and localStorage migration completed in "
                  << elapsed_timer->Elapsed().InMilliseconds() << " ms.";
        std::move(callback).Run();
      },
      std::make_unique<base::ElapsedTimer>());
}
#endif  // !defined(COBALT_IS_RELEASE_BUILD)

Task ReloadTask(content::WeakDocumentPtr weak_document_ptr, const GURL& url) {
  return base::BindOnce(
      [](content::WeakDocumentPtr weak_document_ptr, const GURL& url,
         base::OnceClosure callback) {
        auto* rfh = RenderFrameHost(weak_document_ptr);
        content::WebContents* web_contents =
            content::WebContents::FromRenderFrameHost(rfh);
        if (web_contents) {
          LOG(INFO) << "ColinL: MigrationManager Reloading URL: " << url.spec();
          content::NavigationController::LoadURLParams params(url);
          params.transition_type = ui::PageTransitionFromInt(
              ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
          web_contents->GetController().LoadURLWithParams(params);
        }
        std::move(callback).Run();
      },
      weak_document_ptr, url);
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
    // Don't just run the callback; post it to the current sequence
    // to give the ThreadPool a head start and clear the stack.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
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

  // 2. Get the target URL (to determine the Origin/StorageKey)
  GURL initial_url(
      switches::GetInitialURL(*base::CommandLine::ForCurrentProcess()));
  if (!initial_url.is_valid()) {
    std::move(done_callback).Run();
    return;
  }
  url::Origin origin = url::Origin::Create(initial_url);

  // 3. Read the legacy storage record
  auto storage = ReadStorage();
  if (!storage ||
      (storage->cookies_size() == 0 && storage->local_storages_size() == 0)) {
    LOG(INFO) << "ColinL: Nothing to migrate.";
    std::move(done_callback).Run();
    return;
  }

  // 4. Build and Run Task Group
  std::vector<Task> tasks;
  tasks.push_back(CookieTask(partition, ToCanonicalCookies(*storage)));
  tasks.push_back(LocalStorageTask(partition, origin,
                                   ToLocalStorageItems(origin, *storage)));
  tasks.push_back(DeleteOldCacheDirectoryTask());

  std::move(GroupTasks(std::move(tasks))).Run(std::move(done_callback));
}

// TODO(b/399165612): Add metrics.
// TODO(b/399165796): Disable and delete migration code when possible.
// TODO(b/399166308): Add unit and/or integration tests for migration.
// void MigrationManager::EnsureMigrationDone(content::WebContents*
// web_contents,
//                                            base::OnceClosure done_callback) {
//   LOG(INFO) << "ColinL: EnsureMigrationDone started.";
//   if (migration_attempted_.test_and_set()) {
//     LOG(INFO) << "ColinL: Migration already attempted this session.";
//     std::move(done_callback).Run();
//     return;
//   }

//   base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
//   if (command_line->HasSwitch(switches::kDisableStorageMigration)) {
//     LOG(INFO) << "ColinL: Storage migration disabled via switch.";
//     std::move(done_callback).Run();
//     return;
//   }

//   auto* rfh = web_contents->GetPrimaryMainFrame();
//   if (!rfh) {
//     LOG(ERROR) << "ColinL: No PrimaryMainFrame available for migration.";
//     std::move(done_callback).Run();
//     return;
//   }
//   auto weak_document_ptr = rfh->GetWeakDocumentPtr();

//   LOG(INFO) << "ColinL: Checking for existing cookies before migration...";
//   CookieManager(weak_document_ptr)
//       ->GetAllCookies(base::BindOnce(
//           [](content::WebContents* web_contents,
//              base::OnceClosure done_callback,
//              const std::vector<net::CanonicalCookie>& existing_cookies) {
//             if (!existing_cookies.empty()) {
//               LOG(INFO) << "ColinL: Found " << existing_cookies.size()
//                         << " existing cookies. Skipping migration.";
//               for (const auto& cookie : existing_cookies) {
//                 VLOG(1) << "ColinL: Existing Cookie: " << cookie.Name();
//               }
//               std::move(done_callback).Run();
//               return;
//             }
//             LOG(INFO)
//                 << "ColinL: No cookies found. Triggering RunMigrationTasks.";
//             RunMigrationTasks(web_contents, std::move(done_callback));
//           },
//           web_contents, std::move(done_callback)));
// }

// void MigrationManager::RunMigrationTasks(content::WebContents* web_contents,
//                                          base::OnceClosure done_callback) {
//   LOG(INFO) << "ColinL: RunMigrationTasks started.";
//   auto storage = ReadStorage();
//   if (!storage ||
//       (storage->cookies_size() == 0 && storage->local_storages_size() == 0))
//       {
//     LOG(INFO) << "ColinL: Legacy storage empty or null. Nothing to migrate.";
//     std::move(done_callback).Run();
//     return;
//   }

//   GURL url = web_contents->GetLastCommittedURL();
//   LOG(INFO) << "ColinL: Stopping WebContents for migration. Last URL: "
//             << url.spec();
//   web_contents->Stop();

//   auto* render_frame_host = web_contents->GetPrimaryMainFrame();
//   CHECK(render_frame_host);
//   auto weak_document_ptr = render_frame_host->GetWeakDocumentPtr();

//   std::vector<Task> tasks;
//   LOG(INFO) << "ColinL: Queueing Cookie and LocalStorage tasks...";
//   tasks.push_back(CookieTask(partition, ToCanonicalCookies(*storage)));
//   url::Origin origin = url::Origin::Create(initial_url);
//   tasks.push_back(LocalStorageTask(partition, origin,
//                                    ToLocalStorageItems(origin, *storage)));
// #if !defined(COBALT_IS_RELEASE_BUILD)
//   tasks.push_back(LogElapsedTimeTask());
// #endif
//   tasks.push_back(DeleteOldCacheDirectoryTask());
//   // tasks.push_back(ReloadTask(weak_document_ptr, url));

//   LOG(INFO) << "ColinL: Executing task group.";
//   std::move(GroupTasks(std::move(tasks))).Run(std::move(done_callback));
// }

// static
Task MigrationManager::CookieTask(
    content::StoragePartition* partition,
    std::vector<std::unique_ptr<net::CanonicalCookie>> cookies) {
  LOG(INFO) << "ColinL: Creating CookieTask for " << cookies.size()
            << " cookies.";
  return base::BindOnce(
      [](content::StoragePartition* partition,
         std::vector<std::unique_ptr<net::CanonicalCookie>> cookies,
         base::OnceClosure callback) {
        auto* cookie_manager = GetCookieManager(partition);

        // 1. Clear existing cookies
        cookie_manager->DeleteCookies(
            network::mojom::CookieDeletionFilter::New(),
            base::BindOnce([](uint32_t) { /* ignored */ }));

        // 2. Inject new cookies
        for (auto& cookie : cookies) {
          GURL source_url("https://" + cookie->Domain() + cookie->Path());
          cookie_manager->SetCanonicalCookie(
              *cookie, source_url, net::CookieOptions::MakeAllInclusive(),
              base::DoNothing());
        }

        // 3. Flush and continue
        cookie_manager->FlushCookieStore(std::move(callback));
      },
      partition, std::move(cookies));
}

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
        StorageAreaRemote area;
        GetLocalStorageArea(partition, origin, area);

        // Your mojom: DeleteAll(string source,
        // pending_remote<StorageAreaObserver>? observer) => (bool success) We
        // use BindOnce with IgnoreArgs because we don't care about the bool
        // result.
        area->DeleteAll("migration", mojo::NullRemote(),
                        base::BindOnce([](bool success) {}));

        absl::optional<std::vector<uint8_t>> empty_old_value;
        for (const auto& pair : pairs) {
          std::vector<uint8_t> key(pair->first.begin(), pair->first.end());
          std::vector<uint8_t> value(pair->second.begin(), pair->second.end());

          // Your mojom: Put(array<uint8> key, array<uint8> value, array<uint8>?
          // old, string source) => (bool success)
          area->Put(key, value, empty_old_value, "migration",
                    base::BindOnce([](bool success) {}));
        }

        // In your version of Mojo, FlushForTesting is synchronous and takes NO
        // arguments. It blocks the current thread until all previous calls
        // (DeleteAll, Put) have been dispatched to the Storage Service.
        area.FlushForTesting();

        // Now that the pipe is flushed, we can safely allow the task chain to
        // continue.
        std::move(callback).Run();
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
