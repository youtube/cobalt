// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_STORE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_STORE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_list.h"
#include "components/safe_browsing/core/browser/db/sb_store.h"
#include "components/safe_browsing/core/browser/db/sb_store_file_format.h"

namespace safe_browsing {

// Enumerate different results of the migration attempt from v4 to v5.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(V4ToV5MigrationResult)
enum class V4ToV5MigrationResult {
  // The disk is already in v5 format (migration not needed).
  kDiskAlreadyV5 = 0,

  // The migration from v4 to v5 completed successfully.
  kV4ToV5MigrationSucceeded = 1,

  // The v4 store file was not found on disk.
  kV4StoreNotFound = 2,

  // Failed to read or validate the v4 store file from disk.
  kReadV4Failed = 3,

  // The v4 store file has multiple hash files, which is not supported.
  kMultipleHashFilesFailure = 4,

  // The prefix size in v4 hash file doesn't match the expected V5 prefix size.
  kPrefixSizeMismatchFailure = 5,

  // The referenced v4 hash file is missing from disk.
  kHashFileMissingFailure = 6,

  // Failed to rename/move the v4 hash file to the v5 path.
  kRenameHashFileFailure = 7,

  // Failed to write the new V5StoreFileFormat proto to disk.
  kWriteV5FileFailure = 8,

  // Failed to serialize the V5StoreFileFormat proto.
  kProtoSerializationFailure = 9,

  // Failed to parse or validate the extension of the v4 hash file.
  kExtensionParsingFailure = 10,

  // Failed to rename the temp V5 store file to the final path.
  kRenameV5StoreFileFailure = 11,

  kMaxValue = kRenameV5StoreFileFailure
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:V4ToV5MigrationResult)

class V5Store : public SBStore {
 public:
  // The `task_runner` is used to ensure that the operations in this file are
  // performed on the correct thread. `store_path` specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  // `v4_store_path` is the store path for the corresponding v4 store; this is
  // used for migrating from a v4 database to a v5 database.
  // If the store is being created to apply an update to the old store, then
  // `old_file_size` is the size of the existing file on disk for this store;
  // 0 otherwise. This is needed so that we can correctly report the size of
  // store file on disk, even if writing the new file fails after successfully
  // applying an update.
  V5Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path,
          PrefixSize prefix_size,
          const base::FilePath& v4_store_path,
          int64_t old_file_size = 0);
  ~V5Store() override;

  const std::string& version() const { return version_; }

  // Reads the store file from disk and populates the in-memory representation
  // of the hash prefixes.
  void Initialize();

 protected:
  std::string GetMetricPrefix() const override;

 private:
  friend class V5StoreTest;

  // Reads the state of the store from the file on disk, attempting migration
  // from v4 if necessary. Returns the reason for the failure or reports
  // success.
  V5StoreReadResult ReadFromDisk();

  // Reads the state of the store from the v5 file on disk directly. Returns the
  // reason for the failure or reports success.
  V5StoreReadResult ReadFromDiskInternal();

  // Attempts to migrate the store from v4 to v5 if eligible and needed. Returns
  // the reason for the failure or reports success.
  V4ToV5MigrationResult AttemptV4ToV5Migration();

  // Performs the actual migration steps from the v4 store to v5. Returns the
  // reason for the failure or reports success.
  V4ToV5MigrationResult MigrateFromV4(const base::FilePath& v4_store_path);

  std::unique_ptr<HashPrefixList> hash_prefix_list_;

  // The expected prefix size for the hash prefixes in this store.
  const PrefixSize prefix_size_;

  // The path to the corresponding v4 store file on disk.
  const base::FilePath v4_store_path_;

  // The version of the store as returned by the PVer5 server in the last
  // applied update response.
  std::string version_;

  // The checksum value as read from the disk, until it is verified. Once
  // verified, it is cleared.
  std::string expected_checksum_;

  // Records the status of the update being applied to the database.
  V5ApplyUpdateResult last_apply_update_result_ = V5ApplyUpdateResult::kUnknown;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_STORE_H_
