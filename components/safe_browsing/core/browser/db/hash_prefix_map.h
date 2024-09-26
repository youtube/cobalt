// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_

#include <string>
#include <unordered_map>

#include "base/files/memory_mapped_file.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

namespace safe_browsing {

// Enumerate different events while applying the update fetched fom the server
// for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum ApplyUpdateResult {
  // No errors.
  APPLY_UPDATE_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_APPLY_UPDATE_FAILURE = 1,

  // Prefix size smaller than 4 (which is the lowest expected).
  PREFIX_SIZE_TOO_SMALL_FAILURE = 2,

  // Prefix size larger than 32 (length of a full SHA256 hash).
  PREFIX_SIZE_TOO_LARGE_FAILURE = 3,

  // The number of bytes in additions isn't a multiple of prefix size.
  ADDITIONS_SIZE_UNEXPECTED_FAILURE = 4,

  // The update received from the server contains a prefix that's already
  // present in the map.
  ADDITIONS_HAS_EXISTING_PREFIX_FAILURE = 5,

  // The server sent a response_type that the client did not expect.
  UNEXPECTED_RESPONSE_TYPE_FAILURE = 6,

  // One of more index(es) in removals field of the response is greater than
  // the number of hash prefixes currently in the (old) store.
  REMOVALS_INDEX_TOO_LARGE_FAILURE = 7,

  // Failed to decode the Rice-encoded additions/removals field.
  RICE_DECODING_FAILURE = 8,

  // Compression type other than RAW and RICE for additions.
  UNEXPECTED_COMPRESSION_TYPE_ADDITIONS_FAILURE = 9,

  // Compression type other than RAW and RICE for removals.
  UNEXPECTED_COMPRESSION_TYPE_REMOVALS_FAILURE = 10,

  // The state of the store did not match the expected checksum sent by the
  // server.
  CHECKSUM_MISMATCH_FAILURE = 11,

  // There was a failure trying to map the file.
  MMAP_FAILURE = 12,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  APPLY_UPDATE_RESULT_MAX
};

// The sorted list of hash prefixes.
using HashPrefixes = std::string;

using HashPrefixesView = base::StringPiece;
using HashPrefixMapView = std::unordered_map<PrefixSize, HashPrefixesView>;

// Set a common sense limit on the store file size we try to read.
// The maximum store file size, as of today, is about 6MB.
constexpr size_t kMaxStoreSizeBytes = 50 * 1000 * 1000;

// Stores the list of sorted hash prefixes, by size.
// For instance: {4: ["abcd", "bcde", "cdef", "gggg"], 5: ["fffff"]}
class HashPrefixMap {
 public:
  virtual ~HashPrefixMap() = default;

  // Clears the underlying map.
  virtual void Clear() = 0;

  // Returns a read-only view of the data stored in this map.
  virtual HashPrefixMapView view() const = 0;

  // Appends |prefix| to the prefix list of size |size|.
  virtual void Append(PrefixSize size, HashPrefixesView prefix) = 0;

  // Reserves space for the prefix list of size |size|.
  virtual void Reserve(PrefixSize size, size_t capacity) = 0;

  // Reads and writes the map from disk.
  virtual ApplyUpdateResult ReadFromDisk(
      const V4StoreFileFormat& file_format) = 0;
  virtual bool WriteToDisk(V4StoreFileFormat* file_format) = 0;

  // Returns true if the data in this map is valid and can be used.
  virtual ApplyUpdateResult IsValid() const = 0;

  // Returns a hash prefix if it matches the prefixes stored in this map.
  virtual HashPrefixStr GetMatchingHashPrefix(base::StringPiece full_hash) = 0;

  // Migrates the file format between the different types of HashPrefixMap.
  enum class MigrateResult { kSuccess, kFailure, kNotNeeded };
  virtual MigrateResult MigrateFileFormat(const base::FilePath& store_path,
                                          V4StoreFileFormat* file_format) = 0;

  // Collects debug information about the prefixes in the map.
  virtual void GetPrefixInfo(
      google::protobuf::RepeatedPtrField<
          DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>*
          prefix_sets) = 0;
};

// An in-memory implementation of HashPrefixMap.
class InMemoryHashPrefixMap : public HashPrefixMap {
 public:
  InMemoryHashPrefixMap();
  ~InMemoryHashPrefixMap() override;

  // HashPrefixMap implementation:
  void Clear() override;
  HashPrefixMapView view() const override;
  void Append(PrefixSize size, HashPrefixesView prefix) override;
  void Reserve(PrefixSize size, size_t capacity) override;
  ApplyUpdateResult ReadFromDisk(const V4StoreFileFormat& file_format) override;
  bool WriteToDisk(V4StoreFileFormat* file_format) override;
  ApplyUpdateResult IsValid() const override;
  HashPrefixStr GetMatchingHashPrefix(base::StringPiece full_hash) override;
  MigrateResult MigrateFileFormat(const base::FilePath& store_path,
                                  V4StoreFileFormat* file_format) override;
  void GetPrefixInfo(google::protobuf::RepeatedPtrField<
                     DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>*
                         prefix_sets) override;

 private:
  std::unordered_map<PrefixSize, HashPrefixes> map_;
};

// A HashPrefixMap which will write separate files for hash prefix lists of each
// prefix size. These will be mapped into memory on initialization.
class MmapHashPrefixMap : public HashPrefixMap {
 public:
  explicit MmapHashPrefixMap(const base::FilePath& store_path,
                             size_t buffer_size = 1024 * 512);
  ~MmapHashPrefixMap() override;

  // HashPrefixMap implementation:
  void Clear() override;
  HashPrefixMapView view() const override;
  void Append(PrefixSize size, HashPrefixesView prefix) override;
  void Reserve(PrefixSize size, size_t capacity) override;
  ApplyUpdateResult ReadFromDisk(const V4StoreFileFormat& file_format) override;
  bool WriteToDisk(V4StoreFileFormat* file_format) override;
  ApplyUpdateResult IsValid() const override;
  HashPrefixStr GetMatchingHashPrefix(base::StringPiece full_hash) override;
  MigrateResult MigrateFileFormat(const base::FilePath& store_path,
                                  V4StoreFileFormat* file_format) override;
  void GetPrefixInfo(google::protobuf::RepeatedPtrField<
                     DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>*
                         prefix_sets) override;

  static base::FilePath GetPath(const base::FilePath& store_path,
                                const std::string& extension);

  const std::string& GetExtensionForTesting(PrefixSize size);

 private:
  class BufferedFileWriter;
  class FileInfo {
   public:
    FileInfo(const base::FilePath& store_path, PrefixSize size);
    ~FileInfo();

    bool Initialize(const HashFile& hash_file);
    bool Finalize(HashFile* hash_file);

    HashPrefixesView GetView() const;
    bool IsReadable() const { return file_.IsValid(); }
    HashPrefixStr Matches(base::StringPiece full_hash) const;
    BufferedFileWriter* GetOrCreateWriter(size_t buffer_size);

    const std::string& GetExtensionForTesting() const;

   private:
    const base::FilePath store_path_;
    const PrefixSize prefix_size_;

    base::MemoryMappedFile file_;
    std::unique_ptr<BufferedFileWriter> writer_;
    std::vector<uint32_t> offsets_;
  };

  FileInfo& GetFileInfo(PrefixSize size);

  base::FilePath store_path_;
  std::unordered_map<PrefixSize, FileInfo> map_;
  size_t buffer_size_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_
