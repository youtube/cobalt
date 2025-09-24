// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_prctl_abi_wrappers.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

// From third_party/musl/include/sys/prctl.h
#ifndef PR_SET_VMA
#define PR_SET_VMA 0x53564d41
#endif
#ifndef PR_SET_VMA_ANON_NAME
#define PR_SET_VMA_ANON_NAME 0
#endif

namespace {

const char kVmaName[] = "TestVmaName";

// Checks /proc/self/maps to see if the memory mapping containing |addr| has
// the specified |name|. This is the expected outcome when the kernel supports
// PR_SET_VMA_ANON_NAME.
bool VmaIsNamed(void* addr, const char* name) {
  FILE* fp = fopen("/proc/self/maps", "r");
  if (!fp) {
    // If we can't open /proc/self/maps, we can't verify.
    // Assume it's not named and let the fallback check proceed.
    return false;
  }

  char line[1024];
  bool found = false;
  unsigned long target_addr = (unsigned long)addr;

  while (fgets(line, sizeof(line), fp)) {
    unsigned long start, end;
    if (sscanf(line, "%lx-%lx", &start, &end) != 2) {
      continue;
    }
    if (target_addr >= start && target_addr < end) {
      if (strstr(line, name)) {
        found = true;
      }
      // We found the mapping containing our address, so we can stop.
      // If it's not named here, it's not named.
      break;
    }
  }

  fclose(fp);
  return found;
}

// Checks for the existence and content of the fallback file. This is the
// expected outcome when the kernel does NOT support PR_SET_VMA_ANON_NAME.
bool FallbackFileIsCorrect(unsigned long start,
                           unsigned long size,
                           const char* name) {
  char file_path[256];
  snprintf(file_path, sizeof(file_path), "/tmp/cobalt_vma_tags_%d.txt",
           getpid());

  FILE* fp = fopen(file_path, "r");
  if (!fp) {
    return false;
  }

  char line[1024];
  bool found = false;
  while (fgets(line, sizeof(line), fp)) {
    unsigned long file_start, file_end;
    char file_name[256];
    // The format is "0x%lx 0x%lx %s\n"
    if (sscanf(line, "0x%lx 0x%lx %s", &file_start, &file_end, file_name) ==
        3) {
      if (file_start == start && file_end == start + size &&
          strcmp(file_name, name) == 0) {
        found = true;
        break;
      }
    }
  }

  fclose(fp);
  return found;
}

// This test verifies that __abi_wrap_prctl with PR_SET_VMA and
// PR_SET_VMA_ANON_NAME correctly names a VMA region, either by calling the
// underlying prctl syscall or by using the fallback mechanism of writing to a
// file.
TEST(PosixPrctlTest, SetVmaAnonName) {
  const size_t kMapSize = 4096;
  void* p = malloc(kMapSize);
  ASSERT_NE(p, nullptr);

  // Ensure we have a clean state by deleting any leftover fallback file.
  char file_path[256];
  snprintf(file_path, sizeof(file_path), "/tmp/cobalt_vma_tags_%d.txt",
           getpid());
  unlink(file_path);

  int result =
      __abi_wrap_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, (unsigned long)p,
                       kMapSize, (unsigned long)kVmaName);

  // The wrapper should return 0 on success, for both the prctl call and the
  // fallback.
  EXPECT_EQ(result, 0);

  bool vma_is_named = VmaIsNamed(p, kVmaName);
  bool fallback_file_is_correct =
      FallbackFileIsCorrect((unsigned long)p, kMapSize, kVmaName);

  // We expect one of the two mechanisms to have worked.
  EXPECT_TRUE(vma_is_named || fallback_file_is_correct)
      << "VMA name was not set via prctl and fallback file was not created or "
         "is incorrect.";

  if (vma_is_named) {
    SB_LOG(INFO) << "VMA naming with PR_SET_VMA_ANON_NAME is supported by the "
                    "kernel.";
  }
  if (fallback_file_is_correct) {
    SB_LOG(INFO) << "VMA naming with PR_SET_VMA_ANON_NAME is NOT supported by "
                    "the kernel, fallback was used.";
  }

  free(p);
  // Clean up the fallback file if it was created.
  unlink(file_path);
}

}  // namespace
