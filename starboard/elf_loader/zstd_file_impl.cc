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

#include "starboard/elf_loader/zstd_file_impl.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

ZstdFileImpl::ZstdFileImpl()
    : total_decompressed_size_(0),
      compressed_data_(nullptr),
      compressed_data_size_(0),
      current_tasks_(nullptr),
      stop_workers_(false),
      worker_failure_(false) {
  SB_CHECK_EQ(pthread_mutex_init(&worker_mutex_, nullptr), 0);
  SB_CHECK_EQ(pthread_cond_init(&work_ready_cv_, nullptr), 0);
  SB_CHECK_EQ(pthread_cond_init(&work_done_cv_, nullptr), 0);
}

ZstdFileImpl::~ZstdFileImpl() {
  SB_CHECK_EQ(pthread_mutex_lock(&worker_mutex_), 0);
  stop_workers_ = true;
  SB_CHECK_EQ(pthread_cond_broadcast(&work_ready_cv_), 0);
  SB_CHECK_EQ(pthread_mutex_unlock(&worker_mutex_), 0);

  for (pthread_t thread : workers_) {
    pthread_join(thread, nullptr);
  }

  if (compressed_data_) {
    free(compressed_data_);
  }

  SB_CHECK_EQ(pthread_mutex_destroy(&worker_mutex_), 0);
  SB_CHECK_EQ(pthread_cond_destroy(&work_ready_cv_), 0);
  SB_CHECK_EQ(pthread_cond_destroy(&work_done_cv_), 0);
}

bool ZstdFileImpl::Open(const char* name) {
  if (!FileImpl::Open(name)) {
    return false;
  }

  struct stat st;
  if (fstat(file_, &st) != 0) {
    return false;
  }
  if (st.st_size <= 0 ||
      static_cast<uint64_t>(st.st_size) > std::numeric_limits<size_t>::max()) {
    return false;
  }
  compressed_data_size_ = static_cast<size_t>(st.st_size);

  SB_LOG(INFO) << "Reading " << compressed_data_size_
               << " bytes from zstd file...";
  int64_t read_start_time_us = CurrentMonotonicTime();

  // Page-aligning the buffer enables zero-copy storage DMA transfers and
  // optimizes memory throughput during SIMD frame decompression.
  if (posix_memalign(&compressed_data_, 4096, compressed_data_size_) != 0) {
    return false;
  }

  if (!FileImpl::ReadFromOffset(0, static_cast<char*>(compressed_data_),
                                compressed_data_size_)) {
    return false;
  }
  int64_t read_duration_ms =
      (CurrentMonotonicTime() - read_start_time_us) / 1000;
  SB_LOG(INFO) << "Reading zstd file took " << read_duration_ms << " ms";

  if (!ScanFrames()) {
    return false;
  }

  SB_LOG(INFO) << "Found " << metadata_.size() << " frames.";

  for (int i = 0; i < kNumWorkers; ++i) {
    pthread_t thread;
    if (pthread_create(&thread, nullptr, &ClaimDecompressionTasks, this) == 0) {
      workers_.push_back(thread);
    }
  }

  return !workers_.empty();
}

bool ZstdFileImpl::ScanFrames() {
  const char* data = static_cast<const char*>(compressed_data_);
  size_t frame_compressed_offset = 0;
  size_t frame_decompressed_offset = 0;

  while (frame_compressed_offset < compressed_data_size_) {
    size_t const remaining = compressed_data_size_ - frame_compressed_offset;
    size_t const frame_compressed_size =
        ZSTD_findFrameCompressedSize(data + frame_compressed_offset, remaining);
    if (ZSTD_isError(frame_compressed_size)) {
      SB_LOG(ERROR) << "Zstd frame header error: "
                    << ZSTD_getErrorName(frame_compressed_size);
      return false;
    }

    unsigned long long const frame_decompressed_size = ZSTD_getFrameContentSize(
        data + frame_compressed_offset, frame_compressed_size);
    if (frame_decompressed_size == ZSTD_CONTENTSIZE_ERROR ||
        frame_decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
      SB_LOG(ERROR) << "Invalid or unknown content size in header";
      return false;
    }
    if (frame_decompressed_size > std::numeric_limits<size_t>::max()) {
      SB_LOG(ERROR) << "Decompressed frame size exceeds size_t limits";
      return false;
    }
    if (frame_decompressed_size > kMaxFrameDecompressedSize) {
      SB_LOG(ERROR) << "Decompressed frame size (" << frame_decompressed_size
                    << ") exceeds maximum allowed size ("
                    << kMaxFrameDecompressedSize << ")";
      return false;
    }

    metadata_.push_back({frame_compressed_offset, frame_compressed_size,
                         frame_decompressed_offset,
                         static_cast<size_t>(frame_decompressed_size)});
    frame_decompressed_offset += static_cast<size_t>(frame_decompressed_size);
    frame_compressed_offset += frame_compressed_size;
  }
  total_decompressed_size_ = frame_decompressed_offset;
  return !metadata_.empty();
}

bool ZstdFileImpl::HasTasksToProcess() const {
  return current_tasks_ != nullptr && next_task_index_ < current_tasks_->size();
}

void* ZstdFileImpl::ClaimDecompressionTasks(void* context) {
  ZstdFileImpl* impl = static_cast<ZstdFileImpl*>(context);
  ZSTD_DCtx* dctx = ZSTD_createDCtx();
  SB_CHECK(dctx) << "ZSTD_createDCtx() failed!";

  std::unique_ptr<char[]> scratch(new char[impl->kMaxFrameDecompressedSize]);

  while (true) {
    SB_CHECK_EQ(pthread_mutex_lock(&impl->worker_mutex_), 0);
    while (!impl->HasTasksToProcess() && !impl->stop_workers_) {
      SB_CHECK_EQ(
          pthread_cond_wait(&impl->work_ready_cv_, &impl->worker_mutex_), 0);
    }

    if (impl->stop_workers_) {
      SB_CHECK_EQ(pthread_mutex_unlock(&impl->worker_mutex_), 0);
      break;
    }

    size_t total_current_tasks = impl->current_tasks_->size();
    SB_CHECK_EQ(pthread_mutex_unlock(&impl->worker_mutex_), 0);

    size_t index = impl->next_task_index_.fetch_add(1);
    if (index >= total_current_tasks) {
      // All tasks for the active ReadFromOffset() request have been claimed.
      continue;
    }

    const DecompressionTask& item = (*impl->current_tasks_)[index];
    const char* src = static_cast<const char*>(impl->compressed_data_) +
                      item.frame_metadata->compressed_offset;

    bool local_failure = false;
    if (item.direct) {
      size_t const result = ZSTD_decompressDCtx(
          dctx, item.destination, item.frame_metadata->decompressed_size, src,
          item.frame_metadata->compressed_size);
      if (ZSTD_isError(result)) {
        SB_LOG(ERROR) << "Zstd decompression error: "
                      << ZSTD_getErrorName(result);
        local_failure = true;
      } else if (result != item.frame_metadata->decompressed_size) {
        SB_LOG(ERROR) << "Zstd decompression size mismatch. Expected "
                      << item.frame_metadata->decompressed_size << " but got "
                      << result;
        local_failure = true;
      }
    } else {
      size_t const result = ZSTD_decompressDCtx(
          dctx, scratch.get(), impl->kMaxFrameDecompressedSize, src,
          item.frame_metadata->compressed_size);
      if (ZSTD_isError(result)) {
        SB_LOG(ERROR) << "Zstd scratch decompression error: "
                      << ZSTD_getErrorName(result);
        local_failure = true;
      } else if (result != item.frame_metadata->decompressed_size) {
        SB_LOG(ERROR) << "Zstd scratch decompression size mismatch. Expected "
                      << item.frame_metadata->decompressed_size << " but got "
                      << result;
        local_failure = true;
      } else {
        memcpy(item.destination, scratch.get() + item.copy_offset,
               item.copy_size);
      }
    }

    if (local_failure) {
      impl->worker_failure_ = true;
    }

    if (impl->tasks_remaining_.fetch_sub(1) == 1) {
      // The final decompression task for this ReadFromOffset() request is done.
      SB_CHECK_EQ(pthread_mutex_lock(&impl->worker_mutex_), 0);
      SB_CHECK_EQ(pthread_cond_signal(&impl->work_done_cv_), 0);
      SB_CHECK_EQ(pthread_mutex_unlock(&impl->worker_mutex_), 0);
    }
  }

  ZSTD_freeDCtx(dctx);
  return nullptr;
}

bool ZstdFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (file_ < 0) {
    return false;
  }
  if (offset < 0 || size < 0) {
    return false;
  }
  if (offset > static_cast<int64_t>(total_decompressed_size_) ||
      size > static_cast<int64_t>(total_decompressed_size_) - offset) {
    return false;
  }

  int64_t read_start_time_us = CurrentMonotonicTime();
  std::vector<DecompressionTask> tasks;

  size_t request_start = static_cast<size_t>(offset);
  size_t request_end = request_start + static_cast<size_t>(size);

  for (size_t i = 0; i < metadata_.size(); ++i) {
    const auto& meta = metadata_[i];
    size_t frame_start = meta.decompressed_offset;
    size_t frame_end = frame_start + meta.decompressed_size;

    if (frame_end <= request_start || frame_start >= request_end) {
      continue;
    }

    size_t intersection_start = std::max(frame_start, request_start);
    size_t intersection_end = std::min(frame_end, request_end);

    DecompressionTask item;
    item.frame_metadata = &meta;
    item.destination = buffer + (intersection_start - request_start);
    item.copy_offset = intersection_start - frame_start;
    item.copy_size = intersection_end - intersection_start;

    item.direct = (frame_start >= request_start && frame_end <= request_end);
    tasks.push_back(item);
  }

  SB_CHECK_EQ(pthread_mutex_lock(&worker_mutex_), 0);
  current_tasks_ = &tasks;
  next_task_index_ = 0;
  tasks_remaining_ = tasks.size();
  worker_failure_ = false;
  SB_CHECK_EQ(pthread_cond_broadcast(&work_ready_cv_), 0);

  while (tasks_remaining_ > 0) {
    SB_CHECK_EQ(pthread_cond_wait(&work_done_cv_, &worker_mutex_), 0);
  }
  current_tasks_ = nullptr;
  SB_CHECK_EQ(pthread_mutex_unlock(&worker_mutex_), 0);

  if (worker_failure_) {
    return false;
  }

  int64_t read_duration_ms =
      (CurrentMonotonicTime() - read_start_time_us) / 1000;
  SB_LOG(INFO) << "ReadFromOffset(off=" << offset << ", size=" << size
               << ") took " << read_duration_ms
               << " ms. (Frames=" << tasks.size() << ")";

  return true;
}

}  // namespace elf_loader
