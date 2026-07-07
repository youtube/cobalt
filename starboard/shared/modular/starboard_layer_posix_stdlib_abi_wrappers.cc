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

#include "starboard/shared/modular/starboard_layer_posix_stdlib_abi_wrappers.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Reimplementation of musl's __randname
static void randname(char* tmpl) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  unsigned long r =
      ts.tv_sec + ts.tv_nsec + (unsigned long)((uintptr_t)tmpl) * 65537UL;
  for (int i = 0; i < 6; i++, r >>= 5) {
    tmpl[i] = 'A' + (r & 15) + (r & 16) * 2;
  }
}

// Reimplementation of musl's __mkostemps
int __abi_wrap_mkostemps(char* tmpl, int suffixlen, int flags) {
  size_t l = strlen(tmpl);
  if (suffixlen < 0 || l < 6 || suffixlen > (int)(l - 6) ||
      memcmp(tmpl + l - suffixlen - 6, "XXXXXX", 6)) {
    errno = EINVAL;
    return -1;
  }

  flags -= flags & O_ACCMODE;
  int fd;
  int retries = 100;
  do {
    randname(tmpl + l - suffixlen - 6);
    fd = openat(AT_FDCWD, tmpl, flags | O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) {
      return fd;
    }
  } while (--retries && errno == EEXIST);

  memcpy(tmpl + l - suffixlen - 6, "XXXXXX", 6);
  return -1;
}

int __abi_wrap_mkostemp(char* tmpl, int flags) {
  return __abi_wrap_mkostemps(tmpl, 0, flags);
}
