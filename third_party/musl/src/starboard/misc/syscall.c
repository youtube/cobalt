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
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "syscall_arch.h"

// Although the syscall function calls into the kernel, it is a libc provided
// function, and therefore it uses libc return conventions (errors return with
// -1 with errno set).
long syscall(long number, ...) {
  va_list ap;
  va_start(ap, number);

  switch (number) {
    case __NR_gettid:
      va_end(ap);
      return libc_wrapper_SYS_gettid();
    case __NR_close: {
      int fildes = va_arg(ap, int);
      va_end(ap);
      return libc_wrapper_SYS_close(fildes);
    }
    case __NR_read: {
      int fildes = va_arg(ap, int);
      void* buf = va_arg(ap, void*);
      size_t nbyte = va_arg(ap, size_t);
      va_end(ap);
      return libc_wrapper_SYS_read(fildes, buf, nbyte);
    }
    case __NR_readv: {
      int fildes = va_arg(ap, int);
      const struct iovec* iov = va_arg(ap, const struct iovec*);
      int iovcnt = va_arg(ap, int);
      va_end(ap);
      return libc_wrapper_SYS_readv(fildes, iov, iovcnt);
    }
    case __NR_write: {
      int fildes = va_arg(ap, int);
      const void* buf = va_arg(ap, const void*);
      size_t nbyte = va_arg(ap, size_t);
      va_end(ap);
      return libc_wrapper_SYS_write(fildes, buf, nbyte);
    }
    case __NR_writev: {
      int fildes = va_arg(ap, int);
      const struct iovec* iov = va_arg(ap, const struct iovec*);
      int iovcnt = va_arg(ap, int);
      va_end(ap);
      return libc_wrapper_SYS_writev(fildes, iov, iovcnt);
    }
    case __NR_lseek: {
      int fildes = va_arg(ap, int);
      off_t offset = va_arg(ap, off_t);
      int whence = va_arg(ap, int);
      va_end(ap);
      return libc_wrapper_SYS_lseek(fildes, offset, whence);
    }
    case __NR_open: {
      const char* pathname = va_arg(ap, const char*);
      int flags = va_arg(ap, int);
      mode_t mode = va_arg(ap, mode_t);
      va_end(ap);
      return libc_wrapper_SYS_open(pathname, flags, mode);
    }
    case __NR_unlink: {
      const char* pathname = va_arg(ap, const char*);
      va_end(ap);
      return libc_wrapper_SYS_unlink(pathname);
    }
    case __NR_fcntl: {
      int fd = va_arg(ap, int);
      int op = va_arg(ap, int);
      void* arg = va_arg(ap, void*);
      va_end(ap);
      return libc_wrapper_SYS_fcntl(fd, op, arg);
    }
    default:
      va_end(ap);
      errno = ENOSYS;
      return -1;
  }
}
