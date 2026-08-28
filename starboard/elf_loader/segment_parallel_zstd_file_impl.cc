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

#include "starboard/elf_loader/segment_parallel_zstd_file_impl.h"

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

SegmentParallelZstdFileImpl::SegmentParallelZstdFileImpl()
    : total_decompressed_size_(0),
      burst_buffer_(nullptr),
      burst_buffer_size_(0),
      current_items_(nullptr),
      stop_workers_(false),
      worker_success_(true) {
  cache_.valid = false;
  cache_.frame_index = 0;
  
  pthread_mutex_init(&mutex_, nullptr);
  pthread_cond_init(&cond_work_ready_, nullptr);
  pthread_cond_init(&cond_work_done_, nullptr);
}

SegmentParallelZstdFileImpl::~SegmentParallelZstdFileImpl() {
  // Signal workers to shutdown
  pthread_mutex_lock(&mutex_);
  stop_workers_ = true;
  pthread_cond_broadcast(&cond_work_ready_);
  pthread_mutex_unlock(&mutex_);

  for (pthread_t thread : workers_) {
    pthread_join(thread, nullptr);
  }

  if (burst_buffer_) {
    free(burst_buffer_);
  }
  
  pthread_mutex_destroy(&mutex_);
  pthread_cond_destroy(&cond_work_ready_);
  pthread_cond_destroy(&cond_work_done_);
}

bool SegmentParallelZstdFileImpl::Open(const char* name) {
  if (!FileImpl::Open(name)) return false;

  struct stat st;
  if (fstat(file_, &st) != 0) return false;
  burst_buffer_size_ = st.st_size;

  SB_LOG(INFO) << "Segment Zstd: Burst-reading " << burst_buffer_size_ << " bytes...";
  int64_t t0 = CurrentMonotonicTime();
  
  if (posix_memalign(&burst_buffer_, 4096, burst_buffer_size_) != 0) {
    return false;
  }
  
  if (!FileImpl::ReadFromOffset(0, static_cast<char*>(burst_buffer_), burst_buffer_size_)) {
    return false;
  }
  int64_t io_duration_ms = (CurrentMonotonicTime() - t0) / 1000;
  SB_LOG(INFO) << "Segment Zstd: Sequential I/O took " << io_duration_ms << " ms";

  if (!ScanFrames()) return false;

  SB_LOG(INFO) << "Segment Zstd: Found " << metadata_.size() << " frames.";

  // Spawn the persistent thread pool
  for (int i = 0; i < kNumWorkers; ++i) {
    pthread_t thread;
    if (pthread_create(&thread, nullptr, &WorkerThreadLoop, this) == 0) {
      workers_.push_back(thread);
    }
  }

  return !workers_.empty();
}

bool SegmentParallelZstdFileImpl::ScanFrames() {
  const char* data = static_cast<const char*>(burst_buffer_);
  size_t current_pos = 0;
  size_t current_decomp_offset = 0;

  while (current_pos < burst_buffer_size_) {
    size_t const remaining = burst_buffer_size_ - current_pos;
    size_t const cSize = ZSTD_findFrameCompressedSize(data + current_pos, remaining);
    if (ZSTD_isError(cSize)) break;

    unsigned long long const dSize = ZSTD_getFrameContentSize(data + current_pos, cSize);
    if (dSize != ZSTD_CONTENTSIZE_ERROR && dSize != ZSTD_CONTENTSIZE_UNKNOWN) {
      metadata_.push_back({static_cast<off_t>(current_pos), cSize, current_decomp_offset, static_cast<size_t>(dSize)});
      current_decomp_offset += dSize;
    }
    current_pos += cSize;
  }
  total_decompressed_size_ = current_decomp_offset;
  return !metadata_.empty();
}

void* SegmentParallelZstdFileImpl::WorkerThreadLoop(void* context) {
  SegmentParallelZstdFileImpl* impl = static_cast<SegmentParallelZstdFileImpl*>(context);
  ZSTD_DCtx* dctx = ZSTD_createDCtx();
  
  const size_t kScratchSize = 4 * 1024 * 1024;
  std::unique_ptr<char[]> scratch(new char[kScratchSize]);

  while (true) {
    pthread_mutex_lock(&impl->mutex_);
    while (impl->next_item_index_ >= (impl->current_items_ ? impl->current_items_->size() : 0) && !impl->stop_workers_) {
      pthread_cond_wait(&impl->cond_work_ready_, &impl->mutex_);
    }

    if (impl->stop_workers_) {
      pthread_mutex_unlock(&impl->mutex_);
      break;
    }

    size_t idx = impl->next_item_index_.fetch_add(1);
    if (idx >= impl->current_items_->size()) {
       pthread_mutex_unlock(&impl->mutex_);
       continue;
    }
    pthread_mutex_unlock(&impl->mutex_);

    const WorkItem& item = (*impl->current_items_)[idx];
    const char* src = static_cast<const char*>(impl->burst_buffer_) + item.meta->compressed_offset;

    bool local_success = true;
    if (item.direct) {
      size_t const result = ZSTD_decompressDCtx(
          dctx, item.dest, item.meta->decompressed_size, src, item.meta->compressed_size);
      if (ZSTD_isError(result)) local_success = false;
    } else {
      size_t const result = ZSTD_decompressDCtx(
          dctx, scratch.get(), kScratchSize, src, item.meta->compressed_size);
      if (ZSTD_isError(result)) {
        local_success = false;
      } else {
        memcpy(item.dest, scratch.get() + item.copy_offset, item.copy_size);
      }
    }

    if (!local_success) impl->worker_success_ = false;

    if (impl->items_remaining_.fetch_sub(1) == 1) {
       pthread_mutex_lock(&impl->mutex_);
       pthread_cond_signal(&impl->cond_work_done_);
       pthread_mutex_unlock(&impl->mutex_);
    }
  }

  ZSTD_freeDCtx(dctx);
  return nullptr;
}

bool SegmentParallelZstdFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (offset < 0 || static_cast<size_t>(offset + size) > total_decompressed_size_) {
    return false;
  }

  int64_t t0 = CurrentMonotonicTime();
  std::vector<WorkItem> items;
  int cached_count = 0;

  for (size_t i = 0; i < metadata_.size(); ++i) {
    const auto& meta = metadata_[i];
    size_t frame_start = meta.decompressed_offset;
    size_t frame_end = frame_start + meta.decompressed_size;
    size_t req_start = static_cast<size_t>(offset);
    size_t req_end = req_start + size;

    if (frame_end <= req_start || frame_start >= req_end) continue;

    if (cache_.valid && cache_.frame_index == i) {
      size_t overlap_start = std::max(frame_start, req_start);
      size_t overlap_end = std::min(frame_end, req_end);
      memcpy(buffer + (overlap_start - req_start), 
             cache_.data.get() + (overlap_start - frame_start), 
             overlap_end - overlap_start);
      cached_count++;
      continue;
    }

    WorkItem item;
    item.meta = &meta;
    size_t overlap_start = std::max(frame_start, req_start);
    size_t overlap_end = std::min(frame_end, req_end);
    item.dest = buffer + (overlap_start - req_start);
    item.copy_offset = overlap_start - frame_start;
    item.copy_size = overlap_end - overlap_start;
    
    item.direct = (frame_start >= req_start && frame_end <= req_end);
    items.push_back(item);

    // If this is a partial frame and it's the LAST one in the request,
    // it's highly likely to be the FIRST one in the NEXT request (overlap).
    // Let's cache it.
    if (!item.direct && overlap_end == req_end) {
       if (!cache_.data || cache_.size < meta.decompressed_size) {
         cache_.data.reset(new char[meta.decompressed_size]);
         cache_.size = meta.decompressed_size;
       }
       ZSTD_decompress(cache_.data.get(), meta.decompressed_size, 
                       static_cast<const char*>(burst_buffer_) + meta.compressed_offset, 
                       meta.compressed_size);
       cache_.frame_index = i;
       cache_.valid = true;
       memcpy(item.dest, cache_.data.get() + item.copy_offset, item.copy_size);
       items.pop_back();
    }
  }

  if (!items.empty()) {
    pthread_mutex_lock(&mutex_);
    current_items_ = &items;
    next_item_index_ = 0;
    items_remaining_ = items.size();
    worker_success_ = true;
    pthread_cond_broadcast(&cond_work_ready_);
    
    while (items_remaining_ > 0) {
      pthread_cond_wait(&cond_work_done_, &mutex_);
    }
    current_items_ = nullptr;
    pthread_mutex_unlock(&mutex_);
    
    if (!worker_success_) return false;
  }

  int64_t duration_ms = (CurrentMonotonicTime() - t0) / 1000;
  SB_LOG(INFO) << "SegmentRead(off=" << offset << ", size=" << size 
               << ") took " << duration_ms << " ms. (Cached=" << cached_count 
               << ", Parallel=" << items.size() << ")";

  return true;
}

}  // namespace elf_loader
