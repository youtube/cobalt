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

#ifndef COBALT_BROWSER_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_
#define COBALT_BROWSER_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "cobalt/browser/migrate_storage_record/storage.pb.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/canonical_cookie.h"

namespace cobalt {
namespace migrate_storage_record {

// Used to sequence tasks.
using Task = base::OnceCallback<void(base::OnceClosure)>;

class MigrationManager {
 public:
  static void DoMigrationTasksOnce(content::WebContents* web_contents);

 private:
  friend class MigrationManagerTest;

  // Returns a task. When invoked, the grouped |tasks| are run sequentially.
  static Task GroupTasks(std::vector<Task> tasks);
  static Task CookieTask(
      content::WeakDocumentPtr weak_document_ptr,
      std::vector<std::unique_ptr<net::CanonicalCookie>> cookies);
  static Task LocalStorageTask(
      content::WeakDocumentPtr weak_document_ptr,
      std::vector<std::unique_ptr<std::pair<std::string, std::string>>> pairs);
  static std::vector<std::unique_ptr<std::pair<std::string, std::string>>>
  ToLocalStorageItems(const url::Origin& page_origin,
                      const cobalt::storage::Storage& storage);
  static std::vector<std::unique_ptr<net::CanonicalCookie>> ToCanonicalCookies(
      const cobalt::storage::Storage& storage);
  static std::atomic_flag migration_attempted_;
};

}  // namespace migrate_storage_record
}  // namespace cobalt

#endif  // COBALT_BROWSER_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_
