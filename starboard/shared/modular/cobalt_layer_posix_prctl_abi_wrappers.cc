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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <algorithm>

#include "starboard/shared/modular/starboard_layer_posix_prctl_abi_wrappers.h"

namespace {

const char kVmaTagsFileNamePrefix[] = "cobalt_vma_tags";

char g_vma_tags_file_path[256] = {0};

void VmaTagFileCleanup() {
  if (g_vma_tags_file_path[0] != '\0') {
    unlink(g_vma_tags_file_path);
  }
}

void VmaTagSignalHandler(int signum) {
  VmaTagFileCleanup();
  // Re-raise signal to get default behavior (e.g. core dump).
  signal(signum, SIG_DFL);
  raise(signum);
}

const char* GetTempDir() {
  const char* tmpdir = getenv("TMPDIR");
  if (!tmpdir) {
    tmpdir = "/tmp";
  }
  return tmpdir;
}

}  // namespace

extern "C" {

int __abi_wrap_prctl(int option,
                     unsigned long arg2,
                     unsigned long arg3,
                     unsigned long arg4,
                     unsigned long arg5);

int prctl(int option, ...) {
  va_list args;
  va_start(args, option);
  unsigned long arg2 = va_arg(args, unsigned long);
  unsigned long arg3 = va_arg(args, unsigned long);
  unsigned long arg4 = va_arg(args, unsigned long);
  unsigned long arg5 = va_arg(args, unsigned long);
  va_end(args);

  int result = __abi_wrap_prctl(option, arg2, arg3, arg4, arg5);

  if (result == -1 && errno == EINVAL && option == MUSL_PR_SET_VMA &&
      arg2 == MUSL_PR_SET_VMA_ANON_NAME) {
    if (g_vma_tags_file_path[0] == '\0') {
      char path_template[256];
      snprintf(path_template, sizeof(path_template), "%s/%s_XXXXXX.txt",
               GetTempDir(), kVmaTagsFileNamePrefix);

      // mkstemps creates the file with 0600 permissions.
      int fd = mkstemps(path_template, 4);
      if (fd != -1) {
        snprintf(g_vma_tags_file_path, sizeof(g_vma_tags_file_path), "%s",
                 path_template);
        close(fd);

        static bool cleanup_registered = false;
        if (!cleanup_registered) {
          atexit(VmaTagFileCleanup);
          // Also register signal handlers for common crash signals.
          signal(SIGSEGV, VmaTagSignalHandler);
          signal(SIGABRT, VmaTagSignalHandler);
          signal(SIGTERM, VmaTagSignalHandler);
          signal(SIGQUIT, VmaTagSignalHandler);
          signal(SIGINT, VmaTagSignalHandler);
          cleanup_registered = true;
        }
      }
    }

    if (g_vma_tags_file_path[0] != '\0') {
      int fd = open(g_vma_tags_file_path, O_WRONLY | O_APPEND);
      if (fd != -1) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "0x%lx 0x%lx %s\n", arg3,
                           arg3 + arg4, (const char*)arg5);
        if (len > 0) {
          size_t write_len =
              std::min(static_cast<size_t>(len), sizeof(buf) - 1);
          write(fd, buf, write_len);
        }
        close(fd);
        return 0;  // Success for our fallback.
      }
    }
  }

  return result;
}

}  // extern "C"
