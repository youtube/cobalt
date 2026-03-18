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

#ifndef COBALT_SHELL_BROWSER_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_
#define COBALT_SHELL_BROWSER_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "cobalt/shell/browser/migrate_storage_record/storage.pb.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/canonical_cookie.h"

namespace cobalt {
namespace migrate_storage_record {

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

// Used to sequence tasks.
using Task = base::OnceCallback<void(base::OnceClosure)>;

struct MigrationState : public base::RefCountedThreadSafe<MigrationState> {
  StorageReadResult read_result = StorageReadResult::kSuccess;
  std::atomic<InjectionResult> cookie_result{InjectionResult::kSuccess};
  std::atomic<InjectionResult> local_storage_result{InjectionResult::kSuccess};

  void UpdateCookieResult(InjectionResult res) {
    if (res == InjectionResult::kError) {
      cookie_result.store(InjectionResult::kError, std::memory_order_relaxed);
    }
  }

  void UpdateLocalStorageResult(InjectionResult res) {
    if (res == InjectionResult::kError) {
      local_storage_result.store(InjectionResult::kError,
                                 std::memory_order_relaxed);
    }
  }

 private:
  friend class base::RefCountedThreadSafe<MigrationState>;
  ~MigrationState() = default;
};

class MigrationManager {
 public:
  static void RunMigration(content::StoragePartition* partition,
                           base::OnceClosure done_callback);

 private:
  friend class MigrationManagerTest;

  // Returns a task. When invoked, the grouped |tasks| are run sequentially.
  static Task GroupTasks(std::vector<Task> tasks);
  static Task CookieTask(
      content::StoragePartition* partition,
      std::vector<std::unique_ptr<net::CanonicalCookie>> cookies,
      scoped_refptr<MigrationState> state);
  static Task LocalStorageTask(
      content::StoragePartition* partition,
      const url::Origin& origin,
      std::vector<std::unique_ptr<std::pair<std::string, std::string>>> pairs,
      scoped_refptr<MigrationState> state);
  static std::vector<std::unique_ptr<std::pair<std::string, std::string>>>
  ToLocalStorageItems(const url::Origin& page_origin,
                      const cobalt::storage::Storage& storage);
  static std::vector<std::unique_ptr<net::CanonicalCookie>> ToCanonicalCookies(
      const cobalt::storage::Storage& storage);
};

}  // namespace migrate_storage_record
}  // namespace cobalt

#endif  // COBALT_SHELL_BROWSER_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_
