// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/elf_loader/naive_zstd_file_impl.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <limits>

#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "third_party/zstd/src/lib/zstd.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

bool NaiveZstdFileImpl::Open(const char* name) {
  if (!FileImpl::Open(name)) {
    return false;
  }

  struct stat st;
  if (fstat(file_, &st) != 0) {
    return false;
  }
  if (st.st_size < 0 ||
      static_cast<uint64_t>(st.st_size) > std::numeric_limits<size_t>::max()) {
    return false;
  }
  size_t compressed_size = static_cast<size_t>(st.st_size);

  SB_LOG(INFO) << "Reading " << compressed_size << " bytes from zstd file...";
  int64_t read_start_time_us = CurrentMonotonicTime();
  std::vector<char> compressed_data(compressed_size);
  if (!FileImpl::ReadFromOffset(0, compressed_data.data(), compressed_size)) {
    return false;
  }
  int64_t read_duration_ms =
      (CurrentMonotonicTime() - read_start_time_us) / 1000;
  SB_LOG(INFO) << "Reading zstd file took " << read_duration_ms << " ms";

  size_t const first_frame_compressed_size =
      ZSTD_findFrameCompressedSize(compressed_data.data(), compressed_size);
  if (!ZSTD_isError(first_frame_compressed_size) &&
      first_frame_compressed_size < compressed_size) {
    SB_LOG(ERROR) << "Multi-frame Zstd file detected. Naive Zstd only "
                  << "supports single-frame files.";
    return false;
  }

  int64_t decompression_start_time_us = CurrentMonotonicTime();

  unsigned long long const decompressed_size =
      ZSTD_getFrameContentSize(compressed_data.data(), compressed_size);
  if (decompressed_size == ZSTD_CONTENTSIZE_ERROR ||
      decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
    SB_LOG(ERROR) << "Invalid or unknown content size in header";
    return false;
  }

  if (decompressed_size > std::numeric_limits<size_t>::max()) {
    SB_LOG(ERROR) << "Decompressed size " << decompressed_size
                  << " exceeds maximum representable size_t";
    return false;
  }

  decompressed_data_.resize(static_cast<size_t>(decompressed_size));

  SB_LOG(INFO) << "Decompressing frame with content size " << decompressed_size;
  size_t const result =
      ZSTD_decompress(decompressed_data_.data(), decompressed_size,
                      compressed_data.data(), compressed_size);
  int64_t decompression_duration_ms =
      (CurrentMonotonicTime() - decompression_start_time_us) / 1000;
  SB_LOG(INFO) << "Decompression took " << decompression_duration_ms << " ms";

  if (ZSTD_isError(result)) {
    SB_LOG(ERROR) << "Decompression error: " << ZSTD_getErrorName(result);
    return false;
  }

  if (result != decompressed_size) {
    SB_LOG(ERROR) << "Decompressed size mismatch. Expected "
                  << decompressed_size << " but got " << result;
    return false;
  }

  return true;
}

bool NaiveZstdFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (file_ < 0) {
    return false;
  }
  if (offset < 0 || size < 0) {
    return false;
  }
  if (offset > static_cast<int64_t>(decompressed_data_.size()) ||
      size > static_cast<int64_t>(decompressed_data_.size()) - offset) {
    return false;
  }
  memcpy(buffer, decompressed_data_.data() + offset, size);
  return true;
}

}  // namespace elf_loader
