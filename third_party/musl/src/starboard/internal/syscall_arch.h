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

#ifndef THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_SYSCALL_ARCH_H_
#define THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_SYSCALL_ARCH_H_

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>

#include "pthread_impl.h"
#include "starboard/thread.h"

static inline pid_t libc_wrapper_SYS_gettid() {
  return SbThreadGetId();
}

static inline int libc_wrapper_SYS_close(int fildes) {
  return close(fildes);
}

static inline ssize_t libc_wrapper_SYS_read(int fildes,
                                            void* buf,
                                            size_t nbyte) {
  return read(fildes, buf, nbyte);
}

static inline ssize_t libc_wrapper_SYS_readv(int fildes,
                                             const struct iovec* iov,
                                             int iovcnt) {
  return readv(fildes, iov, iovcnt);
}

static inline ssize_t libc_wrapper_SYS_write(int fildes,
                                             const void* buf,
                                             size_t nbyte) {
  return write(fildes, buf, nbyte);
}

static inline ssize_t libc_wrapper_SYS_writev(int fildes,
  const struct iovec* iov,
  int iovcnt) {
  return writev(fildes, iov, iovcnt);
}

static inline off_t libc_wrapper_SYS_lseek(int fildes,
                                           off_t offset,
                                           int whence) {
  return lseek(fildes, offset, whence);
}

static inline int libc_wrapper_SYS_open(const char *pathname, int flags, mode_t mode) {
  return open(pathname, flags, mode);
}

static inline int libc_wrapper_SYS_unlinkat(int dirfd, const char *pathname, int flags) {
  return unlinkat(dirfd, pathname, flags);
}

static inline int libc_wrapper_SYS_fcntl(int fd, int op, ... /* arg */ ) {
  va_list ap;
  va_start(ap, op);
  void* arg = va_arg(ap, void*);
  va_end(ap);
  return fcntl(fd, op, arg);
}

static inline int libc_wrapper_SYS_ioctl(int fd, int op,...) {
  va_list ap;
  va_start(ap, op);
  void* arg = va_arg(ap, void*);
  va_end(ap);
  return ioctl(fd, op, arg);
}

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_SYSCALL_ARCH_H_
