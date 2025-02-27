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

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "cobalt/browser/migrate_storage_record/storage.pb.h"
#include "cobalt/browser/switches.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "starboard/common/storage.h"
#include "url/gurl.h"

namespace cobalt {
namespace migrate_storage_record {

namespace {

std::atomic_flag migration_attempted = ATOMIC_FLAG_INIT;

using Task = base::OnceCallback<void(base::OnceClosure)>;

void DoTasks(std::vector<Task> tasks) {
  base::OnceClosure all_tasks_closure = base::DoNothing();
  for (Task& task : base::Reversed(tasks)) {
    all_tasks_closure =
        base::BindOnce(std::move(task), std::move(all_tasks_closure));
  }
  std::move(all_tasks_closure).Run();
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

std::vector<std::unique_ptr<net::CanonicalCookie>> ToCanonicalCookies(
    const cobalt::storage::Storage& storage) {
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

std::vector<std::pair<std::string, std::string>> ToLocalStorageEntries(
    const url::Origin& page_origin,
    const cobalt::storage::Storage& storage) {
  std::vector<std::pair<std::string, std::string>> entries;
  for (const auto& local_storages : storage.local_storages()) {
    GURL local_storage_origin(local_storages.serialized_origin());
    if (!local_storage_origin.SchemeIs("https") ||
        !page_origin.IsSameOriginWith(local_storage_origin)) {
      continue;
    }

    for (const auto& local_storage_entry :
         local_storages.local_storage_entries()) {
      entries.push_back(std::make_pair<>(local_storage_entry.key(),
                                         local_storage_entry.value()));
    }
  }
  return entries;
}

network::mojom::CookieManager* GetCookieManager(
    content::WeakDocumentPtr weak_document_ptr) {
  auto* render_frame_host = weak_document_ptr.AsRenderFrameHostIfValid();
  CHECK(render_frame_host);
  auto* storage_partition = render_frame_host->GetStoragePartition();
  CHECK(storage_partition);
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  CHECK(cookie_manager);
  return cookie_manager;
}

void ClearCookies(content::WeakDocumentPtr weak_document_ptr,
                  base::OnceClosure callback) {
  GetCookieManager(weak_document_ptr)
      ->DeleteCookies(
          network::mojom::CookieDeletionFilter::New(),
          base::BindOnce(
              [](base::OnceClosure callback, uint32_t /* num_deleted */) {
                std::move(callback).Run();
              },
              std::move(callback)));
}

void ClearLocalStorage(content::WeakDocumentPtr weak_document_ptr,
                       base::OnceClosure callback) {
  auto* render_frame_host = weak_document_ptr.AsRenderFrameHostIfValid();
  CHECK(render_frame_host);
  render_frame_host->ExecuteJavaScriptMethod(
      base::ASCIIToUTF16(std::string("localStorage")),
      base::ASCIIToUTF16(std::string("clear")), base::Value::List(),
      base::BindOnce(
          [](base::OnceClosure callback, base::Value /* result */) {
            std::move(callback).Run();
          },
          std::move(callback)));
}

void MigrateCookiesHelper(
    content::WeakDocumentPtr weak_document_ptr,
    std::vector<std::unique_ptr<net::CanonicalCookie>> cookies,
    base::OnceClosure callback,
    net::CookieAccessResult /* set_cookie_result */) {
  if (cookies.empty()) {
    GetCookieManager(weak_document_ptr)->FlushCookieStore(std::move(callback));
    return;
  }

  auto cookie = std::move(cookies.back());
  cookies.pop_back();
  GURL source_url("https://" + cookie->Domain() + cookie->Path());
  GetCookieManager(weak_document_ptr)
      ->SetCanonicalCookie(
          *cookie, source_url, net::CookieOptions::MakeAllInclusive(),
          base::BindOnce(&MigrateCookiesHelper, weak_document_ptr,
                         std::move(cookies), std::move(callback)));
}

void MigrateCookies(content::WeakDocumentPtr weak_document_ptr,
                    std::vector<std::unique_ptr<net::CanonicalCookie>> cookies,
                    base::OnceClosure callback) {
  if (cookies.empty()) {
    std::move(callback).Run();
    return;
  }
  MigrateCookiesHelper(weak_document_ptr, std::move(cookies),
                       std::move(callback), net::CookieAccessResult());
}

void MigrateLocalStorageHelper(
    content::WeakDocumentPtr weak_document_ptr,
    std::vector<std::pair<std::string, std::string>> entries,
    base::OnceClosure callback,
    base::Value /* result */) {
  if (entries.empty()) {
    std::move(callback).Run();
    return;
  }

  auto* render_frame_host = weak_document_ptr.AsRenderFrameHostIfValid();
  CHECK(render_frame_host);

  std::string key = entries.back().first;
  std::string value = entries.back().second;
  entries.pop_back();
  base::Value::List arguments = base::Value::List().Append(key).Append(value);
  render_frame_host->ExecuteJavaScriptMethod(
      base::ASCIIToUTF16(std::string("localStorage")),
      base::ASCIIToUTF16(std::string("setItem")), std::move(arguments),
      base::BindOnce(&MigrateLocalStorageHelper, weak_document_ptr,
                     std::move(entries), std::move(callback)));
}

void MigrateLocalStorage(
    content::WeakDocumentPtr weak_document_ptr,
    std::vector<std::pair<std::string, std::string>> entries,
    base::OnceClosure callback) {
  MigrateLocalStorageHelper(weak_document_ptr, std::move(entries),
                            std::move(callback), base::Value());
}

void Reload(content::WeakDocumentPtr weak_document_ptr,
            base::OnceClosure callback) {
  auto* render_frame_host = weak_document_ptr.AsRenderFrameHostIfValid();
  CHECK(render_frame_host);
  render_frame_host->Reload();
  std::move(callback).Run();
}

}  // namespace

// TODO(b/399165612): Add metrics.
// TODO(b/399165796): Disable and delete migration code when possible.
// TODO(b/399166308): Add unit and/or integration tests for migration.
void DoMigrationTasksOnce(content::WebContents* web_contents) {
  if (migration_attempted.test_and_set()) {
    return;
  }

  auto storage = ReadStorage();
  if (!storage) {
    return;
  }

  web_contents->Stop();
  auto* render_frame_host = web_contents->GetPrimaryMainFrame();
  CHECK(render_frame_host);
  auto weak_document_ptr = render_frame_host->GetWeakDocumentPtr();
  auto entries = ToLocalStorageEntries(
      render_frame_host->GetLastCommittedOrigin(), *storage);
  auto cookies = ToCanonicalCookies(*storage);
  std::vector<Task> tasks;
  tasks.push_back(base::BindOnce(&ClearCookies, weak_document_ptr));
  tasks.push_back(base::BindOnce(&ClearLocalStorage, weak_document_ptr));
  tasks.push_back(
      base::BindOnce(&MigrateCookies, weak_document_ptr, std::move(cookies)));
  tasks.push_back(base::BindOnce(&MigrateLocalStorage, weak_document_ptr,
                                 std::move(entries)));
  tasks.push_back(base::BindOnce(&Reload, weak_document_ptr));
#if !defined(COBALT_IS_RELEASE_BUILD)
  tasks.push_back(base::BindOnce(
      [](std::unique_ptr<base::ElapsedTimer> elapsed_timer,
         base::OnceClosure /* callback */) {
        LOG(INFO) << "Cookie and localStorage migration completed in "
                  << elapsed_timer->Elapsed().InMilliseconds() << " ms.";
      },
      std::make_unique<base::ElapsedTimer>()));
#endif  // !defined(COBALT_IS_RELEASE_BUILD)
  DoTasks(std::move(tasks));
}

}  // namespace migrate_storage_record
}  // namespace cobalt
