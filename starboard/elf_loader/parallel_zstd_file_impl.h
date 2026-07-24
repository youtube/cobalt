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

#ifndef STARBOARD_ELF_LOADER_PARALLEL_ZSTD_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_PARALLEL_ZSTD_FILE_IMPL_H_

#include <atomic>
#include <memory>
#include <vector>

#include "starboard/elf_loader/file_impl.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "third_party/zstd/src/lib/zstd.h"

namespace elf_loader {

// This class decompresses a multi-frame Zstd file using parallel Starboard threads.
class ParallelZstdFileImpl : public FileImpl {
 public:
  ParallelZstdFileImpl();
  ~ParallelZstdFileImpl();

  bool Open(const char* name) override;
  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;

 private:
  struct FrameInfo {
    size_t compressed_offset;
    size_t compressed_size;
    size_t decompressed_offset;
    size_t decompressed_size;
  };

  struct ThreadContext {
    ParallelZstdFileImpl* impl;
    const std::vector<FrameInfo>* frames;
    const char* compressed_data_ptr;
    std::atomic<size_t>* next_frame_index;
    bool success;
  };

  static void* DecompressFrameThread(void* context);
  bool Decompress();
  bool FindFrames(const char* data, size_t size, std::vector<FrameInfo>* frames);

  std::unique_ptr<char[]> decompressed_data_;
  size_t decompressed_data_size_;
};

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_PARALLEL_ZSTD_FILE_IMPL_H_
