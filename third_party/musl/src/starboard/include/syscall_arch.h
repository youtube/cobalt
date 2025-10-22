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

#include "starboard/thread.h"

// Map syscall to `libc_wrapper_SYS_foo`.
//
// On Starboard, we have separate implementations for each function callable
// with syscall. As a result, we can expand the syscall argument in a macro and
// avoid having to do runtime disambiguation.
//
// Unimplemented syscall functions can be detected from their undefined
// symbol references for names starting with the prefix "libc_wrapper_".
//
// Note: The  libc_wrapper_() functions available in Starboard are declared in
// starboard/include/syscall_arch.h

#define SYSCALL_PASTE_HELPER(a, b) a##b
#define SYSCALL_PASTE(a, b) SYSCALL_PASTE_HELPER(a, b)
#define syscall(name, ...) SYSCALL_PASTE(libc_wrapper_, name)(__VA_ARGS__)
//#define syscall(name, ...) libc_wrapper_##name(__VA_ARGS__)

// Map `libc_wrapper_SYS_ioctl(int fd, op,...)` to `ioctl_op(int fd,...)` calls
// to allow separate implementation per ioctl operation.
#define __LIBC_WRAPPER_SYS_IOCTL_CONCAT_X(a,b) a##b
#define __LIBC_WRAPPER_SYS_IOCTL_CONCAT(a,b) __LIBC_WRAPPER_SYS_IOCTL_CONCAT_X(a,b)
#define __LIBC_WRAPPER_SYS_IOCTL_DISP(b, op,...) __LIBC_WRAPPER_SYS_IOCTL_CONCAT(b,op)(__VA_ARGS__)
#define libc_wrapper_SYS_ioctl(fd, op,...) __LIBC_WRAPPER_SYS_IOCTL_DISP(ioctl_, op, fd, __VA_ARGS__)

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

// Define wrappers for unsupported syscalls that it called by code that can
// handle unsupported syscall functions.
#define libc_wrapper_SYS_memfd_create(name, flags) (errno = ENOSYS, -1)


static inline int libc_wrapper_SYS_fcntl(int fd, int op, ... /* arg */ ) {
  va_list ap;
  va_start(ap, op);
  void* arg = va_arg(ap, void*);
  va_end(ap);
  return fcntl(fd, op, arg);
}

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_SYSCALL_ARCH_H_
