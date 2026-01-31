// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"

#if SB_IS(MODULAR)

#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <wchar.h>

SB_COMPILE_ASSERT(sizeof(int8_t) == SB_SIZE_OF_CHAR,
                  SB_SIZE_OF_CHAR_is_inconsistent_with_sizeof_int8_t);

SB_COMPILE_ASSERT(sizeof(uint8_t) == SB_SIZE_OF_CHAR,
                  SB_SIZE_OF_CHAR_is_inconsistent_with_sizeof_uint8_t);

SB_COMPILE_ASSERT(sizeof(clockid_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_clockid_t);

SB_COMPILE_ASSERT(sizeof(gid_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_gid_t);

SB_COMPILE_ASSERT(sizeof(id_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_id_t);

SB_COMPILE_ASSERT(sizeof(int32_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_int32_t);

SB_COMPILE_ASSERT(sizeof(key_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_key_t);

SB_COMPILE_ASSERT(sizeof(mode_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_mode_t);

SB_COMPILE_ASSERT(sizeof(pid_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_pid_t);

SB_COMPILE_ASSERT(sizeof(uid_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_uid_t);

SB_COMPILE_ASSERT(sizeof(uint32_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_uint32_t);

SB_COMPILE_ASSERT(sizeof(useconds_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_useconds_t);

SB_COMPILE_ASSERT(sizeof(wint_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_wint_t);

#if SB_IS(ARCH_ARM64)
SB_COMPILE_ASSERT(sizeof(blksize_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_blksize_t);
#else   // !SB_IS(ARCH_ARM64)
SB_COMPILE_ASSERT(sizeof(blksize_t) == SB_SIZE_OF_LONG,
                  SB_SIZE_OF_LONG_is_inconsistent_with_sizeof_blksize_t);
#endif  // SB_IS(ARCH_ARM64)

SB_COMPILE_ASSERT(sizeof(clock_t) == SB_SIZE_OF_LONG,
                  SB_SIZE_OF_LONG_is_inconsistent_with_sizeof_clock_t);

// WARNING: do not change unless you know what you are doing. The following
// define, SB_TYPE_SIZE_EXPECTED, is used to simplify the size checking across
// different architectures.

#if SB_IS(32_BIT)
#define SB_EXPECTED_TYPE_SIZE SB_SIZE_OF_LLONG
#elif SB_IS(64_BIT)
#define SB_EXPECTED_TYPE_SIZE SB_SIZE_OF_LONG
#endif  // SB_IS(64_BIT)

SB_COMPILE_ASSERT(sizeof(suseconds_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_suseconds_t);

SB_COMPILE_ASSERT(sizeof(time_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_time_t);

SB_COMPILE_ASSERT(sizeof(wctype_t) == SB_SIZE_OF_LONG,
                  SB_SIZE_OF_LONG_is_inconsistent_with_sizeof_wctype_t);

SB_COMPILE_ASSERT(sizeof(timer_t) == SB_SIZE_OF_POINTER,
                  SB_SIZE_OF_POINTER_is_inconsistent_with_sizeof_timer_t);

SB_COMPILE_ASSERT(sizeof(int16_t) == SB_SIZE_OF_SHORT,
                  SB_SIZE_OF_SHORT_is_inconsistent_with_sizeof_int16_t);

SB_COMPILE_ASSERT(sizeof(uint16_t) == SB_SIZE_OF_SHORT,
                  SB_SIZE_OF_SHORT_is_inconsistent_with_sizeof_uint16_t);

#undef SB_EXPECTED_TYPE_SIZE

#if SB_IS(32_BIT)
#define SB_EXPECTED_TYPE_SIZE SB_SIZE_OF_INT
#elif SB_IS(64_BIT)
#define SB_EXPECTED_TYPE_SIZE SB_SIZE_OF_LONG
#endif  // SB_IS(64_BIT)

SB_COMPILE_ASSERT(sizeof(intptr_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_intptr_t);

#if SB_IS(ARCH_ARM64)
SB_COMPILE_ASSERT(sizeof(nlink_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_nlink_t);
#else   // !SB_IS(ARCH_ARM64)
SB_COMPILE_ASSERT(sizeof(nlink_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_nlink_t);
#endif  // SB_IS(ARCH_ARM64)

SB_COMPILE_ASSERT(sizeof(ptrdiff_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_ptrdiff_t);

SB_COMPILE_ASSERT(sizeof(register_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_register_t);

SB_COMPILE_ASSERT(sizeof(size_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_size_t);

SB_COMPILE_ASSERT(sizeof(ssize_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_ssize_t);

SB_COMPILE_ASSERT(sizeof(uintptr_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_uintptr_t);

#undef SB_EXPECTED_TYPE_SIZE

#if SB_IS(32_BIT)
#define SB_EXPECTED_TYPE_SIZE SB_SIZE_OF_LLONG
#elif SB_IS(64_BIT)
#define SB_EXPECTED_TYPE_SIZE SB_SIZE_OF_LONG
#endif  // SB_IS(64_BIT)

SB_COMPILE_ASSERT(sizeof(blkcnt_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_blkcnt_t);

SB_COMPILE_ASSERT(sizeof(dev_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_dev_t);

SB_COMPILE_ASSERT(sizeof(fsblkcnt_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_fsblkcnt_t);

SB_COMPILE_ASSERT(sizeof(fsfilcnt_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_fsfilcnt_t);

SB_COMPILE_ASSERT(sizeof(ino_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_ino_t);

SB_COMPILE_ASSERT(sizeof(int64_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_int64_t);

SB_COMPILE_ASSERT(sizeof(intmax_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_intmax_t);

SB_COMPILE_ASSERT(sizeof(off_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_off_t);

SB_COMPILE_ASSERT(sizeof(u_int64_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_u_int64_t);

SB_COMPILE_ASSERT(sizeof(uint64_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_uint64_t);

SB_COMPILE_ASSERT(sizeof(uintmax_t) == SB_EXPECTED_TYPE_SIZE,
                  SB_EXPECTED_TYPE_SIZE_is_inconsistent_with_sizeof_uintmax_t);

#undef SB_EXPECTED_TYPE_SIZE

#if SB_IS(ARCH_X86)
#if defined(__WCHAR_TYPE__)
#else   // !defined(__WCHAR_TYPE__)
SB_COMPILE_ASSERT(sizeof(wchar_t) == SB_SIZE_OF_LONG,
                  SB_SIZE_OF_LONG_is_inconsistent_with_sizeof_wchar_t);
#endif  // defined(__WCHAR_TYPE__)
// TODO: Decide if we should, and how, verify that __WCHAR_TYPE__ is the
//       expected size.
#else   // !SB_IS(ARCH_X86)
#if SB_IS(WCHAR_T_UTF16)
SB_COMPILE_ASSERT(sizeof(wchar_t) == 2,
                  SB_IS_WCHAR_T_UTF16_is_inconsistent_with_sizeof_wchar_t);
#else
SB_COMPILE_ASSERT(sizeof(wchar_t) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_wchar_t);
#endif // SB_IS(WCHAR_T_UTF16)
#endif  // SB_IS(ARCH_X86)

// Test for stat size
#if SB_IS(ARCH_ARM64) || SB_IS(ARCH_X64)
SB_COMPILE_ASSERT(offsetof(musl_stat, st_mode) == sizeof(int64_t)*3, SB_ARCH64_st_mode_offset_wrong_for_stat);
SB_COMPILE_ASSERT(offsetof(musl_stat, st_atim) == (sizeof(int64_t)*7 + sizeof(unsigned)*4), SB_ARCH64_st_atim_offset_wrong_for_stat);
//   int64_t /*dev_t*/ st_dev;
//   int64_t /*ino_t*/ st_ino;
//   int64_t /*nlink_t*/ st_nlink;

//   unsigned /*mode_t*/ st_mode;
//   unsigned /*uid_t*/ st_uid;
//   unsigned /*gid_t*/ st_gid;
//   unsigned /*unsigned int*/ __pad0;
//   int64_t /*dev_t*/ st_rdev;
//   int64_t /*off_t*/ st_size;
//   int64_t /*blksize_t*/ st_blksize;
//   int64_t /*blkcnt_t*/ st_blocks;

//   struct musl_timespec /*struct timespec*/ st_atim;
//   struct musl_timespec /*struct timespec*/ st_mtim;
//   struct musl_timespec /*struct timespec*/ st_ctim;
//   int64_t unused[3];
#else
SB_COMPILE_ASSERT(offsetof(musl_stat, st_mode) == (sizeof(uint64_t)+sizeof(uint32_t) + sizeof(int32_t)), SB_st_mode_offset_wrong_for_stat);
SB_COMPILE_ASSERT(offsetof(musl_stat, st_atim) == (sizeof(uint32_t)*9 + sizeof(int64_t)*5), SB_st_atim_offset_wrong_for_stat);

//   uint64_t /*dev_t*/ st_dev;
//   uint32_t /*int*/ _st_dev_padding;
//   int32_t /*long*/ _st_ino_truncated;

//   uint32_t /*mode_t*/ st_mode;
//   uint32_t /*nlink_t*/ st_nlink;
//   uint32_t /*uid_t*/ st_uid;
//   uint32_t /*gid_t*/ st_gid;
//   uint64_t /*dev_t*/ st_rdev;
//   uint32_t /*int*/ __st_rdev_padding;
//   int64_t /*off_t*/ st_size;
//   int32_t /*blksize_t*/ st_blksize;
//   int64_t /*blkcnt_t*/ st_blocks;
//   int32_t _unused[6];
//   uint64_t /*ino_t*/ st_ino;
//   struct musl_timespec /*struct timespec*/ st_atim;
//   struct musl_timespec /*struct timespec*/ st_mtim;
//   struct musl_timespec /*struct timespec*/ st_ctim;
#endif

#endif  // SB_IS(MODULAR)
