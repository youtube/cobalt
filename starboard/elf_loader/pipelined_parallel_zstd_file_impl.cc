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

#include "starboard/elf_loader/pipelined_parallel_zstd_file_impl.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <errno.h>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

PipelinedParallelZstdFileImpl::PipelinedParallelZstdFileImpl()
    : decompressed_data_size_(0), worker_success_(true) {
  pthread_mutex_init(&queue_mutex_, nullptr);
  pthread_cond_init(&queue_cond_, nullptr);
}

PipelinedParallelZstdFileImpl::~PipelinedParallelZstdFileImpl() {
  pthread_mutex_destroy(&queue_mutex_);
  pthread_cond_destroy(&queue_cond_);
}

bool PipelinedParallelZstdFileImpl::Open(const char* name) {
  if (!FileImpl::Open(name)) return false;

  int64_t start_time_us = CurrentMonotonicTime();
  bool result = Decompress();
  int64_t duration_us = CurrentMonotonicTime() - start_time_us;

  SB_LOG(INFO) << "Pipelined Parallel Zstd decompression took: " << duration_us / 1000 << " ms";
  return result;
}

bool PipelinedParallelZstdFileImpl::ScanFrames(std::vector<FrameMetadata>* metadata,
                                              size_t* total_decompressed_size) {
  struct stat st;
  if (fstat(file_, &st) != 0) return false;
  off_t file_size = st.st_size;

  // Map only for scanning to find boundaries
  void* mapped = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, file_, 0);
  if (mapped == MAP_FAILED) return false;

  const char* data = static_cast<const char*>(mapped);
  size_t current_pos = 0;
  size_t current_decomp_offset = 0;

  while (current_pos < static_cast<size_t>(file_size)) {
    size_t const remaining = static_cast<size_t>(file_size) - current_pos;
    size_t const cSize = ZSTD_findFrameCompressedSize(data + current_pos, remaining);
    if (ZSTD_isError(cSize)) break;

    unsigned long long const dSize = ZSTD_getFrameContentSize(data + current_pos, cSize);
    if (dSize != ZSTD_CONTENTSIZE_ERROR && dSize != ZSTD_CONTENTSIZE_UNKNOWN) {
      metadata->push_back({static_cast<off_t>(current_pos), cSize, current_decomp_offset, static_cast<size_t>(dSize)});
      current_decomp_offset += dSize;
    }
    current_pos += cSize;
  }

  munmap(mapped, file_size);
  *total_decompressed_size = current_decomp_offset;
  return !metadata->empty();
}

void* PipelinedParallelZstdFileImpl::WorkerThreadEntry(void* context) {
  static_cast<PipelinedParallelZstdFileImpl*>(context)->WorkerLoop();
  return nullptr;
}

void PipelinedParallelZstdFileImpl::WorkerLoop() {
  ZSTD_DCtx* dctx = ZSTD_createDCtx();

  while (true) {
    FrameWork work;
    
    pthread_mutex_lock(&queue_mutex_);
    while (work_queue_.empty()) {
      pthread_cond_wait(&queue_cond_, &queue_mutex_);
    }
    work = std::move(work_queue_.front());
    work_queue_.pop_front();
    pthread_mutex_unlock(&queue_mutex_);

    if (work.is_sentinel) break;

    size_t const result = ZSTD_decompressDCtx(
        dctx,
        decompressed_data_.get() + work.meta.decompressed_offset,
        work.meta.decompressed_size,
        work.compressed_data.get(),
        work.meta.compressed_size);

    if (ZSTD_isError(result)) {
      worker_success_ = false;
    }
  }

  ZSTD_freeDCtx(dctx);
}

bool PipelinedParallelZstdFileImpl::Decompress() {
  std::vector<FrameMetadata> metadata;
  size_t total_size = 0;
  if (!ScanFrames(&metadata, &total_size)) return false;

  SB_LOG(INFO) << "Pipelined Parallel: Found " << metadata.size() << " frames.";

  decompressed_data_.reset(new char[total_size]);
  decompressed_data_size_ = total_size;

  int64_t total_io_time_us = 0;
  size_t frames_pushed = 0;

  // 1. Pre-fill the queue with 4 frames before starting workers.
  // This ensures workers have a "warm" start.
  for (size_t i = 0; i < std::min(metadata.size(), (size_t)4); ++i) {
    FrameWork work;
    work.meta = metadata[i];
    work.is_sentinel = false;
    work.compressed_data.reset(new char[work.meta.compressed_size]);
    
    int64_t t0 = CurrentMonotonicTime();
    pread(file_, work.compressed_data.get(), work.meta.compressed_size, work.meta.compressed_offset);
    total_io_time_us += (CurrentMonotonicTime() - t0);
    
    work_queue_.push_back(std::move(work));
    frames_pushed++;
  }

  // 2. Start workers
  const int kNumWorkers = 3;
  std::vector<pthread_t> threads(kNumWorkers);
  for (int i = 0; i < kNumWorkers; ++i) {
    pthread_create(&threads[i], nullptr, &WorkerThreadEntry, this);
  }

  // 3. Continue feeding the pipeline
  for (size_t i = frames_pushed; i < metadata.size(); ++i) {
    FrameWork work;
    work.meta = metadata[i];
    work.is_sentinel = false;
    work.compressed_data.reset(new char[work.meta.compressed_size]);
    
    int64_t t0 = CurrentMonotonicTime();
    pread(file_, work.compressed_data.get(), work.meta.compressed_size, work.meta.compressed_offset);
    total_io_time_us += (CurrentMonotonicTime() - t0);

    pthread_mutex_lock(&queue_mutex_);
    work_queue_.push_back(std::move(work));
    pthread_cond_signal(&queue_cond_);
    pthread_mutex_unlock(&queue_mutex_);
  }

  // 4. Shutdown
  pthread_mutex_lock(&queue_mutex_);
  for (int i = 0; i < kNumWorkers; ++i) {
    FrameWork sentinel;
    sentinel.is_sentinel = true;
    work_queue_.push_back(std::move(sentinel));
  }
  pthread_cond_broadcast(&queue_cond_);
  pthread_mutex_unlock(&queue_mutex_);

  for (int i = 0; i < kNumWorkers; ++i) {
    pthread_join(threads[i], nullptr);
  }

  SB_LOG(INFO) << "Pipelined I/O Time (Overlapped): " << total_io_time_us / 1000 << " ms";

  return worker_success_;
}

bool PipelinedParallelZstdFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (offset < 0 || static_cast<size_t>(offset + size) > decompressed_data_size_) {
    return false;
  }
  memcpy(buffer, decompressed_data_.get() + offset, size);
  return true;
}

}  // namespace elf_loader
