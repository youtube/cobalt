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

#ifndef STARBOARD_ELF_LOADER_PIPELINED_PARALLEL_ZSTD_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_PIPELINED_PARALLEL_ZSTD_FILE_IMPL_H_

#include <pthread.h>
#include <deque>
#include <memory>
#include <vector>

#include "starboard/elf_loader/file_impl.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "third_party/zstd/src/lib/zstd.h"

namespace elf_loader {

// This class decompresses a multi-frame Zstd file by overlapping disk I/O
// with parallel worker threads.
class PipelinedParallelZstdFileImpl : public FileImpl {
 public:
  PipelinedParallelZstdFileImpl();
  ~PipelinedParallelZstdFileImpl();

  bool Open(const char* name) override;
  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;

  struct FrameMetadata {
    off_t compressed_offset;
    size_t compressed_size;
    size_t decompressed_offset;
    size_t decompressed_size;
  };

 private:
  struct FrameWork {
    std::unique_ptr<char[]> compressed_data;
    FrameMetadata meta;
    bool is_sentinel;
  };

  static void* WorkerThreadEntry(void* context);
  void WorkerLoop();

  bool Decompress();
  bool ScanFrames(std::vector<FrameMetadata>* metadata, size_t* total_decompressed_size);

  std::unique_ptr<char[]> decompressed_data_;
  size_t decompressed_data_size_;

  // Thread-safe work queue
  std::deque<FrameWork> work_queue_;
  pthread_mutex_t queue_mutex_;
  pthread_cond_t queue_cond_;
  bool worker_success_;
};

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_PIPELINED_PARALLEL_ZSTD_FILE_IMPL_H_
