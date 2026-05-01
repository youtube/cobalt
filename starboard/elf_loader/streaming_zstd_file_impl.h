// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ELF_LOADER_STREAMING_ZSTD_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_STREAMING_ZSTD_FILE_IMPL_H_

#include <vector>

#include "starboard/elf_loader/file_impl.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "third_party/zstd/src/lib/zstd.h"

namespace elf_loader {

// This class provides opening and reading a file compressed using Zstd by
// decompressing data on-demand in a streaming fashion.
class StreamingZstdFileImpl : public FileImpl {
 public:
  StreamingZstdFileImpl();
  ~StreamingZstdFileImpl();

  // Opens the file specified.
  bool Open(const char* name) override;

  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;

 private:
  // Reset the decompression context and seek back to the beginning of the file.
  bool ResetStream();

  // Decompress until current_uncompressed_offset_ reaches target_offset.
  bool DecompressToOffset(int64_t target_offset);

  // The Zstd decompression context.
  ZSTD_DCtx* dctx_;

  // Current position in the virtual uncompressed file.
  int64_t current_uncompressed_offset_;

  // Buffer for reading compressed data from the physical file.
  std::vector<char> compressed_buf_;

  // Buffer for leftover decompressed data that hasn't been requested yet.
  std::vector<char> decompressed_buf_;

  // Offset into decompressed_buf_ where the leftover data starts.
  size_t decompressed_buf_pos_;
  // Amount of valid data currently in decompressed_buf_.
  size_t decompressed_buf_size_;

  // Profiling stats
  int64_t total_read_time_us_;
  int64_t total_zstd_time_us_;
  int rewind_count_;
};

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_STREAMING_ZSTD_FILE_IMPL_H_
