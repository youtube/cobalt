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
#define SYS_openat SYS_openat
#define SYS_unlinkat SYS_unlinkat
#define SYS_fstatat SYS_fstatat

// For Starboard builds, the syscall SYS_foo names are not mapped to numbers,
// allowing these wrappers to use the SYS_foo syscall name.
// See src/internal/syscall.h for more details.

// Simple wrappers can be directly replaced with the function name.
#define libc_wrapper_SYS_close(fildes) close(fildes)
#define libc_wrapper_SYS_fcntl(fd, op, ...) fcntl(fd, op, ##__VA_ARGS__)
#define libc_wrapper_SYS_gettid() gettid()
#define libc_wrapper_SYS_ioctl(fd, op, ...) ioctl(fd, op, ##__VA_ARGS__)
#define libc_wrapper_SYS_lseek(fildes, offset, whence) lseek(fildes, offset, whence)
#define libc_wrapper_SYS_read(fildes, buf, nbyte) read(fildes, buf, nbyte)
#define libc_wrapper_SYS_readv(fildes, iov, iovcnt) readv(fildes, iov, iovcnt)
#define libc_wrapper_SYS_write(fildes, buf, nbyte) write(fildes, buf, nbyte)
#define libc_wrapper_SYS_writev(fildes, iov, iovcnt) writev(fildes, iov, iovcnt)
#define libc_wrapper_SYS_fstatat(fildes, pathname, statbuf, flag) fstatat(fildes, pathname, statbuf, flag)
#define libc_wrapper_SYS_openat(fildes, pathname, flags, mode) openat(fildes, pathname, flags, mode)
#define libc_wrapper_SYS_unlinkat(fildes, pathname, flag) unlinkat(fildes, pathname, flag)

// Redirection wrappers.
#define libc_wrapper_SYS_lstat(pathname, statbuf) fstatat(AT_FDCWD, pathname, statbuf, AT_SYMLINK_NOFOLLOW)
#define libc_wrapper_SYS_open(pathname, flags, mode) openat(AT_FDCWD, pathname, flags, mode)
#define libc_wrapper_SYS_rmdir(pathname) unlinkat(AT_FDCWD, pathname, AT_REMOVEDIR)
#define libc_wrapper_SYS_stat(pathname, statbuf) fstatat(AT_FDCWD, pathname, statbuf, 0)
#define libc_wrapper_SYS_unlink(pathname) unlinkat(AT_FDCWD, pathname, 0)

// Define wrappers for unsupported syscalls that it called by code that can
// handle unsupported syscall functions.
#define libc_wrapper_SYS_memfd_create(name, flags) (errno = ENOSYS, -1)

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_SYSCALL_ARCH_H_
