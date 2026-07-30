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

#ifndef STARBOARD_ELF_LOADER_ZSTD_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_ZSTD_FILE_IMPL_H_

#include <pthread.h>
#include <sys/types.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include "starboard/elf_loader/file_impl.h"
#include "third_party/zstd/src/lib/zstd.h"

namespace elf_loader {

// Provides opening and reading of a Zstandard compressed file.
//
// Decompression is deferred until bytes are requested by ReadFromOffset(), and
// multithreaded to parallelize the CPU intensive work across CPU cores. The
// function comments below describe the algorithm in more detail.
//
// It is assumed that the file consists of one or more valid Zstandard frames,
// and that no frame exceeds 4MB in decompressed size. To keep frames under this
// decompressed size limit and to promote smooth parallelization, it is
// recommended to break the compressed ELF file into at least 64 frames.
//
// Threading Model:
// While this class has a multithreaded implementation it is not itself thread-
// safe for concurrent use by multiple threads.
class ZstdFileImpl : public FileImpl {
 public:
  ZstdFileImpl();
  ~ZstdFileImpl() override;

  // A single thread opens the specified file, reads it into a memory buffer,
  // and scans the buffer to construct metadata describing the Zstandard frames.
  //
  // Returns false if the file cannot be opened or read, or if any Zstandard
  // frame header in the file is invalid or corrupted.
  bool Open(const char* name) override;

  // A single thread determines which frames are needed to satisfy the request,
  // creates a decompression task for each frame, and notifies worker threads
  // to pick up and complete the tasks, returning when the request is satisfied.
  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;

 private:
  struct FrameMetadata {
    // Byte offset of this frame in the compressed representation of the file.
    size_t compressed_offset;
    // Compressed size of this frame, in bytes.
    size_t compressed_size;
    // Byte offset of this frame in the uncompressed representation of the file.
    size_t decompressed_offset;
    // Uncompressed size of this frame, in bytes.
    size_t decompressed_size;
  };

  struct DecompressionTask {
    // Metadata describing the compressed frame to process.
    const FrameMetadata* frame_metadata;
    // Destination buffer where decompressed bytes will be written.
    char* destination;
    // True if frame is completely enclosed within the read request, allowing
    // decompression directly into the caller's target buffer.
    bool direct;
    // Byte offset within scratch buffer to copy from when direct is false.
    size_t copy_offset;
    // Number of bytes to copy from scratch buffer when direct is false.
    size_t copy_size;
  };

  bool ScanFrames();
  bool HasTasksToProcess() const;
  static void* ClaimDecompressionTasks(void* context);
  static bool DecompressFrame(ZSTD_DCtx* dctx,
                              void* dst,
                              size_t dst_capacity,
                              const void* src,
                              size_t src_size,
                              size_t expected_decompressed_size);

  // Maximum decompressed payload size allowed per frame. Compression tooling
  // must guarantee frames do not exceed this limit so that worker threads can
  // safely decompress non-direct frames into pre-allocated scratch buffers.
  static constexpr size_t kMaxFrameDecompressedSize = 4 * 1024 * 1024;

  std::vector<FrameMetadata> metadata_;
  size_t total_decompressed_size_;

  // Raw buffer holding the entire compressed file payload. A raw void* and
  // explicit size are used instead of std::vector to avoid the overhead of
  // zero-initializing a huge vector and to allow allocating page-aligned
  // memory via posix_memalign().
  void* compressed_data_;
  size_t compressed_data_size_;

  // Persistent Thread Pool state

  // This value is based on the 2028 HW requirement that devices have at least
  // four CPU cores, and our desire to smoothly parallelize decompression across
  // those cores.
  // TODO: b/497012299 - Consider sizing the thread pool at runtime based on
  // SbSystemGetNumberOfProcessors(), optionally with some sensible upper and
  // lower bounds. If devices have fewer than four available cores, we may be
  // incurring some unnecessary synchronization overhead; if devices have more
  // than four available cores, we may be under utilizing them.
  static constexpr int kNumWorkers = 4;
  std::vector<pthread_t> workers_;
  std::mutex worker_mutex_;
  std::condition_variable work_ready_cv_;
  std::condition_variable work_done_cv_;

  // Shared work state between threads
  // Points to the vector of tasks for the active ReadFromOffset() request.
  // Set to nullptr when the worker thread pool is idle.
  const std::vector<DecompressionTask>* current_tasks_;
  // Index of the next task to be claimed by a worker for this ReadFromOffset()
  // request. Incremented when a worker attempts to claim a task.
  std::atomic<size_t> next_task_index_{0};
  // Number of remaining decompression tasks for this ReadFromOffset() request.
  // Decremented when a worker completes a decompression task.
  std::atomic<size_t> tasks_remaining_{0};
  bool stop_workers_;
  // Set to true by any worker thread if a frame decompression error occurs.
  std::atomic<bool> worker_failure_;
};

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_ZSTD_FILE_IMPL_H_
