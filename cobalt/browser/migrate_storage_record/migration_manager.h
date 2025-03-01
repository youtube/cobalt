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

#ifndef COBALT_COBALT_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_
#define COBALT_COBALT_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/canonical_cookie.h"

namespace cobalt {
namespace migrate_storage_record {

using Task = base::OnceCallback<void(base::OnceClosure)>;
using Tasks = std::vector<Task>;

template <typename T>
using ItemPtr = std::unique_ptr<T>;
template <typename T>
using ItemPtrs = std::vector<ItemPtr<T>>;

using StringPair = std::pair<std::string, std::string>;

class MigrationManager {
 public:
  static void DoMigrationTasksOnce(content::WebContents* web_contents);

 private:
  friend class MigrationManagerTest;

  static void DoTasks(std::vector<Task> tasks,
                      base::OnceClosure callback = base::DoNothing());
  static Task GroupTasks(std::vector<Task> tasks);
  static Task CookieTask(content::WeakDocumentPtr weak_document_ptr,
                         ItemPtrs<net::CanonicalCookie> cookies);
  static Task LocalStorageTask(content::WeakDocumentPtr weak_document_ptr,
                               ItemPtrs<StringPair> pairs);

  static std::atomic_flag migration_attempted_;
};

}  // namespace migrate_storage_record
}  // namespace cobalt

#endif  // COBALT_COBALT_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_
