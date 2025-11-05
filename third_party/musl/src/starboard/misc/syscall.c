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

#if defined(ENABLE_DEBUGGER)
#include "starboard/log.h"
#endif // defined(ENABLE_DEBUGGER)

#include "starboard/configuration.h"
#include "syscall_arch.h"

#define STARBOARD_MUSL_SYSCALL_IMPL

// Relative to  -isystem../../third_party/musl/arch/generic
#if SB_IS(ARCH_X64) || defined(_M_X64) || defined(__x86_64__)
#include <../x86_64/bits/syscall.h.in>
#elif SB_IS(ARCH_ARM64) || defined(__AARCH64EL__) || defined(_M_ARM64)
#include <../aarch64/bits/syscall.h.in>
#elif SB_IS(ARCH_X86) || defined(_M_IX86) || defined(__i386__)
#include <../i386/bits/syscall.h.in>
#elif SB_IS(ARCH_ARM) || defined(__ARMEL__)
#include <../arm/bits/syscall.h.in>
#endif

#ifndef __NR_gettid
#error For syscall to be implementable, we need numbers.
#endif

// Although the syscall function calls into the kernel, it is a libc provided
// function, and therefore it uses libc return conventions (errors return with
// -1 with errno set).

// The syscall macro from internal/syscall.h needs to be undefined so that
// the syscall function can be defined.
#undef syscall
long syscall(long number, ...) {
  va_list ap;
  va_start(ap, number);

#if defined(ENABLE_DEBUGGER)
  printf("Unexpected syscall %d (0x%x)\n", number, number);
  SbLogRawDumpStack(0);
#endif  // defined(ENABLE_DEBUGGE

  switch (number) {
#ifdef __NR_close
    case __NR_close: {
      int fildes = va_arg(ap, int);
      va_end(ap);
      return libc_wrapper_SYS_close(fildes);
    }
#endif
#ifdef __NR_fcntl
    case __NR_fcntl: {
      int fd = va_arg(ap, int);
      int op = va_arg(ap, int);
      void* arg = va_arg(ap, void*);
      va_end(ap);
      return libc_wrapper_SYS_fcntl(fd, op, arg);
    }
#endif
#ifdef __NR_gettid
    case __NR_gettid:
      va_end(ap);
      return libc_wrapper_SYS_gettid();
#endif
#ifdef __NR_lseek
    case __NR_lseek: {
      int fildes = va_arg(ap, int);
      off_t offset = va_arg(ap, off_t);
      int whence = va_arg(ap, int);
      va_end(ap);
      return libc_wrapper_SYS_lseek(fildes, offset, whence);
    }
#endif
#ifdef __NR_lstat
    case __NR_lstat: {
      const char *pathname = va_arg(ap, const char *);
      struct stat * statbuf = va_arg(ap, struct stat *);
      va_end(ap);
      return libc_wrapper_SYS_lstat(pathname, statbuf);
    }
#endif
#ifdef __NR_open
    case __NR_open: {
      const char* pathname = va_arg(ap, const char*);
      int flags = va_arg(ap, int);
      mode_t mode = va_arg(ap, mode_t);
      va_end(ap);
      return libc_wrapper_SYS_open(pathname, flags, mode);
    }
#endif
#ifdef __NR_read
    case __NR_read: {
      int fildes = va_arg(ap, int);
      void* buf = va_arg(ap, void*);
      size_t nbyte = va_arg(ap, size_t);
      va_end(ap);
      return libc_wrapper_SYS_read(fildes, buf, nbyte);
    }
#endif
#ifdef __NR_readv
    case __NR_readv: {
      int fildes = va_arg(ap, int);
      const struct iovec* iov = va_arg(ap, const struct iovec*);
      int iovcnt = va_arg(ap, int);
      va_end(ap);
      return libc_wrapper_SYS_readv(fildes, iov, iovcnt);
    }
#endif
#ifdef __NR_rmdir
    case __NR_rmdir: {
      const char* pathname = va_arg(ap, const char*);
      va_end(ap);
      return libc_wrapper_SYS_rmdir(pathname);
    }
#endif
#ifdef __NR_stat
    case __NR_stat: {
      const char *pathname = va_arg(ap, const char *);
      struct stat * statbuf = va_arg(ap, struct stat *);
      va_end(ap);
      return libc_wrapper_SYS_stat(pathname, statbuf);
    }
#endif
#ifdef __NR_unlink
    case __NR_unlink: {
      const char* pathname = va_arg(ap, const char*);
      va_end(ap);
      return libc_wrapper_SYS_unlink(pathname);
    }
#endif
#ifdef __NR_write
    case __NR_write: {
      int fildes = va_arg(ap, int);
      const void* buf = va_arg(ap, const void*);
      size_t nbyte = va_arg(ap, size_t);
      va_end(ap);
      return libc_wrapper_SYS_write(fildes, buf, nbyte);
    }
#endif
#ifdef __NR_writev
    case __NR_writev: {
      int fildes = va_arg(ap, int);
      const struct iovec* iov = va_arg(ap, const struct iovec*);
      int iovcnt = va_arg(ap, int);
      va_end(ap);
      return libc_wrapper_SYS_writev(fildes, iov, iovcnt);
    }
#endif
    default:
      va_end(ap);
      errno = ENOSYS;
      return -1;
  }
}
