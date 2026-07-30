// Copyright (C) 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PDS_PROTO_DATA_STORE_H_
#define PDS_PROTO_DATA_STORE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/stubs/status_macros.h"
#include "protostore/crc32.h"
#include "protostore/file-storage.h"
#include "protostore/status-macros.h"

namespace protostore {

/// \brief A simple file-backed proto with an in-memory cache.
/// WARNING: Only use this for small protos. Files storing larger protos can
/// benefit from more sophisticated strategies like chunked reads/writes,
/// using mmap and ideally, not even using protos.
///
/// This class is go/thread-compatible
template <typename ProtoT>
class ProtoDataStore final {
 public:
  // Header stored at the beginning of the file before the proto.
  struct Header {
    static constexpr int32_t kMagic = 0x726f746f;

    // Holds the magic as a quick sanity check against file corruption.
    int32_t magic;

    // Checksum of the serialized proto, for a more thorough check against file
    // corruption.
    uint32_t proto_checksum;
  };

  // Used the specified file to read older version of the proto and store
  // newer versions of the proto.
  //
  ProtoDataStore(const FileStorage& file_storage, absl::string_view filename);

  ~ProtoDataStore() = default;

  // Returns a reference to the proto read from the file. It
  // internally caches the read proto so that future calls are fast.
  //
  // NOTE: The caller does NOT get ownership of the object returned and
  // the returned object is only valid till a new version of the proto is
  // written to the file.
  //
  // Returns NOT_FOUND if the file was empty or never written to.
  // Returns INTERNAL_ERROR if an IO error or a corruption was encountered.
  absl::StatusOr<const ProtoT*> Read() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Writes the new version of the proto provided through to disk.
  // Successful Write() invalidates any previously read version of the proto.
  //
  // Returns INTERNAL_ERROR if any IO error is encountered and will NOT
  // invalidate any previously read versions of the proto.
  //
  // TODO(b/132637068): The implementation today loses old data if Write()
  //  fails. We should write to a tmp file first and rename the file to fix
  //  this.
  absl::Status Write(std::unique_ptr<ProtoT> proto) ABSL_LOCKS_EXCLUDED(mutex_);

  // Disallow copy and assign.
  ProtoDataStore(const ProtoDataStore&) = delete;
  ProtoDataStore& operator=(const ProtoDataStore&) = delete;

 private:
  // Upper bound of file-size that is supported.
  static constexpr uint64_t kMaxFileSize = 1 * 1024 * 1024;  // 1 MiB.

  // Used to provide reader and writer locks
  mutable absl::Mutex mutex_;

  const FileStorage& file_storage_;
  const std::string filename_;

  mutable std::unique_ptr<ProtoT> cached_proto_ ABSL_GUARDED_BY(mutex_);
};

template <typename ProtoT>
constexpr uint64_t ProtoDataStore<ProtoT>::kMaxFileSize;

template <typename ProtoT>
ProtoDataStore<ProtoT>::ProtoDataStore(
    const FileStorage& file_storage, absl::string_view filename)
    : file_storage_(file_storage), filename_(filename) {}

template <typename ProtoT>
absl::StatusOr<const ProtoT*> ProtoDataStore<ProtoT>::Read() const {

  absl::WriterMutexLock lock(&mutex_);

  // Return cached proto if we've already read from disk.
  if (cached_proto_ != nullptr) {
    return cached_proto_.get();
  }

  PDS_ASSIGN_OR_RETURN(uint64_t file_size, file_storage_.GetFileSize(filename_));
  if (file_size > kMaxFileSize) {
    return absl::InternalError(absl::StrCat(
        "File larger than expected, couldn't read: ", filename_));
  }

  PDS_ASSIGN_OR_RETURN(std::unique_ptr<InputStream> input_stream,
                   file_storage_.OpenForRead(filename_));

  // Used to hold the memory address and length of the read data.
  absl::string_view read;

  Header header;
  PDS_RETURN_IF_ERROR(input_stream->Read(sizeof(Header), &read,
                                     reinterpret_cast<char*>(&header)));

  if (header.magic != Header::kMagic) {
    return absl::InternalError(
        absl::StrCat("Invalid header kMagic for: ", filename_));
  }

  const uint64_t proto_size = file_size - sizeof(Header);

  // Used to hold the proto read from file.
  auto scratch = absl::make_unique<char[]>(proto_size);

  // Now, |read.data()| points to the beginning of ProtoT.
  PDS_RETURN_IF_ERROR(input_stream->Read(proto_size, &read, scratch.get()));

  absl::string_view proto_str(read.data(), proto_size);

  Crc32 crc;
  crc.Append(proto_str);
  if (header.proto_checksum != crc.Get()) {
    return absl::InternalError(
        absl::StrCat("Checksum of file does not match: ", filename_));
  }

  auto proto = absl::make_unique<ProtoT>();
  if (!proto->ParseFromArray(read.data(), proto_size)) {
    return absl::InternalError(
        absl::StrCat("Proto parse failed. File corrupted: ", filename_));
  }

  cached_proto_ = std::move(proto);
  return cached_proto_.get();
}

template <typename ProtoT>
absl::Status ProtoDataStore<ProtoT>::Write(std::unique_ptr<ProtoT> new_proto) {
  absl::WriterMutexLock lock(&mutex_);

  const std::string new_proto_str = new_proto->SerializeAsString();
  if (new_proto_str.size() >= kMaxFileSize) {
    return absl::InvalidArgumentError(
        absl::StrFormat("New proto too large. size: %lu; limit: %lu.",
                        new_proto_str.size(), kMaxFileSize));
  }

  if (cached_proto_ != nullptr &&
      cached_proto_->SerializeAsString() == new_proto_str) {
    return absl::OkStatus();
  }

  PDS_ASSIGN_OR_RETURN(std::unique_ptr<OutputStream> output_stream,
                   file_storage_.OpenForWrite(filename_));

  Crc32 crc;
  crc.Append(new_proto_str);
  const Header header{.magic = Header::kMagic, .proto_checksum = crc.Get()};

  // Write the header to output stream.
  PDS_RETURN_IF_ERROR(output_stream->Append(absl::string_view(
      reinterpret_cast<const char*>(&header), sizeof(Header))));

  // Write the new proto to output stream.
  PDS_RETURN_IF_ERROR(output_stream->Append(new_proto_str));
  PDS_RETURN_IF_ERROR(output_stream->Close());

  cached_proto_ = std::move(new_proto);
  return absl::OkStatus();
}

}  // namespace protostore

#endif  // PDS_PROTO_DATA_STORE_H_
