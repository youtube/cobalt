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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

// From third_party/musl/include/sys/prctl.h
#ifndef PR_SET_VMA
#define PR_SET_VMA 0x53564d41
#endif
#ifndef PR_SET_VMA_ANON_NAME
#define PR_SET_VMA_ANON_NAME 0
#endif

namespace {
void VmaTagFileCleanup() {
  char file_path[256];
  snprintf(file_path, sizeof(file_path), "/tmp/cobalt_vma_tags_%d.txt",
           getpid());
  unlink(file_path);
}

void VmaTagSignalHandler(int signum) {
  VmaTagFileCleanup();
  // Re-raise signal to get default behavior (e.g. core dump).
  signal(signum, SIG_DFL);
  raise(signum);
}
}  // namespace

SB_EXPORT int __abi_wrap_prctl(int option,
                               unsigned long arg2,
                               unsigned long arg3,
                               unsigned long arg4,
                               unsigned long arg5) {
  int result = prctl(option, arg2, arg3, arg4, arg5);
  if (result == -1 && errno == EINVAL && option == PR_SET_VMA &&
      arg2 == PR_SET_VMA_ANON_NAME) {
    // Kernel does not support PR_SET_VMA_ANON_NAME. Fallback to writing to a
    // file.
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "/tmp/cobalt_vma_tags_%d.txt",
             getpid());

    FILE* file = fopen(file_path, "a");
    if (file) {
      static bool cleanup_registered = false;
      if (!cleanup_registered) {
        atexit(VmaTagFileCleanup);
        // Also register signal handlers for common crash signals.
        // This is not a perfect solution as it can interfere with application
        // signal handlers and does not handle SIGKILL.
        signal(SIGSEGV, VmaTagSignalHandler);
        signal(SIGABRT, VmaTagSignalHandler);
        signal(SIGTERM, VmaTagSignalHandler);
        signal(SIGQUIT, VmaTagSignalHandler);
        signal(SIGINT, VmaTagSignalHandler);
        cleanup_registered = true;
      }
      fprintf(file, "0x%lx 0x%lx %s\n", arg3, arg3 + arg4, (const char*)arg5);
      fclose(file);
      return 0;  // Success for our fallback.
    } else {
      SB_LOG(ERROR) << "Failed to open VMA tag file: " << file_path;
      // Fall through to return original error.
    }
  }
  return result;
}
