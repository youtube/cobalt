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

#ifndef PROTOSTORE_FILE_STORAGE_H_
#define PROTOSTORE_FILE_STORAGE_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace protostore {

/// \brief Supports sequential reading from a file.
class InputStream {
 public:
  InputStream(absl::string_view filename, FILE* file);
  InputStream(const InputStream&) = delete;
  InputStream& operator=(const InputStream&) = delete;
  ~InputStream();

  /// \brief Reads up to `n` bytes from the file starting at the current offset.
  ///
  /// `scratch[0..n-1]` may be written by this routine.  Sets `*result`
  /// to the data that was read (including if fewer than `n` bytes were
  /// successfully read).  May set `*result` to point at data in
  /// `scratch[0..n-1]`, so `scratch[0..n-1]` must be live when
  /// `*result` is used. result->data() is guaranteed to point to scratch[0]
  /// when data is successfully read.
  ///
  /// On OK returned status: `n` bytes have been stored in `*result`.
  /// On OUT_OF_RANGE returned status: encounted EOF before reading `n` bytes.
  ///   `n` minus `result.size()` bytes stored in `result`.
  /// On other non-OK returned status: `[0..n]` bytes have been stored in
  /// `*result`.
  ///
  /// Safe for concurrent use by multiple threads.
  absl::Status Read(size_t n, absl::string_view* result, char* scratch);

 private:
  std::string filename_;
  FILE* file_;
};

/// \brief Supports sequential writing to a file.
class OutputStream {
 public:
  OutputStream(absl::string_view filename, FILE* file);
  OutputStream(const OutputStream&) = delete;
  OutputStream& operator=(const OutputStream&) = delete;
  /// \brief Flushes and closes the file if it has not been closed.
  ~OutputStream();

  /// \brief Append 'data' to the file.
  absl::Status Append(absl::string_view data);

  /// \brief Close the file.
  ///
  /// Flush() and de-allocate resources associated with this file
  ///
  /// Typical return codes (not guaranteed to be exhaustive):
  ///  * OK
  ///  * Other codes, as returned from Flush()
  absl::Status Close();
 private:
  std::string filename_;
  FILE* file_;
};

/// \brief An lightweight interface to access the filesystem based on MobStore
/// File C++.
class FileStorage {
 public:
  FileStorage() = default;
  FileStorage(const FileStorage&) = delete;
  FileStorage& operator=(const FileStorage&) = delete;
  ~FileStorage() = default;

  /// Returns the file size or error.
  absl::StatusOr<uint64_t> GetFileSize(const std::string& filename) const;

  /// Returns the file opened for sequential read, or error. The file is
  /// closed when the input stream goes out of scope.
  absl::StatusOr<std::unique_ptr<InputStream>> OpenForRead(
      const std::string& filename) const;

  /// Returns the file opened for sequential write, or error. The file is
  /// closed when the output stream goes out of scope (or Close() is called).
  absl::StatusOr<std::unique_ptr<OutputStream>> OpenForWrite(
      const std::string& filename) const;
};

}  // namespace protostore

#endif  // PROTOSTORE_FILE_STORAGE_H_
