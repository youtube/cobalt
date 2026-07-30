// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_H_

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
class V5StoreFileFormat;

namespace safe_browsing {

class V4StoreFileFormat;

// Enumerate different failure events while parsing the file read from disk for
// histogramming purposes.  DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum StoreReadResult {
  // No errors.
  READ_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_READ_FAILURE = 1,

  // The contents of the file could not be read.
  FILE_UNREADABLE_FAILURE = 2,

  // The file was found to be empty.
  FILE_EMPTY_FAILURE = 3,

  // The contents of the file could not be interpreted as a valid
  // V4StoreFileFormat proto.
  PROTO_PARSING_FAILURE = 4,

  // The magic number didn't match. We're most likely trying to read a file
  // that doesn't contain hash prefixes.
  UNEXPECTED_MAGIC_NUMBER_FAILURE = 5,

  // The version of the file is different from expected and Chromium doesn't
  // know how to interpret this version of the file.
  FILE_VERSION_INCOMPATIBLE_FAILURE = 6,

  // The rest of the file could not be parsed as a ListUpdateResponse protobuf.
  // This can happen if the machine crashed before the file was fully written to
  // disk or if there was disk corruption.
  HASH_PREFIX_INFO_MISSING_FAILURE = 7,

  // Unable to generate the hash prefix map from the updates on disk.
  HASH_PREFIX_MAP_GENERATION_FAILURE = 8,

  // There was a failure migrating between in-memory and mmap file formats.
  MIGRATION_FAILURE = 9,

  // The file is in a pre-mmap migration format, which is no longer supported.
  PRE_MMAP_MIGRATION_FILE_FORMAT_FAILURE = 10,

  // Failed to migrate from v5 to v4.
  V5_TO_V4_MIGRATION_FAILURE = 11,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  STORE_READ_RESULT_MAX
};

// Enumerate different failure events while parsing the file read from disk for
// histogramming purposes. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
// LINT.IfChange(V5StoreReadResult)
enum class V5StoreReadResult {
  // No errors.
  kReadSuccess = 0,

  // Reserved for errors in parsing this enum.
  kUnexpectedReadFailure = 1,

  // The store file could not be opened (e.g. missing, access denied).
  kFileOpenFailure = 2,

  // The file was found to be empty.
  kFileEmptyFailure = 3,

  // The contents of the file could not be interpreted as a valid
  // V5StoreFileFormat proto.
  kProtoParsingFailure = 4,

  // The magic number didn't match. We're most likely trying to read a file
  // that doesn't contain hash prefixes.
  kUnexpectedMagicNumberFailure = 5,

  // The version of the file is different from expected and Chromium doesn't
  // know how to interpret this version of the file.
  kFileVersionIncompatibleFailure = 6,

  // The rest of the file could not be parsed.
  kHashPrefixInfoMissingFailure = 7,

  // Unable to generate the hash prefix list from the updates on disk.
  kHashPrefixListGenerationFailure = 8,

  // A read error occurred while parsing the file.
  kFileReadFailure = 9,

  // Failed to migrate from v4 to v5.
  kV4ToV5MigrationFailure = 10,

  kMaxValue = kV4ToV5MigrationFailure
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SafeBrowsingV5StoreReadResult)

// A ZeroCopyInputStream that reads from a file using base::File. Any errors
// during deserialization close the file.
class BaseFileInputStream : public google::protobuf::io::ZeroCopyInputStream {
 public:
  // Creates and opens `input_file`.
  explicit BaseFileInputStream(const base::FilePath& input_file);
  BaseFileInputStream(const BaseFileInputStream&) = delete;
  BaseFileInputStream& operator=(const BaseFileInputStream&) = delete;

  // Closes the file, if it was still open.
  ~BaseFileInputStream() override;

  // Returns `base::File::FILE_OK` if no error and the file is still open; else
  // the error that led to closure of the file.
  base::File::Error GetError() const;

  // google::protobuf::io::ZeroCopyInputStream:
  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  int64_t ByteCount() const override;

 private:
  class CopyingBaseFileInputStream
      : public google::protobuf::io::CopyingInputStream {
   public:
    explicit CopyingBaseFileInputStream(const base::FilePath& input_file);
    CopyingBaseFileInputStream(const CopyingBaseFileInputStream&) = delete;
    CopyingBaseFileInputStream& operator=(const CopyingBaseFileInputStream&) =
        delete;
    ~CopyingBaseFileInputStream() override;

    base::File::Error GetError() const;

    // google::protobuf::io::CopyingInputStream:
    int Read(void* buffer, int size) override;
    int Skip(int count) override;

   private:
    base::File file_;
  };

  CopyingBaseFileInputStream stream_;
  google::protobuf::io::CopyingInputStreamAdaptor impl_;
};

// The base class for the Safe Browsing V4 and V5 stores.
class SBStore {
 public:
  // The |task_runner| is used to ensure that the operations in this file are
  // performed on the correct thread. |store_path| specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  // If the store is being created to apply an update to the old store, then
  // |old_file_size| is the size of the existing file on disk for this store;
  // 0 otherwise. This is needed so that we can correctly report the size of
  // store file on disk, even if writing the new file fails after successfully
  // applying an update.
  SBStore(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path,
          int64_t old_file_size = 0);
  virtual ~SBStore();

  // True if this store has valid contents, either from a successful read
  // from disk or a full update.  This does not mean the checksum was verified.
  virtual bool HasValidData();

  const base::FilePath& store_path() const { return store_path_; }

  int64_t file_size() const { return file_size_; }

 protected:
  static constexpr uint32_t kFileMagic = 0x600D71FE;
  static constexpr uint32_t kV4FileVersion = 9;
  static constexpr uint32_t kV5FileVersion = 10;

  // Parses and validates a v4 store file format from disk.
  static StoreReadResult ParseAndValidateV4StoreFileFormat(
      const base::FilePath& store_path,
      V4StoreFileFormat& file_format,
      int64_t* file_size = nullptr);

  // Parses and validates a v5 store file format from disk.
  static V5StoreReadResult ParseAndValidateV5StoreFileFormat(
      const base::FilePath& store_path,
      V5StoreFileFormat& file_format,
      int64_t* file_size = nullptr);

  virtual std::string GetMetricPrefix() const = 0;

  // The size of the file on disk for this store.
  int64_t file_size_;

  // True if the file was successfully read+parsed or was populated from
  // a full update.
  bool has_valid_data_;

  const base::FilePath store_path_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

 private:
  static void RecordBooleanWithAndWithoutSuffix(const std::string& metric,
                                                bool value,
                                                const std::string& suffix);

  void LogHasValidDataHistograms();

  // A counter used to manage how frequently the value of `has_valid_data_`
  // below is recorded.
  uint8_t record_has_valid_data_counter_ = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_H_
