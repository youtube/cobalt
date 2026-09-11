// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/elf_loader/dl_iterate_phdr_override.h"

#include <link.h>
#include <string.h>

#include <string>
#include <vector>

#include "starboard/common/string.h"
#include "starboard/elf_loader/evergreen_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

#if UINTPTR_MAX > 0xffffffffU
constexpr uintptr_t kMockBase = 0x7f1234000000ULL;
#else
constexpr uintptr_t kMockBase = 0x70000000U;
#endif

struct HostLibsContext {
  std::vector<std::string> names;
  bool found_libc = false;
};

int HostLibsCallback(struct dl_phdr_info* info, size_t size, void* data) {
  auto* ctx = static_cast<HostLibsContext*>(data);
  if (info->dlpi_name) {
    ctx->names.push_back(info->dlpi_name);
    if (strstr(info->dlpi_name, "libc.so") != nullptr) {
      ctx->found_libc = true;
    }
  }
  return 0;
}

class DlIteratePhdrOverrideTest : public ::testing::Test {
 protected:
  void SetUp() override { InitDlIteratePhdrOverride(); }
};

TEST_F(DlIteratePhdrOverrideTest, EnumeratesHostLibraries) {
  HostLibsContext ctx;
  int result = dl_iterate_phdr(HostLibsCallback, &ctx);
  EXPECT_EQ(result, 0);
  EXPECT_FALSE(ctx.names.empty());
  EXPECT_TRUE(ctx.found_libc);
}

struct EvergreenTestContext {
  bool found = false;
  uint64_t base_address = 0;
  uint16_t phnum = 0;
  std::vector<ElfW(Phdr)> phdrs;
};

int EvergreenCallback(struct dl_phdr_info* info, size_t size, void* data) {
  auto* ctx = static_cast<EvergreenTestContext*>(data);
  if (info->dlpi_name &&
      strcmp(info->dlpi_name, "/test/path/libcobalt.so") == 0) {
    ctx->found = true;
    ctx->base_address = info->dlpi_addr;
    ctx->phnum = info->dlpi_phnum;
    for (size_t i = 0; i < info->dlpi_phnum; ++i) {
      ctx->phdrs.push_back(info->dlpi_phdr[i]);
    }
  }
  return 0;
}

TEST_F(DlIteratePhdrOverrideTest, EnumeratesEvergreenLibraryWhenRegistered) {
  SetEvergreenInfo(nullptr);

  constexpr size_t kMockLoadSize = 0x200000;
  const char* kMockPath = "/test/path/libcobalt.so";

  ElfW(Phdr) mock_phdrs[2];
  memset(mock_phdrs, 0, sizeof(mock_phdrs));
  mock_phdrs[0].p_type = PT_LOAD;
  mock_phdrs[0].p_vaddr = 0x1000;
  mock_phdrs[0].p_memsz = 0x5000;
  mock_phdrs[0].p_flags = PF_R | PF_X;

  mock_phdrs[1].p_type = PT_LOAD;
  mock_phdrs[1].p_vaddr = 0x7000;
  mock_phdrs[1].p_memsz = 0x3000;
  mock_phdrs[1].p_flags = PF_R | PF_W;

  EvergreenInfo info;
  memset(&info, 0, sizeof(info));
  starboard::strlcpy(info.file_path_buf, kMockPath, sizeof(info.file_path_buf));
  info.base_address = kMockBase;
  info.load_size = kMockLoadSize;
  info.phdr_table = reinterpret_cast<uint64_t>(mock_phdrs);
  info.phdr_table_num = 2;

  EXPECT_TRUE(SetEvergreenInfo(&info));

  EvergreenTestContext ctx;
  int result = dl_iterate_phdr(EvergreenCallback, &ctx);
  EXPECT_EQ(result, 0);
  EXPECT_TRUE(ctx.found);
  EXPECT_EQ(ctx.base_address, kMockBase);
  EXPECT_EQ(ctx.phnum, 2u);
  ASSERT_EQ(ctx.phdrs.size(), 2u);
  EXPECT_EQ(ctx.phdrs[0].p_type, static_cast<ElfW(Word)>(PT_LOAD));
  EXPECT_EQ(ctx.phdrs[0].p_vaddr, static_cast<ElfW(Addr)>(0x1000));
  EXPECT_EQ(ctx.phdrs[0].p_memsz, static_cast<ElfW(Xword)>(0x5000));
  EXPECT_EQ(ctx.phdrs[1].p_type, static_cast<ElfW(Word)>(PT_LOAD));
  EXPECT_EQ(ctx.phdrs[1].p_vaddr, static_cast<ElfW(Addr)>(0x7000));
  EXPECT_EQ(ctx.phdrs[1].p_memsz, static_cast<ElfW(Xword)>(0x3000));

  SetEvergreenInfo(nullptr);
}

int EarlyTermCallback(struct dl_phdr_info* info, size_t size, void* data) {
  int* count = static_cast<int*>(data);
  (*count)++;
  return 1234;
}

TEST_F(DlIteratePhdrOverrideTest, EarlyTermination) {
  int call_count = 0;
  int result = dl_iterate_phdr(EarlyTermCallback, &call_count);
  EXPECT_EQ(result, 1234);
  EXPECT_EQ(call_count, 1);
}

TEST_F(DlIteratePhdrOverrideTest, EarlyTerminationOnEvergreenLibrary) {
  const char* kMockPath = "/test/path/libcobalt.so";
  ElfW(Phdr) mock_phdr;
  memset(&mock_phdr, 0, sizeof(mock_phdr));
  mock_phdr.p_type = PT_LOAD;

  EvergreenInfo info;
  memset(&info, 0, sizeof(info));
  starboard::strlcpy(info.file_path_buf, kMockPath, sizeof(info.file_path_buf));
  info.base_address = kMockBase;
  info.load_size = 0x10000;
  info.phdr_table = reinterpret_cast<uint64_t>(&mock_phdr);
  info.phdr_table_num = 1;

  EXPECT_TRUE(SetEvergreenInfo(&info));

  struct TermContext {
    int calls = 0;
    bool saw_evergreen = false;
  };

  auto cb = [](struct dl_phdr_info* info, size_t size, void* data) -> int {
    auto* ctx = static_cast<TermContext*>(data);
    ctx->calls++;
    if (info->dlpi_name &&
        strcmp(info->dlpi_name, "/test/path/libcobalt.so") == 0) {
      ctx->saw_evergreen = true;
      return 5678;
    }
    return 0;
  };

  TermContext ctx;
  int result = dl_iterate_phdr(cb, &ctx);
  EXPECT_EQ(result, 5678);
  EXPECT_TRUE(ctx.saw_evergreen);

  SetEvergreenInfo(nullptr);
}

TEST_F(DlIteratePhdrOverrideTest, AsyncSignalSafetyZeroAllocations) {
  const char* kMockPath = "/test/path/libcobalt.so";
  ElfW(Phdr) mock_phdr;
  memset(&mock_phdr, 0, sizeof(mock_phdr));
  mock_phdr.p_type = PT_LOAD;

  EvergreenInfo info;
  memset(&info, 0, sizeof(info));
  starboard::strlcpy(info.file_path_buf, kMockPath, sizeof(info.file_path_buf));
  info.base_address = kMockBase;
  info.load_size = 0x10000;
  info.phdr_table = reinterpret_cast<uint64_t>(&mock_phdr);
  info.phdr_table_num = 1;

  EXPECT_TRUE(SetEvergreenInfo(&info));

  struct StackCheckContext {
    bool checked = false;
    bool name_is_stack = false;
    bool phdr_is_expected = false;
    const ElfW(Phdr) * expected_phdr = nullptr;
  };

  StackCheckContext ctx;
  ctx.expected_phdr = &mock_phdr;

  auto cb = [](struct dl_phdr_info* info, size_t size, void* data)
                SB_NO_SANITIZE_ADDRESS -> int {
    auto* c = static_cast<StackCheckContext*>(data);
    if (info->dlpi_name &&
        strcmp(info->dlpi_name, "/test/path/libcobalt.so") == 0) {
      c->checked = true;
      // Verify that dlpi_name and info itself reside on the call stack
      // (within 64KB of the callback's stack frame), proving zero heap
      // allocations.
      uintptr_t stack_ref = reinterpret_cast<uintptr_t>(&size);
      uintptr_t name_addr = reinterpret_cast<uintptr_t>(info->dlpi_name);
      uintptr_t info_addr = reinterpret_cast<uintptr_t>(info);
      ptrdiff_t name_diff = name_addr > stack_ref ? (name_addr - stack_ref)
                                                  : (stack_ref - name_addr);
      ptrdiff_t info_diff = info_addr > stack_ref ? (info_addr - stack_ref)
                                                  : (stack_ref - info_addr);
      c->name_is_stack = (name_diff < 65536) && (info_diff < 65536);
      c->phdr_is_expected = (info->dlpi_phdr == c->expected_phdr);
    }
    return 0;
  };

  int result = dl_iterate_phdr(cb, &ctx);
  EXPECT_EQ(result, 0);
  EXPECT_TRUE(ctx.checked);
  EXPECT_TRUE(ctx.name_is_stack);
  EXPECT_TRUE(ctx.phdr_is_expected);

  SetEvergreenInfo(nullptr);
}

// Test that intentionally triggers a heap-use-after-free for Phase 4
// verification of ASan stack traces and symbolization on Evergreen. Disabled by
// default so normal test runs pass.
TEST_F(DlIteratePhdrOverrideTest, DISABLED_TriggerAsanCrash) {
  volatile int* ptr = new int(42);
  delete ptr;
  // Intentional heap use-after-free to trigger ASan crash handler.
  volatile int val = *ptr;
  (void)val;
}

}  // namespace
}  // namespace starboard
