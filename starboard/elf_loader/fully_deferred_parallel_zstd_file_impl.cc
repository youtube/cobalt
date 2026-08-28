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

#include "starboard/elf_loader/fully_deferred_parallel_zstd_file_impl.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

FullyDeferredParallelZstdFileImpl::FullyDeferredParallelZstdFileImpl()
    : total_decompressed_size_(0) {
  cache_.valid = false;
  cache_.frame_index = 0;
}

FullyDeferredParallelZstdFileImpl::~FullyDeferredParallelZstdFileImpl() {}

bool FullyDeferredParallelZstdFileImpl::Open(const char* name) {
  if (!FileImpl::Open(name)) return false;

  if (!ScanFrames()) return false;

  SB_LOG(INFO) << "Fully Deferred Zstd: Found " << metadata_.size() << " frames. Total size: " << total_decompressed_size_;
  return true;
}

bool FullyDeferredParallelZstdFileImpl::ScanFrames() {
  struct stat st;
  if (fstat(file_, &st) != 0) return false;
  off_t file_size = st.st_size;

  // Use mmap briefly just to build the frame map. 
  // This does not consume physical RAM for the whole file.
  void* mapped = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, file_, 0);
  if (mapped == MAP_FAILED) return false;

  const char* data = static_cast<const char*>(mapped);
  size_t current_pos = 0;
  size_t current_decomp_offset = 0;

  while (current_pos < static_cast<size_t>(file_size)) {
    size_t const remaining = static_cast<size_t>(file_size) - current_pos;
    size_t const cSize = ZSTD_findFrameCompressedSize(data + current_pos, remaining);
    if (ZSTD_isError(cSize)) {
      SB_LOG(ERROR) << "Fully Deferred Scan error at " << current_pos << ": " << ZSTD_getErrorName(cSize);
      break;
    }

    unsigned long long const dSize = ZSTD_getFrameContentSize(data + current_pos, cSize);
    if (dSize != ZSTD_CONTENTSIZE_ERROR && dSize != ZSTD_CONTENTSIZE_UNKNOWN) {
      metadata_.push_back({static_cast<off_t>(current_pos), cSize, current_decomp_offset, static_cast<size_t>(dSize)});
      current_decomp_offset += dSize;
    }
    current_pos += cSize;
  }
  
  munmap(mapped, file_size);
  total_decompressed_size_ = current_decomp_offset;
  return !metadata_.empty();
}

void* FullyDeferredParallelZstdFileImpl::DecompressThread(void* context) {
  ThreadContext* ctx = static_cast<ThreadContext*>(context);
  ZSTD_DCtx* dctx = ZSTD_createDCtx();
  
  const size_t kMaxCompressedSize = 4 * 1024 * 1024;
  const size_t kScratchSize = 4 * 1024 * 1024;
  
  void* comp_ptr = nullptr;
  posix_memalign(&comp_ptr, 4096, kMaxCompressedSize);
  std::unique_ptr<char[], void(*)(void*)> compressed_buf(static_cast<char*>(comp_ptr), free);
  
  std::unique_ptr<char[]> scratch(new char[kScratchSize]);

  while (true) {
    size_t idx = ctx->next_item_index->fetch_add(1);
    if (idx >= ctx->items->size()) break;

    const WorkItem& item = (*ctx->items)[idx];
    
    if (pread(ctx->fd, compressed_buf.get(), item.meta->compressed_size, item.meta->compressed_offset) != (ssize_t)item.meta->compressed_size) {
      ctx->success = false;
      break;
    }

    if (item.direct) {
      size_t const result = ZSTD_decompressDCtx(
          dctx, item.dest, item.meta->decompressed_size, compressed_buf.get(), item.meta->compressed_size);
      if (ZSTD_isError(result)) ctx->success = false;
    } else {
      size_t const result = ZSTD_decompressDCtx(
          dctx, scratch.get(), kScratchSize, compressed_buf.get(), item.meta->compressed_size);
      if (ZSTD_isError(result)) {
        ctx->success = false;
      } else {
        memcpy(item.dest, scratch.get() + item.copy_offset, item.copy_size);
      }
    }
  }

  ZSTD_freeDCtx(dctx);
  return nullptr;
}

bool FullyDeferredParallelZstdFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
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

    if (!item.direct && overlap_end == req_end) {
       if (!cache_.data || cache_.size < meta.decompressed_size) {
         cache_.data.reset(new char[meta.decompressed_size]);
         cache_.size = meta.decompressed_size;
       }
       std::unique_ptr<char[]> temp_comp(new char[meta.compressed_size]);
       pread(file_, temp_comp.get(), meta.compressed_size, meta.compressed_offset);
       ZSTD_decompress(cache_.data.get(), meta.decompressed_size, temp_comp.get(), meta.compressed_size);
       
       cache_.frame_index = i;
       cache_.valid = true;
       memcpy(item.dest, cache_.data.get() + item.copy_offset, item.copy_size);
       items.pop_back();
    }
  }

  if (!items.empty()) {
    std::atomic<size_t> next_item_index(0);
    ThreadContext ctx = {this, &items, &next_item_index, file_, true};
    const int kNumWorkers = 4;
    std::vector<pthread_t> threads(kNumWorkers);
    for (int j = 0; j < kNumWorkers; ++j) {
      pthread_create(&threads[j], nullptr, &DecompressThread, &ctx);
    }
    for (int j = 0; j < kNumWorkers; ++j) {
      pthread_join(threads[j], nullptr);
    }
    if (!ctx.success) return false;
  }

  int64_t duration_ms = (CurrentMonotonicTime() - t0) / 1000;
  SB_LOG(INFO) << "FullyDeferredRead(off=" << offset << ", size=" << size 
               << ") took " << duration_ms << " ms. (Cached=" << cached_count 
               << ", Parallel=" << items.size() << ")";

  return true;
}

}  // namespace elf_loader
