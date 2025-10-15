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

// Signal that we support these sycalls. This is only necessary for syscalls
// where musl code uses non-existence of these defines to use fallbacks instead.
#define SYS_lstat
#define SYS_open
#define SYS_unlink
#define SYS_rmdir
#define SYS_stat

// syscall() calls are split by syscall 'number' parameter.
// For Starboard builds, the syscall names are not mapped to numbers, allowing
// these wrappers to use the syscall name in the function name.
// See src/internal/syscall.h for more details.

// Simple wrappers can be directly replaced with the function name.
#define libc_wrapper_SYS_close(fildes) close(fildes)
#define libc_wrapper_SYS_gettid() SbThreadGetId()
#define libc_wrapper_SYS_lseek(fildes, offset, whence) lseek(fildes, offset, whence)
#define libc_wrapper_SYS_lstat(pathname, statbuf) lstat(pathname, statbuf)
#define libc_wrapper_SYS_open(pathname, flags, mode) open(pathname, flags, mode)
#define libc_wrapper_SYS_read(fildes, buf, nbyte) read(fildes, buf, nbyte)
#define libc_wrapper_SYS_readv(fildes, iov, iovcnt) readv(fildes, iov, iovcnt)
#define libc_wrapper_SYS_rmdir(pathname) rmdir(pathname)
#define libc_wrapper_SYS_stat(pathname, statbuf) stat(pathname, statbuf)
#define libc_wrapper_SYS_unlink(pathname) unlink(pathname)
#define libc_wrapper_SYS_write(fildes, buf, nbyte) write(fildes, buf, nbyte)
#define libc_wrapper_SYS_writev(fildes, iov, iovcnt) writev(fildes, iov, iovcnt)

static inline int libc_wrapper_SYS_fcntl(int fd, int op, ... /* arg */ ) {
  va_list ap;
  va_start(ap, op);
  void* arg = va_arg(ap, void*);
  va_end(ap);
  return fcntl(fd, op, arg);
}

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_SYSCALL_ARCH_H_
