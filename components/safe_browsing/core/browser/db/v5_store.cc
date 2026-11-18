// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_store.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_container.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace safe_browsing {

namespace {

const char kApplyUpdate[] = ".ApplyUpdate";
const char kResult[] = ".Result";

void RecordStoreReadResult(V5StoreReadResult result) {
  base::UmaHistogramEnumeration("SafeBrowsing.V5StoreRead.Result", result);
  base::UmaHistogramBoolean("SafeBrowsing.SBStoreRead.Success",
                            result == V5StoreReadResult::kReadSuccess);
}

template <typename T>
void RecordEnumWithAndWithoutSuffix(const std::string& metric,
                                    T value,
                                    const base::FilePath& file_path) {
  base::UmaHistogramEnumeration(metric + kResult, value);
  std::string suffix = GetUmaSuffixForStore(file_path);
  base::UmaHistogramEnumeration(metric + kResult + suffix, value);
}

void RecordApplyUpdateResult(const std::string& base_metric,
                             V5ApplyUpdateResult result,
                             const base::FilePath& file_path) {
  RecordEnumWithAndWithoutSuffix(base_metric + kApplyUpdate, result, file_path);
}

}  // namespace

V5Store::V5Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path,
                 PrefixSize prefix_size,
                 const base::FilePath& v4_store_path,
                 const int64_t old_file_size)
    : SBStore(task_runner, store_path, old_file_size),
      hash_prefix_list_(std::make_unique<HashPrefixList>(store_path,
                                                         prefix_size,
                                                         task_runner)),
      prefix_size_(prefix_size),
      v4_store_path_(v4_store_path) {}

V5Store::~V5Store() = default;

void V5Store::Initialize() {
  CHECK(version_.empty());

  V5StoreReadResult store_read_result = ReadFromDisk();
  has_valid_data_ = (store_read_result == V5StoreReadResult::kReadSuccess);
  RecordStoreReadResult(store_read_result);
}

std::string V5Store::GetMetricPrefix() const {
  return "SafeBrowsing.V5Store";
}

V5StoreReadResult V5Store::ReadFromDisk() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  V4ToV5MigrationResult migration_result = AttemptV4ToV5Migration();
  base::UmaHistogramEnumeration("SafeBrowsing.V5Store.V4ToV5MigrationResult",
                                migration_result);

  switch (migration_result) {
    case V4ToV5MigrationResult::kDiskAlreadyV5:
    case V4ToV5MigrationResult::kV4ToV5MigrationSucceeded:
      return ReadFromDiskInternal();
    case V4ToV5MigrationResult::kV4StoreNotFound:
      return V5StoreReadResult::kFileOpenFailure;
    case V4ToV5MigrationResult::kReadV4Failed:
    case V4ToV5MigrationResult::kMultipleHashFilesFailure:
    case V4ToV5MigrationResult::kPrefixSizeMismatchFailure:
    case V4ToV5MigrationResult::kHashFileMissingFailure:
    case V4ToV5MigrationResult::kRenameHashFileFailure:
    case V4ToV5MigrationResult::kWriteV5FileFailure:
    case V4ToV5MigrationResult::kProtoSerializationFailure:
    case V4ToV5MigrationResult::kExtensionParsingFailure:
    case V4ToV5MigrationResult::kRenameV5StoreFileFailure:
      return V5StoreReadResult::kV4ToV5MigrationFailure;
  }
}

V4ToV5MigrationResult V5Store::AttemptV4ToV5Migration() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(!v4_store_path_.empty());

  if (base::PathExists(store_path_)) {
    return V4ToV5MigrationResult::kDiskAlreadyV5;
  }
  if (!base::PathExists(v4_store_path_)) {
    return V4ToV5MigrationResult::kV4StoreNotFound;
  }
  return MigrateFromV4(v4_store_path_);
}

V5StoreReadResult V5Store::ReadFromDiskInternal() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  V5StoreFileFormat file_format;
  int64_t file_size;
  V5StoreReadResult validation_result =
      ParseAndValidateV5StoreFileFormat(store_path_, file_format, &file_size);
  if (validation_result != V5StoreReadResult::kReadSuccess) {
    return validation_result;
  }

  V5ApplyUpdateResult apply_update_result =
      hash_prefix_list_->ReadFromDisk(SBStoreFileFormat(&file_format));
  RecordApplyUpdateResult("SafeBrowsing.V5ReadFromDisk", apply_update_result,
                          store_path_);
  last_apply_update_result_ = apply_update_result;

  if (apply_update_result != V5ApplyUpdateResult::kSuccess) {
    hash_prefix_list_->Clear();
    return V5StoreReadResult::kHashPrefixListGenerationFailure;
  }

  if (file_format.list_details().has_version()) {
    version_ = file_format.list_details().version();
  }
  if (file_format.list_details().has_checksum() &&
      file_format.list_details().checksum().has_sha256()) {
    expected_checksum_ = file_format.list_details().checksum().sha256();
  }

  // Update |file_size_| now because we parsed the file correctly.
  file_size_ = file_size;
  if (file_format.list_details().has_hash_file()) {
    file_size_ += file_format.list_details().hash_file().file_size();
  }

  return V5StoreReadResult::kReadSuccess;
}

V4ToV5MigrationResult V5Store::MigrateFromV4(
    const base::FilePath& v4_store_path) {
  V4StoreFileFormat v4_file_format;
  base::FilePath v5_hash_file_path;
  base::FilePath temp_store_path = store_path_.AddExtensionASCII("tmp");
  bool migration_succeeded = false;

  // If we fail to migrate from v4, we wipe the v4 files and the attempted v5
  // file format temp file.
  absl::Cleanup cleanup_on_failure = [&v4_store_path, &v4_file_format,
                                      &v5_hash_file_path, &temp_store_path,
                                      &migration_succeeded] {
    if (!migration_succeeded) {
      base::DeleteFile(v4_store_path);
      for (const auto& hash_file : v4_file_format.hash_files()) {
        base::DeleteFile(
            HashPrefixContainer::GetPath(v4_store_path, hash_file.extension()));
      }
      if (!v5_hash_file_path.empty()) {
        base::DeleteFile(v5_hash_file_path);
      }
      base::DeleteFile(temp_store_path);
    }
  };

  // Parse and validate the existing V4 store file.
  StoreReadResult validation_result =
      ParseAndValidateV4StoreFileFormat(v4_store_path, v4_file_format);
  if (validation_result != READ_SUCCESS) {
    base::UmaHistogramExactLinear(
        "SafeBrowsing.V5Store.V4ToV5Migration.V4ReadFailureReason",
        validation_result, STORE_READ_RESULT_MAX);
    return V4ToV5MigrationResult::kReadV4Failed;
  }

  // V5 store only supports a single hash file.
  if (v4_file_format.hash_files_size() > 1) {
    return V4ToV5MigrationResult::kMultipleHashFilesFailure;
  }

  base::FilePath v4_hash_file_path;
  std::string v5_ext;
  uint64_t file_size = 0;

  // Handle the V4 hash file if it exists.
  if (v4_file_format.hash_files_size() == 1) {
    const auto& hash_file = v4_file_format.hash_files(0);
    PrefixSize v4_prefix_size = hash_file.prefix_size();
    if (v4_prefix_size != prefix_size_) {
      return V4ToV5MigrationResult::kPrefixSizeMismatchFailure;
    }
    v4_hash_file_path =
        HashPrefixContainer::GetPath(v4_store_path, hash_file.extension());
    if (!base::PathExists(v4_hash_file_path)) {
      return V4ToV5MigrationResult::kHashFileMissingFailure;
    }
    file_size = hash_file.file_size();

    // Extract the V5 extension (timestamp part) from V4 extension
    // (prefix_timestamp).
    std::string v4_ext = hash_file.extension();
    size_t underscore_pos = v4_ext.find('_');
    if (underscore_pos == std::string::npos) {
      return V4ToV5MigrationResult::kExtensionParsingFailure;
    }
    v5_ext = v4_ext.substr(underscore_pos + 1);
    if (v5_ext.empty()) {
      return V4ToV5MigrationResult::kExtensionParsingFailure;
    }

    // Move the V4 hash file to the V5 path with the new extension.
    v5_hash_file_path = HashPrefixContainer::GetPath(store_path_, v5_ext);
    if (!base::Move(v4_hash_file_path, v5_hash_file_path)) {
      return V4ToV5MigrationResult::kRenameHashFileFailure;
    }
  }

  // Construct the new V5StoreFileFormat proto.
  V5StoreFileFormat v5_file_format;
  v5_file_format.set_magic_number(v4_file_format.magic_number());
  v5_file_format.set_file_version(kV5FileVersion);

  ListDetails* list_details = v5_file_format.mutable_list_details();
  if (v4_file_format.list_update_response().has_new_client_state()) {
    list_details->set_version(
        v4_file_format.list_update_response().new_client_state());
  }
  if (v4_file_format.list_update_response().has_checksum()) {
    list_details->mutable_checksum()->set_sha256(
        v4_file_format.list_update_response().checksum().sha256());
  }

  if (!v5_ext.empty()) {
    V5HashFile* v5_hash_file = list_details->mutable_hash_file();
    v5_hash_file->set_extension(v5_ext);
    // TODO(crbug.com/362791941): ensure this is the same as what V5 WriteToDisk
    // eventually does
    v5_hash_file->set_file_size(file_size);
  }

  // Serialize and write the new V5 proto to disk.
  std::string v5_file_format_string;
  if (!v5_file_format.SerializeToString(&v5_file_format_string)) {
    return V4ToV5MigrationResult::kProtoSerializationFailure;
  }

  if (!base::WriteFile(temp_store_path, v5_file_format_string)) {
    return V4ToV5MigrationResult::kWriteV5FileFailure;
  }

  if (!base::Move(temp_store_path, store_path_)) {
    return V4ToV5MigrationResult::kRenameV5StoreFileFailure;
  }

  migration_succeeded = true;

  // Delete the old V4 store file.
  base::DeleteFile(v4_store_path);

  return V4ToV5MigrationResult::kV4ToV5MigrationSucceeded;
}

}  // namespace safe_browsing
