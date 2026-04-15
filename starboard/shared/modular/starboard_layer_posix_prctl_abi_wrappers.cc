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
#include <fcntl.h>
#include <linux/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <algorithm>
#include <mutex>

#include "starboard/common/log.h"

// From third_party/musl/include/sys/prctl.h
#ifndef PR_SET_VMA
#define PR_SET_VMA 0x53564d41
#endif
#ifndef PR_SET_VMA_ANON_NAME
#define PR_SET_VMA_ANON_NAME 0
#endif

namespace {

const char kVmaTagsFileNamePrefix[] = "cobalt_vma_tags";

std::mutex g_vma_tags_mutex;
char g_vma_tags_file_path[256] = {0};

void VmaTagFileCleanup() {
  std::lock_guard<std::mutex> lock(g_vma_tags_mutex);
  if (g_vma_tags_file_path[0] != '\0') {
    unlink(g_vma_tags_file_path);
    g_vma_tags_file_path[0] = '\0';
  }
}

const char* GetTempDir() {
  const char* tmpdir = getenv("TMPDIR");
  if (!tmpdir) {
    tmpdir = "/tmp";
  }
  return tmpdir;
}

}  // namespace

SB_EXPORT int __abi_wrap_prctl(int option,
                               unsigned long arg2,
                               unsigned long arg3,
                               unsigned long arg4,
                               unsigned long arg5) {
  int result = prctl(option, arg2, arg3, arg4, arg5);

  if (result == -1 && option == PR_SET_VMA && arg2 == PR_SET_VMA_ANON_NAME) {
    int saved_errno = errno;
    std::lock_guard<std::mutex> lock(g_vma_tags_mutex);
    int fd = -1;

    // Try to open existing fallback file.
    if (g_vma_tags_file_path[0] != '\0') {
      fd = open(g_vma_tags_file_path, O_WRONLY | O_APPEND | O_CLOEXEC);
      if (fd == -1) {
        g_vma_tags_file_path[0] = '\0';
      }
    }

    // Create new fallback file if needed.
    if (fd == -1 && g_vma_tags_file_path[0] == '\0') {
      char path_template[256];
      snprintf(path_template, sizeof(path_template), "%s/%s_XXXXXX",
               GetTempDir(), kVmaTagsFileNamePrefix);
      fd = mkstemp(path_template);
      if (fd != -1) {
        snprintf(g_vma_tags_file_path, sizeof(g_vma_tags_file_path), "%s",
                 path_template);
        static bool cleanup_registered = false;
        if (!cleanup_registered) {
          atexit(VmaTagFileCleanup);
          cleanup_registered = true;
        }
      }
    }

    if (fd != -1) {
      char buf[256];
      int len = snprintf(buf, sizeof(buf), "0x%lx 0x%lx %s\n", arg3,
                         arg3 + arg4, (const char*)arg5);
      if (len > 0) {
        size_t write_len = std::min((size_t)len, sizeof(buf) - 1);
        if (write(fd, buf, write_len) != -1) {
          close(fd);
          errno = saved_errno;
          return 0;
        }
      }
      close(fd);
    }
    errno = saved_errno;
  }

  return result;
}
