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
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>


// TODO: b/457772688 remove when using starboard includes from Angle is fixed.
#define COBALT_HACK_FOR_ANGLE_STARBOARD_INCLUDE

#if defined(COBALT_HACK_FOR_ANGLE_STARBOARD_INCLUDE)
// Since Angle includes unistd.h for syscall which is implemented here,
// and Angle does not have the top level include path, we can not include
// starboard/thread.h here. Until that is resolved, declare SbThreadGetId() to
// match its declaraiton in starboard/thread.h.
typedef int32_t SbThreadId;
SbThreadId SbThreadGetId();
#else
#include "starboard/thread.h"
#endif

// Map |syscall()| to |libc_wrapper_SYS_foo()|.
//
// On Starboard, we have individual implementations for each function callable
// with syscall. As a result, we can expand the syscall argument in a macro and
// avoid having to do runtime disambiguation.
//
// Unimplemented syscall functions can be detected from the resulting undefined
// symbol references for names starting with the prefix "libc_wrapper_".

// Use a macro to expand syscall(SYS_foo) calls to libc_wrapper_SYS_foo() calls.
#define __SYSCALL_CONCAT_X(a, b) a##b
#define __SYSCALL_CONCAT(a, b) __SYSCALL_CONCAT_X(a, b)
#define syscall(name, ...) __SYSCALL_CONCAT(libc_wrapper_, name)(__VA_ARGS__)

// Map `libc_wrapper_SYS_ioctl(int fd, op,...)` to `ioctl_op(int fd,...)` calls
// to allow separate implementation per ioctl operation.
#define __LIBC_WRAPPER_SYS_IOCTL_CONCAT_X(a,b) a##b
#define __LIBC_WRAPPER_SYS_IOCTL_CONCAT(a,b) __LIBC_WRAPPER_SYS_IOCTL_CONCAT_X(a,b)
#define __LIBC_WRAPPER_SYS_IOCTL_DISP(b, op, fd_param, ...) __LIBC_WRAPPER_SYS_IOCTL_CONCAT(b,op)(fd_param, ##__VA_ARGS__)
#define libc_wrapper_SYS_ioctl(fd, op, ...) __LIBC_WRAPPER_SYS_IOCTL_DISP(ioctl_, op, fd, ##__VA_ARGS__)

// Signal that we support these sycalls. This for code that checks for existence
// of a definition to determine whether to use fallbacks.
#define SYS_close SYS_close
#define SYS_fcntl SYS_fcntl
#define SYS_gettid SYS_gettid
#define SYS_lseek SYS_lseek
#define SYS_lstat SYS_lstat
#define SYS_open SYS_open
#define SYS_read SYS_read
#define SYS_readv SYS_readv
#define SYS_rmdir SYS_rmdir
#define SYS_stat SYS_stat
#define SYS_unlink SYS_unlink
#define SYS_write SYS_write
#define SYS_writev SYS_writev

// For Starboard builds, the syscall SYS_foo names are not mapped to numbers,
// allowing these wrappers to use the SYS_foo syscall name.
// See src/internal/syscall.h for more details.

// Simple wrappers can be directly replaced with the function name.
#define libc_wrapper_SYS_close(fildes) close(fildes)
#define libc_wrapper_SYS_fcntl(fd, op, ...) fcntl(fd, op, ##__VA_ARGS__)
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

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_SYSCALL_ARCH_H_
