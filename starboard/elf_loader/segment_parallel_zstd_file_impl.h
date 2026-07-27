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

#ifndef STARBOARD_ELF_LOADER_SEGMENT_PARALLEL_ZSTD_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_SEGMENT_PARALLEL_ZSTD_FILE_IMPL_H_

/**
 * Segment-Aware Parallel Zstd Decompression Orchestrator
 *
 * This implementation provides a high-performance, memory-efficient bridge 
 * between the Zstandard (Zstd) bitstream and the ELF Loader. It is specifically 
 * tuned for resource-constrained ARM devices where memory bandwidth and RAM 
 * are at a premium.
 *
 * Algorithmic Approach: "Deferred Segment-Parallel"
 * ------------------------------------------------
 * 1. Sequential Burst-Read: 
 *    To maximize disk controller throughput and avoid memory bus contention (which
 *    penalizes simultaneous CPU and Disk work by ~25%), the class performs a 
 *    single linear read of the compressed binary into a page-aligned RAM buffer 
 *    during the Open() call.
 *
 * 2. Structural Alignment:
 *    The implementation assumes the Zstd file has been compressed using 
 *    "Segment-Aware" framing (via make_segment_aware_zstd_frames.py). This 
 *    guarantees that Zstd frame boundaries align perfectly with ELF Program 
 *    Segments (LOAD segments).
 *
 * 3. On-Demand (Deferred) Decompression:
 *    Instead of expanding the whole binary into RAM, decompression only occurs 
 *    when ReadFromOffset() is called by the loader. This reduces peak transient 
 *    RSS by ~34MB compared to naive whole-file decompression.
 *
 * 4. Zero-Copy Parallel Expansion:
 *    When a segment is requested, the orchestrator identifies the corresponding 
 *    Zstd frames and distributes them across a persistent thread pool (4 worker 
 *    threads). Frames that are fully contained within the request are decompressed 
 *    directly into the final executable memory mapping, bypassing any 
 *    intermediate "double-buffering."
 *
 * 5. Overlap Cache:
 *    A lightweight, single-frame cache handles the rare cases where a Zstd 
 *    frame straddles two different loader requests. This ensures that even in 
 *    unaligned or overlapping scenarios, no frame is decompressed more than once.
 */

#include <atomic>
#include <memory>
#include <vector>
#include <pthread.h>

#include "starboard/elf_loader/file_impl.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "third_party/zstd/src/lib/zstd.h"

namespace elf_loader {

// Strategy 9: Sequential Burst-Read + Segment-Aware Parallel Decompression.
// Uses a persistent thread pool and one-frame cache for maximum efficiency.
class SegmentParallelZstdFileImpl : public FileImpl {
 public:
  SegmentParallelZstdFileImpl();
  ~SegmentParallelZstdFileImpl();

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

  bool ScanFrames();
  static void* WorkerThreadLoop(void* context);

  std::vector<FrameMetadata> metadata_;
  size_t total_decompressed_size_;
  void* burst_buffer_;
  size_t burst_buffer_size_;

  // Persistent Thread Pool state
  const int kNumWorkers = 4;
  std::vector<pthread_t> workers_;
  pthread_mutex_t mutex_;
  pthread_cond_t cond_work_ready_;
  pthread_cond_t cond_work_done_;
  
  // Shared work state between threads
  const std::vector<WorkItem>* current_items_;
  std::atomic<size_t> next_item_index_;
  std::atomic<size_t> items_remaining_;
  bool stop_workers_;
  bool worker_success_;

  // Simple one-frame cache
  struct Cache {
    size_t frame_index;
    std::unique_ptr<char[]> data;
    size_t size;
    bool valid;
  } cache_;
};

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_SEGMENT_PARALLEL_ZSTD_FILE_IMPL_H_
