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

#include "media/starboard/media_pool_direct_map.h"

#include "build/build_config.h"

#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
#include <sys/prctl.h>
#endif

#include <atomic>
#include <limits>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/media_switches.h"

namespace media {

namespace {

// -1: use the feature; 0: forced off; 1: forced on.
std::atomic<int> g_enabled_override{-1};
std::atomic<bool> g_map_fail{false};
std::atomic<size_t> g_map_max_bytes{std::numeric_limits<size_t>::max()};
std::atomic<bool> g_warned_table_full{false};

base::Lock& BlocksLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// Live blocks, by the start and size of the usable range (the guard pages
// are the page on each side). A fixed array rather than a container, so that
// tracking a block never allocates: on the soft-fail path a heap allocation
// could itself need a new PartitionAlloc super page and crash. The pool has
// far fewer blocks than this (a 200 MiB budget is 50 blocks of 4 MiB, or 400
// of 512 KiB at worst).
struct Block {
  uintptr_t start;
  size_t size;
};
constexpr size_t kMaxBlocks = MediaPoolDirectMap::kMaxBlocks;
Block g_blocks[kMaxBlocks] GUARDED_BY(BlocksLock());
size_t g_block_count GUARDED_BY(BlocksLock()) = 0;

bool TableFull() EXCLUSIVE_LOCKS_REQUIRED(BlocksLock()) {
  return g_block_count >= kMaxBlocks;
}

// Called without BlocksLock() held; logs once per process.
void WarnTableFull() {
  if (!g_warned_table_full.exchange(true, std::memory_order_relaxed)) {
    LOG(WARNING) << "Media pool: " << kMaxBlocks
                 << " blocks are mapped; further blocks fail.";
  }
}

void NameRegion(void* p, size_t size) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  // PR_SET_VMA, PR_SET_VMA_ANON_NAME; ignored by kernels without it.
  prctl(0x53564d41UL, 0UL, reinterpret_cast<unsigned long>(p),
        static_cast<unsigned long>(size),
        reinterpret_cast<unsigned long>("media-pool-block"));
#endif
}

}  // namespace

// static
bool MediaPoolDirectMap::IsEnabled() {
  const int override_value = g_enabled_override.load(std::memory_order_relaxed);
  if (override_value >= 0) {
    return override_value != 0;
  }
#if defined(ARCH_CPU_32_BITS)
  return base::FeatureList::IsEnabled(kCobaltMediaPoolSoftFail);
#else
  // 64-bit address spaces don't run out of 2 MiB-aligned holes.
  return false;
#endif
}

// static
void* MediaPoolDirectMap::Map(size_t* size) {
  if (*size == 0 || g_map_fail.load(std::memory_order_relaxed) ||
      *size > g_map_max_bytes.load(std::memory_order_relaxed)) {
    return nullptr;
  }
  const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  const size_t rounded = (*size + page - 1) & ~(page - 1);
  // A PROT_NONE guard page at each end, like PartitionAlloc's direct maps: an
  // overrun or underrun faults instead of reaching other data, and two blocks
  // are never adjacent, so the reuse allocator never merges distinct blocks.
  const size_t total = rounded + 2 * page;
  if (rounded < *size || total < rounded) {
    return nullptr;
  }
  bool full = false;
  {
    // Skip the system calls when the table is already full.
    base::AutoLock lock(BlocksLock());
    full = TableFull();
  }
  if (full) {
    WarnTableFull();
    return nullptr;
  }
  void* mapping =
      mmap(nullptr, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED) {
    return nullptr;
  }
  void* block = static_cast<uint8_t*>(mapping) + page;
  if (mprotect(block, rounded, PROT_READ | PROT_WRITE) != 0) {
    munmap(mapping, total);
    return nullptr;
  }
  bool tracked = false;
  {
    base::AutoLock lock(BlocksLock());
    // Another thread may have filled the table since the check above.
    if (!TableFull()) {
      g_blocks[g_block_count++] = {reinterpret_cast<uintptr_t>(block), rounded};
      tracked = true;
    }
  }
  if (!tracked) {
    munmap(mapping, total);
    WarnTableFull();
    return nullptr;
  }
  NameRegion(mapping, total);
  *size = rounded;
  return block;
}

// static
bool MediaPoolDirectMap::Unmap(void* memory) {
  const uintptr_t start = reinterpret_cast<uintptr_t>(memory);
  size_t size = 0;
  {
    base::AutoLock lock(BlocksLock());
    size_t i = 0;
    while (i < g_block_count && g_blocks[i].start != start) {
      ++i;
    }
    if (i == g_block_count) {
      return false;
    }
    size = g_blocks[i].size;
    g_blocks[i] = g_blocks[--g_block_count];
  }
  const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  munmap(reinterpret_cast<void*>(start - page), size + 2 * page);
  return true;
}

// static
void MediaPoolDirectMap::SetEnabledForTesting(std::optional<bool> enabled) {
  g_enabled_override.store(enabled.has_value() ? (*enabled ? 1 : 0) : -1,
                           std::memory_order_relaxed);
}

// static
void MediaPoolDirectMap::SetMapFailForTesting(bool fail) {
  g_map_fail.store(fail, std::memory_order_relaxed);
}

// static
void MediaPoolDirectMap::SetMapFailAboveForTesting(
    std::optional<size_t> max_bytes) {
  g_map_max_bytes.store(max_bytes.value_or(std::numeric_limits<size_t>::max()),
                        std::memory_order_relaxed);
}

// static
size_t MediaPoolDirectMap::BlockCountForTesting() {
  base::AutoLock lock(BlocksLock());
  return g_block_count;
}

}  // namespace media
