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

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

size_t PageSize() {
  return static_cast<size_t>(sysconf(_SC_PAGESIZE));
}

sigjmp_buf g_fault_jmp;

void FaultSignalHandler(int /*sig*/) {
  siglongjmp(g_fault_jmp, 1);
}

// Returns true if reading the byte at `addr` faults with SIGSEGV or SIGBUS.
bool ReadFaults(const void* addr) {
  struct sigaction action = {};
  struct sigaction old_segv = {};
  struct sigaction old_bus = {};
  action.sa_handler = FaultSignalHandler;
  sigemptyset(&action.sa_mask);
  sigaction(SIGSEGV, &action, &old_segv);
  sigaction(SIGBUS, &action, &old_bus);

  bool faulted = false;
  if (sigsetjmp(g_fault_jmp, /*savesigs=*/1) == 0) {
    volatile uint8_t byte = *static_cast<const volatile uint8_t*>(addr);
    (void)byte;
  } else {
    faulted = true;
  }

  sigaction(SIGSEGV, &old_segv, nullptr);
  sigaction(SIGBUS, &old_bus, nullptr);
  return faulted;
}

// Returns true if writing the byte at `addr` faults with SIGSEGV or SIGBUS.
bool WriteFaults(void* addr) {
  struct sigaction action = {};
  struct sigaction old_segv = {};
  struct sigaction old_bus = {};
  action.sa_handler = FaultSignalHandler;
  sigemptyset(&action.sa_mask);
  sigaction(SIGSEGV, &action, &old_segv);
  sigaction(SIGBUS, &action, &old_bus);

  bool faulted = false;
  if (sigsetjmp(g_fault_jmp, /*savesigs=*/1) == 0) {
    *static_cast<volatile uint8_t*>(addr) = 0;
  } else {
    faulted = true;
  }

  sigaction(SIGSEGV, &old_segv, nullptr);
  sigaction(SIGBUS, &old_bus, nullptr);
  return faulted;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
// Returns the 4-char permissions string (e.g. "---p" or "rw-p") from
// /proc/self/maps for the VMA covering `addr`, or std::nullopt if unmapped.
std::optional<std::string> ProcMapsPermsFor(uintptr_t addr) {
  std::ifstream maps("/proc/self/maps");
  std::string line;
  while (std::getline(maps, line)) {
    std::istringstream iss(line);
    std::string range;
    std::string perms;
    if (!(iss >> range >> perms)) {
      continue;
    }
    const size_t dash = range.find('-');
    if (dash == std::string::npos) {
      continue;
    }
    const uintptr_t start = static_cast<uintptr_t>(
        std::strtoull(range.substr(0, dash).c_str(), nullptr, 16));
    const uintptr_t end = static_cast<uintptr_t>(
        std::strtoull(range.substr(dash + 1).c_str(), nullptr, 16));
    if (addr >= start && addr < end) {
      return perms;
    }
  }
  return std::nullopt;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)


TEST(MediaPoolDirectMapTest, MapAndUnmap) {
  const size_t page = PageSize();

  size_t zero = 0;
  EXPECT_EQ(MediaPoolDirectMap::Map(&zero), nullptr);
  EXPECT_EQ(zero, 0u);

  size_t size = 100;
  void* p = MediaPoolDirectMap::Map(&size);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(size, page);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % page, 0u);

  memset(p, 0x7E, size);
  EXPECT_EQ(static_cast<const uint8_t*>(p)[0], 0x7E);
  EXPECT_EQ(static_cast<const uint8_t*>(p)[size - 1], 0x7E);

  int stack_var = 0;
  EXPECT_FALSE(MediaPoolDirectMap::Unmap(nullptr));
  EXPECT_FALSE(MediaPoolDirectMap::Unmap(&stack_var));

  EXPECT_TRUE(MediaPoolDirectMap::Unmap(p));
  EXPECT_FALSE(MediaPoolDirectMap::Unmap(p));

  errno = 0;
  EXPECT_EQ(msync(p, page, MS_ASYNC), -1);
  EXPECT_EQ(errno, ENOMEM);
}

TEST(MediaPoolDirectMapTest, BlocksAreSeparatedByGuardPages) {
  const size_t page = PageSize();
  constexpr int kCount = 8;
  std::vector<std::pair<uintptr_t, size_t>> blocks;
  for (int i = 0; i < kCount; ++i) {
    size_t size = 4 * page;
    void* p = MediaPoolDirectMap::Map(&size);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(size, 4 * page);
    auto* bytes = static_cast<uint8_t*>(p);
    // Leading and trailing guard pages must fault on read and write.
    EXPECT_TRUE(ReadFaults(bytes - 1));
    EXPECT_TRUE(WriteFaults(bytes - 1));
    EXPECT_TRUE(ReadFaults(bytes + size));
    EXPECT_TRUE(WriteFaults(bytes + size));
    blocks.emplace_back(reinterpret_cast<uintptr_t>(p), size);
  }

  std::sort(blocks.begin(), blocks.end());
  for (size_t i = 0; i + 1 < blocks.size(); ++i) {
    const uintptr_t prev_end = blocks[i].first + blocks[i].second;
    const uintptr_t next_start = blocks[i + 1].first;
    // Trailing guard page of block i + leading guard page of block i+1.
    EXPECT_GE(next_start, prev_end + 2 * page);
  }

  for (const auto& [addr, size] : blocks) {
    void* p = reinterpret_cast<void*>(addr);
    uint8_t* leading_guard = reinterpret_cast<uint8_t*>(addr - page);
    uint8_t* trailing_guard = reinterpret_cast<uint8_t*>(addr + size);
    EXPECT_TRUE(MediaPoolDirectMap::Unmap(p));
    errno = 0;
    EXPECT_EQ(msync(leading_guard, page, MS_ASYNC), -1);
    EXPECT_EQ(errno, ENOMEM);
    errno = 0;
    EXPECT_EQ(msync(p, size, MS_ASYNC), -1);
    EXPECT_EQ(errno, ENOMEM);
    errno = 0;
    EXPECT_EQ(msync(trailing_guard, page, MS_ASYNC), -1);
    EXPECT_EQ(errno, ENOMEM);
  }
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
TEST(MediaPoolDirectMapTest, GuardPagesInProcMaps) {
  const size_t page = PageSize();
  size_t size = 4 * page;
  void* p = MediaPoolDirectMap::Map(&size);
  ASSERT_NE(p, nullptr);
  ASSERT_EQ(size, 4 * page);

  const uintptr_t user_addr = reinterpret_cast<uintptr_t>(p);
  EXPECT_EQ(ProcMapsPermsFor(user_addr - page), "---p");
  EXPECT_EQ(ProcMapsPermsFor(user_addr - 1), "---p");
  EXPECT_EQ(ProcMapsPermsFor(user_addr), "rw-p");
  EXPECT_EQ(ProcMapsPermsFor(user_addr + size - 1), "rw-p");
  EXPECT_EQ(ProcMapsPermsFor(user_addr + size), "---p");
  EXPECT_EQ(ProcMapsPermsFor(user_addr + size + page - 1), "---p");

  EXPECT_TRUE(MediaPoolDirectMap::Unmap(p));
  EXPECT_EQ(ProcMapsPermsFor(user_addr - page), std::nullopt);
  EXPECT_EQ(ProcMapsPermsFor(user_addr), std::nullopt);
  EXPECT_EQ(ProcMapsPermsFor(user_addr + size), std::nullopt);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)

// Restores the test hooks even if an ASSERT returns early.
class ScopedMapHooksReset {
 public:
  ScopedMapHooksReset() = default;
  ScopedMapHooksReset(const ScopedMapHooksReset&) = delete;
  ScopedMapHooksReset& operator=(const ScopedMapHooksReset&) = delete;
  ~ScopedMapHooksReset() {
    MediaPoolDirectMap::SetMapFailForTesting(false);
    MediaPoolDirectMap::SetMapFailAboveForTesting(std::nullopt);
  }
};

TEST(MediaPoolDirectMapTest, MapFailHooks) {
  ScopedMapHooksReset reset_hooks;
  const size_t page = PageSize();
  MediaPoolDirectMap::SetMapFailForTesting(true);
  size_t size = page;
  EXPECT_EQ(MediaPoolDirectMap::Map(&size), nullptr);
  EXPECT_EQ(size, page);
  MediaPoolDirectMap::SetMapFailForTesting(false);

  const size_t base_blocks = MediaPoolDirectMap::BlockCountForTesting();
  MediaPoolDirectMap::SetMapFailAboveForTesting(2 * page);
  size = 3 * page;
  EXPECT_EQ(MediaPoolDirectMap::Map(&size), nullptr);
  size = 2 * page;
  void* p = MediaPoolDirectMap::Map(&size);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks + 1);
  EXPECT_TRUE(MediaPoolDirectMap::Unmap(p));
  EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);
}

TEST(MediaPoolDirectMapTest, MapFailsWhenTableIsFull) {
  const size_t page = PageSize();
  std::vector<void*> blocks;
  // Unmaps every block even if an ASSERT returns early.
  std::unique_ptr<std::vector<void*>, void (*)(std::vector<void*>*)> unmap_all(
      &blocks, [](std::vector<void*>* v) {
        for (void* p : *v) {
          MediaPoolDirectMap::Unmap(p);
        }
      });
  while (MediaPoolDirectMap::BlockCountForTesting() <
         MediaPoolDirectMap::kMaxBlocks) {
    size_t size = page;
    void* p = MediaPoolDirectMap::Map(&size);
    ASSERT_NE(p, nullptr);
    blocks.push_back(p);
  }

  size_t size = page;
  EXPECT_EQ(MediaPoolDirectMap::Map(&size), nullptr);
  EXPECT_EQ(size, page);
  EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(),
            MediaPoolDirectMap::kMaxBlocks);

  // Freeing one block makes room again.
  ASSERT_TRUE(MediaPoolDirectMap::Unmap(blocks.back()));
  blocks.pop_back();
  void* p = MediaPoolDirectMap::Map(&size);
  ASSERT_NE(p, nullptr);
  blocks.push_back(p);
}

TEST(MediaPoolDirectMapTest, IsEnabledFollowsFeatureOn32BitOnly) {
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(kCobaltMediaPoolSoftFail);
    EXPECT_FALSE(MediaPoolDirectMap::IsEnabled());
  }
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(kCobaltMediaPoolSoftFail);
#if defined(ARCH_CPU_32_BITS)
    EXPECT_TRUE(MediaPoolDirectMap::IsEnabled());
#else
    EXPECT_FALSE(MediaPoolDirectMap::IsEnabled());
#endif
    // A test override wins over the feature on every architecture.
    MediaPoolDirectMap::SetEnabledForTesting(false);
    EXPECT_FALSE(MediaPoolDirectMap::IsEnabled());
    MediaPoolDirectMap::SetEnabledForTesting(true);
    EXPECT_TRUE(MediaPoolDirectMap::IsEnabled());
    MediaPoolDirectMap::SetEnabledForTesting(std::nullopt);
  }
}

}  // namespace
}  // namespace media
