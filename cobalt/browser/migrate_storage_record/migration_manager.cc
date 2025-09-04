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

#include "cobalt/browser/migrate_storage_record/migration_manager.h"

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "cobalt/browser/switches.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "starboard/common/storage.h"
#include "url/gurl.h"

namespace cobalt {
namespace migrate_storage_record {

namespace {

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
  auto record =
      std::make_unique<starboard::StorageRecord>(partition_key.c_str());
  if (!record->IsValid()) {
    record->Delete();
    bool fallback = partition_key == GetApplicationKey(GURL(kDefaultURL));
    if (!fallback) {
      return nullptr;
    }

    record = std::make_unique<starboard::StorageRecord>();
    if (!record->IsValid()) {
      record->Delete();
      return nullptr;
    }
  }

  if (record->GetSize() < kRecordHeaderSize) {
    record->Delete();
    return nullptr;
  }

  auto bytes = std::vector<uint8_t>(record->GetSize());
  const int read_result =
      record->Read(reinterpret_cast<char*>(bytes.data()), bytes.size());
  record->Delete();
  if (read_result != bytes.size()) {
    return nullptr;
  }

  std::string version(reinterpret_cast<const char*>(bytes.data()),
                      kRecordHeaderSize);
  if (version != kRecordHeader) {
    return nullptr;
  }

  auto storage = std::make_unique<cobalt::storage::Storage>();
  if (!storage->ParseFromArray(
          reinterpret_cast<const char*>(bytes.data() + kRecordHeaderSize),
          bytes.size() - kRecordHeaderSize)) {
    return nullptr;
  }
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

Task ReloadTask(content::WeakDocumentPtr weak_document_ptr) {
  return base::BindOnce(
      [](content::WeakDocumentPtr weak_document_ptr,
         base::OnceClosure callback) {
        RenderFrameHost(weak_document_ptr)->Reload();
        std::move(callback).Run();
      },
      weak_document_ptr);
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
    content::WebContents* web_contents) {
  if (migration_attempted_.test_and_set()) {
    return;
  }

  auto storage = ReadStorage();
  if (!storage ||
      (storage->cookies_size() == 0 && storage->local_storages_size() == 0)) {
    return;
  }

  web_contents->Stop();

  auto* render_frame_host = web_contents->GetPrimaryMainFrame();
  CHECK(render_frame_host);
  auto weak_document_ptr = render_frame_host->GetWeakDocumentPtr();

  std::vector<Task> tasks;
  tasks.push_back(CookieTask(weak_document_ptr, ToCanonicalCookies(*storage)));
  const url::Origin& origin = render_frame_host->GetLastCommittedOrigin();
  tasks.push_back(LocalStorageTask(weak_document_ptr,
                                   ToLocalStorageItems(origin, *storage)));
#if !defined(COBALT_IS_RELEASE_BUILD)
  tasks.push_back(LogElapsedTimeTask());
#endif  // !defined(COBALT_IS_RELEASE_BUILD)
  tasks.push_back(ReloadTask(weak_document_ptr));
  std::move(GroupTasks(std::move(tasks))).Run(base::DoNothing());
}

// static
Task MigrationManager::CookieTask(
    content::WeakDocumentPtr weak_document_ptr,
    std::vector<std::unique_ptr<net::CanonicalCookie>> cookies) {
  std::vector<Task> tasks;
  tasks.push_back(base::BindOnce(
      [](content::WeakDocumentPtr weak_document_ptr,
         base::OnceClosure callback) {
        CookieManager(weak_document_ptr)
            ->DeleteCookies(network::mojom::CookieDeletionFilter::New(),
                            base::IgnoreArgs<uint32_t>(std::move(callback)));
      },
      weak_document_ptr));
  for (auto& cookie : cookies) {
    tasks.push_back(base::BindOnce(
        [](content::WeakDocumentPtr weak_document_ptr,
           std::unique_ptr<net::CanonicalCookie> cookie,
           base::OnceClosure callback) {
          GURL source_url("https://" + cookie->Domain() + cookie->Path());
          CookieManager(weak_document_ptr)
              ->SetCanonicalCookie(*cookie, source_url,
                                   net::CookieOptions::MakeAllInclusive(),
                                   base::IgnoreArgs<net::CookieAccessResult>(
                                       std::move(callback)));
        },
        weak_document_ptr, std::move(cookie)));
  }
  tasks.push_back(base::BindOnce(
      [](content::WeakDocumentPtr weak_document_ptr,
         base::OnceClosure callback) {
        CookieManager(weak_document_ptr)->FlushCookieStore(std::move(callback));
      },
      weak_document_ptr));
  return GroupTasks(std::move(tasks));
}

// static
Task MigrationManager::LocalStorageTask(
    content::WeakDocumentPtr weak_document_ptr,
    std::vector<std::unique_ptr<std::pair<std::string, std::string>>> pairs) {
  std::vector<Task> tasks;
  tasks.push_back(base::BindOnce(
      [](content::WeakDocumentPtr weak_document_ptr,
         base::OnceClosure callback) {
        RenderFrameHost(weak_document_ptr)
            ->ExecuteJavaScriptMethod(
                base::ASCIIToUTF16(std::string("localStorage")),
                base::ASCIIToUTF16(std::string("clear")), base::Value::List(),
                base::IgnoreArgs<base::Value>(std::move(callback)));
      },
      weak_document_ptr));

  for (auto& pair : pairs) {
    tasks.push_back(base::BindOnce(
        [](content::WeakDocumentPtr weak_document_ptr,
           std::unique_ptr<std::pair<std::string, std::string>> pair,
           base::OnceClosure callback) {
          RenderFrameHost(weak_document_ptr)
              ->ExecuteJavaScriptMethod(
                  base::ASCIIToUTF16(std::string("localStorage")),
                  base::ASCIIToUTF16(std::string("setItem")),
                  base::Value::List().Append(pair->first).Append(pair->second),
                  base::IgnoreArgs<base::Value>(std::move(callback)));
        },
        weak_document_ptr, std::move(pair)));
  }
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
        base::Time::FromInternalValue(c.creation_time_us()), c.secure(),
        c.http_only(), net::CookieSameSite::NO_RESTRICTION,
        net::COOKIE_PRIORITY_DEFAULT, false,
        absl::optional<net::CookiePartitionKey>(),
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
