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

#ifndef THIRD_PARTY_MUSL_SRC_STARBOARD_INTERNAL_BITS_SYSCALL_IMPL_H_
#define THIRD_PARTY_MUSL_SRC_STARBOARD_INTERNAL_BITS_SYSCALL_IMPL_H_

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

#include "syscall_arch.h"

#define syscall(name, ...) libc_wrapper_##name(__VA_ARGS__)

// Map `libc_wrapper_SYS_ioctl(int fd, op,...)` to `ioctl_op(int fd,...)` calls
// to allow separate implementation per ioctl operation.
#define __LIBC_WRAPPER_SYS_IOCTL_CONCAT_X(a,b) a##b
#define __LIBC_WRAPPER_SYS_IOCTL_CONCAT(a,b) __LIBC_WRAPPER_SYS_IOCTL_CONCAT_X(a,b)
#define __LIBC_WRAPPER_SYS_IOCTL_DISP(b, op,...) __LIBC_WRAPPER_SYS_IOCTL_CONCAT(b,op)(__VA_ARGS__)
#define libc_wrapper_SYS_ioctl(fd, op,...) __LIBC_WRAPPER_SYS_IOCTL_DISP(ioctl_, op, fd, __VA_ARGS__)

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_INTERNAL_BITS_SYSCALL_IMPL_H_
