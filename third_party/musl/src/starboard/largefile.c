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

#if defined(_LARGEFILE64_SOURCE)
#include <fcntl.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>

int stat64(const char* path, struct stat* info) {
  return stat(path, info);
}

int fstat64(int fildes, struct stat* info) {
  return fstat(fildes, info);
}

int open64(const char *filename, int flags, ...)
{
  mode_t mode = 0;

  if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  return open(filename, flags, mode);
}

void* mmap64(void* addr, size_t len, int prot, int flags, int fildes, off_t off) {
  return mmap(addr, len, prot, flags, fildes, off);
}

off_t lseek64(int fildes, off_t offset, int whence) {
  return lseek(fildes, offset, whence);
}
#endif
