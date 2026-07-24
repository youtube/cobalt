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

#include "starboard/elf_loader/contention_diagnostic_file_impl.h"

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

ContentionDiagnosticFileImpl::ContentionDiagnosticFileImpl() {}

ContentionDiagnosticFileImpl::~ContentionDiagnosticFileImpl() {}

bool ContentionDiagnosticFileImpl::Open(const char* name) {
  if (!FileImpl::Open(name)) return false;

  SB_LOG(INFO) << "=== STARTING MEMORY BUS CONTENTION DIAGNOSTIC ===";

  struct stat st;
  fstat(file_, &st);
  size_t file_size = st.st_size;
  const size_t kBlockSize = 1024 * 1024; // 1MB blocks
  size_t num_blocks = file_size / kBlockSize;

  // 1. Prepare data (Sequential I/O)
  std::vector<WorkBlock> blocks(num_blocks);
  for (size_t i = 0; i < num_blocks; ++i) {
    blocks[i].data.reset(new char[kBlockSize]);
    blocks[i].size = kBlockSize;
    pread(file_, blocks[i].data.get(), kBlockSize, i * kBlockSize);
  }

  // 2. PHASE A: Memory Work ALONE (Disk is idle)
  RunMemoryWork(blocks, "Phase A (CPU Work Alone)");

  // 3. PHASE B: Memory Work + Overlapped I/O (Disk is busy)
  // We'll spawn the memory workers while the main thread performs redundant reads
  SB_LOG(INFO) << "Starting Phase B (CPU Work + Simultaneous Disk I/O)...";
  
  std::atomic<size_t> next_block(0);
  uint64_t total_sum = 0;
  ThreadContext ctx = {this, &blocks, &next_block, &total_sum};

  const int kNumWorkers = 4;
  std::vector<pthread_t> threads(kNumWorkers);
  
  int64_t start_time_us = CurrentMonotonicTime();

  for (int i = 0; i < kNumWorkers; ++i) {
    pthread_create(&threads[i], nullptr, &WorkerThreadEntry, &ctx);
  }

  // Simulate heavy I/O in the main thread during the memory work
  std::unique_ptr<char[]> io_buf(new char[kBlockSize]);
  while (next_block.load() < num_blocks) {
    // Perform redundant reads from a different offset to keep the bus busy
    pread(file_, io_buf.get(), kBlockSize, 0);
  }

  for (int i = 0; i < kNumWorkers; ++i) {
    pthread_join(threads[i], nullptr);
  }
  
  int64_t duration_us = CurrentMonotonicTime() - start_time_us;
  SB_LOG(INFO) << "Phase B (Overlapped) took: " << duration_us / 1000 << " ms";
  
  SB_LOG(INFO) << "=== DIAGNOSTIC COMPLETE ===";

  // Returning false because we don't actually want to load the ELF
  return false;
}

void ContentionDiagnosticFileImpl::RunMemoryWork(const std::vector<WorkBlock>& blocks, const char* label) {
  std::atomic<size_t> next_block(0);
  uint64_t total_sum = 0;
  ThreadContext ctx = {this, &blocks, &next_block, &total_sum};

  const int kNumWorkers = 4;
  std::vector<pthread_t> threads(kNumWorkers);

  int64_t start_time_us = CurrentMonotonicTime();

  for (int i = 0; i < kNumWorkers; ++i) {
    pthread_create(&threads[i], nullptr, &WorkerThreadEntry, &ctx);
  }

  for (int i = 0; i < kNumWorkers; ++i) {
    pthread_join(threads[i], nullptr);
  }

  int64_t duration_us = CurrentMonotonicTime() - start_time_us;
  SB_LOG(INFO) << label << " took: " << duration_us / 1000 << " ms (sum: " << total_sum << ")";
}

void* ContentionDiagnosticFileImpl::WorkerThreadEntry(void* context) {
  ThreadContext* ctx = static_cast<ThreadContext*>(context);
  uint64_t local_sum = 0;

  while (true) {
    size_t idx = ctx->next_block->fetch_add(1);
    if (idx >= ctx->blocks->size()) break;

    const auto& block = (*ctx->blocks)[idx];
    // Perform a very memory-intensive loop to stress the cache and bus.
    // We do this 10 times per byte to simulate the relative complexity of Zstd
    // while keeping the access pattern entirely memory-bound.
    for (int repeat = 0; repeat < 50; ++repeat) {
      for (size_t j = 0; j < block.size; ++j) {
        local_sum += static_cast<uint8_t>(block.data[j]);
      }
    }
  }

  // We don't actually need the sum, just making sure the compiler doesn't 
  // optimize away the loop.
  __sync_fetch_and_add(ctx->result_sum, local_sum);
  return nullptr;
}

bool ContentionDiagnosticFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  return false;
}

}  // namespace elf_loader
