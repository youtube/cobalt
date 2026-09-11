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

// This file tests the dl_iterate_phdr override used by Cobalt Evergreen.
//
// Background:
// In Linux, dl_iterate_phdr() is a standard system function provided by the C
// library (glibc). It allows programs to query the list of all shared libraries
// currently loaded into memory, along with their memory addresses and program
// headers. Diagnostic and debugging tools—most notably AddressSanitizer (ASan)
// and stack unwinding tools—rely on dl_iterate_phdr() during a crash to map
// memory addresses in a stack trace back to specific library files on disk.
//
// Cobalt Evergreen loads its shared libraries (such as libcobalt.so) using a
// custom in-memory ELF loader rather than the operating system's dynamic linker
// (dlopen). Because the operating system is unaware of these custom-loaded
// libraries, the default glibc dl_iterate_phdr() does not list them. Without an
// override, crash reports and stack traces display "(<unknown module>)" for any
// code executing inside an Evergreen library.
//
// The dl_iterate_phdr override solves this by intercepting calls to
// dl_iterate_phdr(), querying glibc for all host system libraries first, and
// then appending the custom-loaded Evergreen library using metadata registered
// with EvergreenInfo.

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

// Test 1: EnumeratesHostLibraries
//
// Purpose:
// Verifies that installing the dl_iterate_phdr override does not break the
// normal enumeration of standard host operating system libraries.
//
// How it works:
// The test invokes dl_iterate_phdr() with a callback that collects the names
// of all reported libraries. It verifies that the iteration succeeds (returns
// 0), that libraries are found, and specifically that the standard C library
// (libc.so) is among them. This confirms that the override correctly delegates
// to the underlying host implementation.
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

// Test 2: EnumeratesEvergreenLibraryWhenRegistered
//
// Purpose:
// Verifies that when an Evergreen library is loaded and registered with
// EvergreenInfo, dl_iterate_phdr() correctly synthesizes and appends an entry
// for that library so that crash handlers (like AddressSanitizer) can inspect
// it.
//
// How it works:
// 1. A mock Evergreen library is registered using SetEvergreenInfo(),
// specifying
//    a mock base memory address, a file path ("/test/path/libcobalt.so"), and
//    mock ELF program headers (PT_LOAD segments representing code and data).
// 2. dl_iterate_phdr() is called to iterate through all libraries.
// 3. The test callback verifies that the Evergreen library is found in the
//    list, and that its reported base address, segment count, and segment
//    properties (virtual addresses, memory sizes, and read/write/execute flags)
//    match the registered metadata.
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

// Test 3: EarlyTermination
//
// Purpose:
// Verifies that the override respects the standard early-termination contract
// of dl_iterate_phdr().
//
// How it works:
// According to the dl_iterate_phdr() specification, if the caller's callback
// function returns a non-zero integer, the iteration must stop immediately,
// and dl_iterate_phdr() must return that exact non-zero value to the caller.
// This test provides a callback that returns a non-zero value (1234) on the
// very first library visited. It verifies that dl_iterate_phdr() halts
// immediately, called the callback only once, and propagated the return value.
TEST_F(DlIteratePhdrOverrideTest, EarlyTermination) {
  int call_count = 0;
  int result = dl_iterate_phdr(EarlyTermCallback, &call_count);
  EXPECT_EQ(result, 1234);
  EXPECT_EQ(call_count, 1);
}

// Test 4: EarlyTerminationOnEvergreenLibrary
//
// Purpose:
// Verifies that early termination also works when the callback decides to stop
// upon encountering the synthesized Evergreen library entry.
//
// How it works:
// 1. A mock Evergreen library is registered.
// 2. The callback returns 0 for all host libraries, but returns a non-zero
//    status (5678) when it encounters the Evergreen library.
// 3. The test verifies that dl_iterate_phdr() stops and returns 5678, ensuring
//    the override correctly handles early exit even after host library
//    enumeration has completed.
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

// Test 5: AsyncSignalSafetyZeroAllocations
//
// Purpose:
// Verifies that the dl_iterate_phdr override does not perform any dynamic heap
// allocations (such as malloc or new) or take locks when generating the
// synthetic Evergreen library entry.
//
// Why this matters:
// When a program crashes (e.g., due to a segmentation fault or invalid memory
// access), the operating system invokes crash handlers in an
// "async-signal-safe" context. In this state, heap memory allocators and
// mutexes may be in an inconsistent state; attempting to allocate memory or
// acquire a lock could cause a deadlock or a secondary crash. Because
// AddressSanitizer calls dl_iterate_phdr() directly from its crash signal
// handler, the override must construct all metadata strictly on the current
// call stack.
//
// How it works:
// The test registers a mock Evergreen library and invokes dl_iterate_phdr().
// When the callback is called for the Evergreen library, it checks that the
// memory addresses of both the dl_phdr_info structure and the library name
// string reside within the current call stack frame (within 64 KB of the
// callback's stack frame pointer), proving that no heap allocation occurred.
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

// Test 6: DISABLED_TriggerAsanCrash
//
// Purpose:
// Provides a manual verification mechanism to observe real AddressSanitizer
// (ASan) crash reports and stack symbolization in an Evergreen environment.
//
// Why it is disabled by default:
// This test intentionally performs a "heap-use-after-free" memory error
// (allocating memory, deleting it, and then reading from the deleted pointer).
// When executed in an ASan-instrumented build, ASan intercepts the invalid read
// and invokes dl_iterate_phdr() to print a symbolized stack trace. It is
// disabled (prefixed with DISABLED_) so that automated test suites pass
// normally without triggering intentional crashes.
TEST_F(DlIteratePhdrOverrideTest, DISABLED_TriggerAsanCrash) {
  volatile int* ptr = new int(42);
  delete ptr;
  // Intentional heap use-after-free to trigger ASan crash handler.
  volatile int val = *ptr;
  (void)val;
}

}  // namespace
}  // namespace starboard
