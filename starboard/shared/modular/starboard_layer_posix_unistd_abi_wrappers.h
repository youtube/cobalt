// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UNISTD_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UNISTD_ABI_WRAPPERS_H_

#include <stdint.h>
#include <sys/types.h>

#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_signal_abi_wrappers.h"

// The `__abi_wrap_lseek` function converts from the musl's off_t
// type to the platform's off_t which may have different sizes as
// the definition is platform specific.
// The `__abi_wrap_read` function converts ssize_t to int32_t or
// int64_t depending on the platform.
//
// The wrapper is used by all modular builds, including Evergreen.
//
// For Evergreen-based modular builds, we will rely on the exported_symbols.cc
// mapping logic to map calls to file IO functions to `__abi_wrap_` file IO
// functions.
//
// For non-Evergreen modular builds, the Cobalt-side shared library will be
// compiled with code that remaps calls to file IO functions to `__abi_wrap_`
// file IO functions.

// A matching type for the off_t definition in musl.

typedef int64_t musl_off_t;

#if SB_IS(ARCH_ARM64) || SB_IS(ARCH_X64)
typedef int64_t ssize_t;
#else
typedef int32_t ssize_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Define MUSL_SC constants.
#define MUSL_SC_ARG_MAX 0
#define MUSL_SC_CHILD_MAX 1
#define MUSL_SC_CLK_TCK 2
#define MUSL_SC_NGROUPS_MAX 3
#define MUSL_SC_OPEN_MAX 4
#define MUSL_SC_STREAM_MAX 5
#define MUSL_SC_TZNAME_MAX 6
#define MUSL_SC_JOB_CONTROL 7
#define MUSL_SC_SAVED_IDS 8
#define MUSL_SC_REALTIME_SIGNALS 9
#define MUSL_SC_PRIORITY_SCHEDULING 10
#define MUSL_SC_TIMERS 11
#define MUSL_SC_ASYNCHRONOUS_IO 12
#define MUSL_SC_PRIORITIZED_IO 13
#define MUSL_SC_SYNCHRONIZED_IO 14
#define MUSL_SC_FSYNC 15
#define MUSL_SC_MAPPED_FILES 16
#define MUSL_SC_MEMLOCK 17
#define MUSL_SC_MEMLOCK_RANGE 18
#define MUSL_SC_MEMORY_PROTECTION 19
#define MUSL_SC_MESSAGE_PASSING 20
#define MUSL_SC_SEMAPHORES 21
#define MUSL_SC_SHARED_MEMORY_OBJECTS 22
#define MUSL_SC_AIO_LISTIO_MAX 23
#define MUSL_SC_AIO_MAX 24
#define MUSL_SC_AIO_PRIO_DELTA_MAX 25
#define MUSL_SC_DELAYTIMER_MAX 26
#define MUSL_SC_MQ_OPEN_MAX 27
#define MUSL_SC_MQ_PRIO_MAX 28
#define MUSL_SC_VERSION 29
#define MUSL_SC_PAGESIZE 30
#define MUSL_SC_RTSIG_MAX 31
#define MUSL_SC_SEM_NSEMS_MAX 32
#define MUSL_SC_SEM_VALUE_MAX 33
#define MUSL_SC_SIGQUEUE_MAX 34
#define MUSL_SC_TIMER_MAX 35
#define MUSL_SC_BC_BASE_MAX 36
#define MUSL_SC_BC_DIM_MAX 37
#define MUSL_SC_BC_SCALE_MAX 38
#define MUSL_SC_BC_STRING_MAX 39
#define MUSL_SC_COLL_WEIGHTS_MAX 40
#define MUSL_SC_EXPR_NEST_MAX 42
#define MUSL_SC_LINE_MAX 43
#define MUSL_SC_RE_DUP_MAX 44
#define MUSL_SC_2_VERSION 46
#define MUSL_SC_2_C_BIND 47
#define MUSL_SC_2_C_DEV 48
#define MUSL_SC_2_FORT_DEV 49
#define MUSL_SC_2_FORT_RUN 50
#define MUSL_SC_2_SW_DEV 51
#define MUSL_SC_2_LOCALEDEF 52
#define MUSL_SC_IOV_MAX 60
#define MUSL_SC_THREADS 67
#define MUSL_SC_THREAD_SAFE_FUNCTIONS 68
#define MUSL_SC_GETGR_R_SIZE_MAX 69
#define MUSL_SC_GETPW_R_SIZE_MAX 70
#define MUSL_SC_LOGIN_NAME_MAX 71
#define MUSL_SC_TTY_NAME_MAX 72
#define MUSL_SC_THREAD_DESTRUCTOR_ITERATIONS 73
#define MUSL_SC_THREAD_KEYS_MAX 74
#define MUSL_SC_THREAD_STACK_MIN 75
#define MUSL_SC_THREAD_THREADS_MAX 76
#define MUSL_SC_THREAD_ATTR_STACKADDR 77
#define MUSL_SC_THREAD_ATTR_STACKSIZE 78
#define MUSL_SC_THREAD_PRIORITY_SCHEDULING 79
#define MUSL_SC_THREAD_PRIO_INHERIT 80
#define MUSL_SC_THREAD_PRIO_PROTECT 81
#define MUSL_SC_THREAD_PROCESS_SHARED 82
#define MUSL_SC_NPROCESSORS_CONF 83
#define MUSL_SC_NPROCESSORS_ONLN 84
#define MUSL_SC_PHYS_PAGES 85
#define MUSL_SC_ATEXIT_MAX 87
#define MUSL_SC_XOPEN_VERSION 89
#define MUSL_SC_XOPEN_UNIX 91
#define MUSL_SC_XOPEN_CRYPT 92
#define MUSL_SC_XOPEN_ENH_I18N 93
#define MUSL_SC_XOPEN_SHM 94
#define MUSL_SC_2_CHAR_TERM 95
#define MUSL_SC_2_UPE 97
#define MUSL_SC_XBS5_ILP32_OFF32 125
#define MUSL_SC_XBS5_ILP32_OFFBIG 126
#define MUSL_SC_XBS5_LP64_OFF64 127
#define MUSL_SC_XBS5_LPBIG_OFFBIG 128
#define MUSL_SC_XOPEN_LEGACY 129
#define MUSL_SC_XOPEN_REALTIME 130
#define MUSL_SC_XOPEN_REALTIME_THREADS 131
#define MUSL_SC_ADVISORY_INFO 132
#define MUSL_SC_BARRIERS 133
#define MUSL_SC_CLOCK_SELECTION 137
#define MUSL_SC_CPUTIME 138
#define MUSL_SC_THREAD_CPUTIME 139
#define MUSL_SC_MONOTONIC_CLOCK 149
#define MUSL_SC_READER_WRITER_LOCKS 153
#define MUSL_SC_SPIN_LOCKS 154
#define MUSL_SC_REGEXP 155
#define MUSL_SC_SHELL 157
#define MUSL_SC_SPAWN 159
#define MUSL_SC_SPORADIC_SERVER 160
#define MUSL_SC_THREAD_SPORADIC_SERVER 161
#define MUSL_SC_TIMEOUTS 164
#define MUSL_SC_TYPED_MEMORY_OBJECTS 165
#define MUSL_SC_2_PBS 168
#define MUSL_SC_2_PBS_ACCOUNTING 169
#define MUSL_SC_2_PBS_LOCATE 170
#define MUSL_SC_2_PBS_MESSAGE 171
#define MUSL_SC_2_PBS_TRACK 172
#define MUSL_SC_SYMLOOP_MAX 173
#define MUSL_SC_2_PBS_CHECKPOINT 175
#define MUSL_SC_V6_ILP32_OFF32 176
#define MUSL_SC_V6_ILP32_OFFBIG 177
#define MUSL_SC_V6_LP64_OFF64 178
#define MUSL_SC_V6_LPBIG_OFFBIG 179
#define MUSL_SC_HOST_NAME_MAX 180
#define MUSL_SC_TRACE 181
#define MUSL_SC_TRACE_EVENT_FILTER 182
#define MUSL_SC_TRACE_INHERIT 183
#define MUSL_SC_TRACE_LOG 184
#define MUSL_SC_IPV6 235
#define MUSL_SC_RAW_SOCKETS 236
#define MUSL_SC_V7_ILP32_OFF32 237
#define MUSL_SC_V7_ILP32_OFFBIG 238
#define MUSL_SC_V7_LP64_OFF64 239
#define MUSL_SC_V7_LPBIG_OFFBIG 240
#define MUSL_SC_SS_REPL_MAX 241
#define MUSL_SC_TRACE_EVENT_NAME_MAX 242
#define MUSL_SC_TRACE_NAME_MAX 243
#define MUSL_SC_TRACE_SYS_MAX 244
#define MUSL_SC_TRACE_USER_EVENT_MAX 245
#define MUSL_SC_XOPEN_STREAMS 246
#define MUSL_SC_THREAD_ROBUST_PRIO_INHERIT 247
#define MUSL_SC_THREAD_ROBUST_PRIO_PROTECT 248

#define MUSL_PC_LINK_MAX 0
#define MUSL_PC_MAX_CANON 1
#define MUSL_PC_MAX_INPUT 2
#define MUSL_PC_NAME_MAX 3
#define MUSL_PC_PATH_MAX 4
#define MUSL_PC_PIPE_BUF 5
#define MUSL_PC_CHOWN_RESTRICTED 6
#define MUSL_PC_NO_TRUNC 7
#define MUSL_PC_VDISABLE 8
#define MUSL_PC_SYNC_IO 9
#define MUSL_PC_ASYNC_IO 10
#define MUSL_PC_PRIO_IO 11
#define MUSL_PC_SOCK_MAXBUF 12
#define MUSL_PC_FILESIZEBITS 13
#define MUSL_PC_REC_INCR_XFER_SIZE 14
#define MUSL_PC_REC_MAX_XFER_SIZE 15
#define MUSL_PC_REC_MIN_XFER_SIZE 16
#define MUSL_PC_REC_XFER_ALIGN 17
#define MUSL_PC_ALLOC_SIZE_MIN 18
#define MUSL_PC_SYMLINK_MAX 19
#define MUSL_PC_2_SYMLINKS 20

#define MUSL_F_OK 0
#define MUSL_R_OK 4
#define MUSL_W_OK 2
#define MUSL_X_OK 1

SB_EXPORT int __abi_wrap_ftruncate(int fildes, musl_off_t length);

SB_EXPORT musl_off_t __abi_wrap_lseek(int fildes,
                                      musl_off_t offset,
                                      int whence);

SB_EXPORT ssize_t __abi_wrap_read(int fildes, void* buf, size_t nbyte);

SB_EXPORT ssize_t __abi_wrap_write(int fildes, const void* buf, size_t nbyte);

SB_EXPORT long __abi_wrap_sysconf(int name);

SB_EXPORT long __abi_wrap_pathconf(const char* path, int name);

SB_EXPORT musl_uid_t __abi_wrap_geteuid();

SB_EXPORT musl_pid_t __abi_wrap_getpid();

SB_EXPORT int __abi_wrap_access(const char* path, int amode);

SB_EXPORT int __abi_wrap_fchown(int fd, musl_uid_t owner, musl_gid_t group);

SB_EXPORT int __abi_wrap_unlinkat(int fildes, const char* path, int musl_flag);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UNISTD_ABI_WRAPPERS_H_
