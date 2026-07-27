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

#include "starboard/elf_loader/parallel_zstd_file_impl.h"

#include <pthread.h>
#include <sys/stat.h>
#include <string.h>
#include <algorithm>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/system.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

ParallelZstdFileImpl::ParallelZstdFileImpl() : decompressed_data_size_(0) {}

ParallelZstdFileImpl::~ParallelZstdFileImpl() {}

bool ParallelZstdFileImpl::Open(const char* name) {
  if (!FileImpl::Open(name)) return false;

  int64_t start_time_us = CurrentMonotonicTime();
  bool result = Decompress();
  int64_t duration_us = CurrentMonotonicTime() - start_time_us;

  SB_LOG(INFO) << "Parallel Zstd decompression took: " << duration_us / 1000 << " ms";
  return result;
}

bool ParallelZstdFileImpl::FindFrames(const char* data, size_t size, std::vector<FrameInfo>* frames) {
  size_t curr = 0;
  size_t decompressed_offset = 0;

  while (curr < size) {
    size_t const cSize = ZSTD_findFrameCompressedSize(data + curr, size - curr);
    if (ZSTD_isError(cSize)) return false;

    unsigned long long const dSize = ZSTD_getFrameContentSize(data + curr, cSize);
    if (dSize == ZSTD_CONTENTSIZE_ERROR || dSize == ZSTD_CONTENTSIZE_UNKNOWN) {
      curr += cSize;
      continue;
    }

    frames->push_back({curr, cSize, decompressed_offset, static_cast<size_t>(dSize)});
    decompressed_offset += dSize;
    curr += cSize;
  }
  return !frames->empty();
}

void* ParallelZstdFileImpl::DecompressFrameThread(void* context) {
  ThreadContext* ctx = static_cast<ThreadContext*>(context);
  ZSTD_DCtx* dctx = ZSTD_createDCtx();
  
  while (true) {
    size_t frame_idx = ctx->next_frame_index->fetch_add(1);
    if (frame_idx >= ctx->frames->size()) break;

    const FrameInfo& frame = (*ctx->frames)[frame_idx];
    size_t const result = ZSTD_decompressDCtx(
        dctx,
        ctx->impl->decompressed_data_.get() + frame.decompressed_offset,
        frame.decompressed_size,
        ctx->compressed_data_ptr + frame.compressed_offset,
        frame.compressed_size);

    if (ZSTD_isError(result)) {
      ctx->success = false;
      break;
    }
  }

  ZSTD_freeDCtx(dctx);
  return nullptr;
}

bool ParallelZstdFileImpl::Decompress() {
  struct stat st;
  if (fstat(file_, &st) != 0) return false;
  size_t file_size = st.st_size;

  int64_t t_io_start = CurrentMonotonicTime();
  std::unique_ptr<char[]> compressed_data(new char[file_size]);
  if (!FileImpl::ReadFromOffset(0, compressed_data.get(), file_size)) return false;
  int64_t t_io_duration = CurrentMonotonicTime() - t_io_start;

  SB_LOG(INFO) << "Parallel Zstd I/O (Read from disk): " << t_io_duration / 1000 << " ms";

  std::vector<FrameInfo> frames;
  if (!FindFrames(compressed_data.get(), file_size, &frames)) {
    SB_LOG(ERROR) << "Zstd: No valid frames found in compressed file";
    return false;
  }

  SB_LOG(INFO) << "Zstd Parallel: Found " << frames.size() << " frames. Using thread pool.";

  size_t total_decompressed_size = 0;
  for (const auto& f : frames) total_decompressed_size += f.decompressed_size;
  
  decompressed_data_.reset(new char[total_decompressed_size]);
  decompressed_data_size_ = total_decompressed_size;

  std::atomic<size_t> next_frame_index(0);
  const int kNumWorkers = 4; // RDK / RPi2 are quad-core
  std::vector<pthread_t> threads(kNumWorkers);
  std::vector<ThreadContext> contexts(kNumWorkers);

  for (int i = 0; i < kNumWorkers; ++i) {
    contexts[i].impl = this;
    contexts[i].frames = &frames;
    contexts[i].compressed_data_ptr = compressed_data.get();
    contexts[i].next_frame_index = &next_frame_index;
    contexts[i].success = true;

    if (pthread_create(&threads[i], nullptr, &DecompressFrameThread, &contexts[i]) != 0) {
      return false;
    }
  }

  bool all_success = true;
  for (int i = 0; i < kNumWorkers; ++i) {
    pthread_join(threads[i], nullptr);
    if (!contexts[i].success) all_success = false;
  }

  return all_success;
}

bool ParallelZstdFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (offset < 0 || static_cast<size_t>(offset + size) > decompressed_data_size_) {
    return false;
  }
  memcpy(buffer, decompressed_data_.get() + offset, size);
  return true;
}

}  // namespace elf_loader
