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

#ifndef STARBOARD_ELF_LOADER_FULLY_DEFERRED_PARALLEL_ZSTD_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_FULLY_DEFERRED_PARALLEL_ZSTD_FILE_IMPL_H_

#include <atomic>
#include <memory>
#include <vector>
#include <pthread.h>

#include "starboard/elf_loader/file_impl.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "third_party/zstd/src/lib/zstd.h"

namespace elf_loader {

// Strategy 10: Fully Deferred Parallel Zstd Decompression.
// Reads compressed data from disk on-demand inside worker threads.
// This minimizes RAM usage by eliminating all intermediate buffers.
class FullyDeferredParallelZstdFileImpl : public FileImpl {
 public:
  FullyDeferredParallelZstdFileImpl();
  ~FullyDeferredParallelZstdFileImpl();

  bool Open(const char* name) override;
  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;

 private:
  struct FrameMetadata {
    off_t compressed_offset;
    size_t compressed_size;
    size_t decompressed_offset;
    size_t decompressed_size;
  };

  struct WorkItem {
    const FrameMetadata* meta;
    char* dest;
    size_t dest_size;
    bool direct;
    size_t copy_offset;
    size_t copy_size;
  };

  struct ThreadContext {
    FullyDeferredParallelZstdFileImpl* impl;
    const std::vector<WorkItem>* items;
    std::atomic<size_t>* next_item_index;
    int fd;
    bool success;
  };

  bool ScanFrames();
  static void* DecompressThread(void* context);

  std::vector<FrameMetadata> metadata_;
  size_t total_decompressed_size_;

  struct Cache {
    size_t frame_index;
    std::unique_ptr<char[]> data;
    size_t size;
    bool valid;
  } cache_;
};

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_FULLY_DEFERRED_PARALLEL_ZSTD_FILE_IMPL_H_
