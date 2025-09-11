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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixUnistdSysconfTest : public ::testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    // Clear errno before each test to ensure a clean state.
    errno = 0;
  }
};

class PosixUnistdSysconfRequiredValueTest : public PosixUnistdSysconfTest {};

class PosixUnistdSysconfOptionalValueTest : public PosixUnistdSysconfTest {};

TEST_P(PosixUnistdSysconfRequiredValueTest, RequiredScValuesAreValidNames) {
  int sc_name = GetParam();
  long ret = sysconf(sc_name);
  // The result should be greater than 0, and errno should not be set.
  EXPECT_GT(ret, 0);
  EXPECT_EQ(errno, 0) << "sysconf failed with error: " << strerror(errno);
}

TEST_P(PosixUnistdSysconfOptionalValueTest, OptionalScValuesAreValidNames) {
  int sc_name = GetParam();
  sysconf(sc_name);

  EXPECT_NE(errno, EINVAL)
      << "sysconf(" << sc_name
      << ") failed with EINVAL, meaning the name is considered invalid.";
}

TEST_F(PosixUnistdSysconfTest, InvalidNameFails) {
  // Use a value that is guaranteed to be an invalid name.
  long result = sysconf(-1);
  // On error, sysconf should return -1 and set errno.
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL)
      << "sysconf with an invalid name failed with an unexpected error: "
      << strerror(errno);
}

INSTANTIATE_TEST_SUITE_P(PosixUnistdSysconfTests,
                         PosixUnistdSysconfRequiredValueTest,
                         ::testing::Values(_SC_CLK_TCK,
                                           _SC_PAGESIZE,
                                           _SC_NPROCESSORS_ONLN,
                                           _SC_PHYS_PAGES));

INSTANTIATE_TEST_SUITE_P(PosixUnistdSysconfTests,
                         PosixUnistdSysconfOptionalValueTest,
                         ::testing::Values(
#if defined(_SC_ARG_MAX)
                             _SC_ARG_MAX,
#endif  // defined(_SC_ARG_MAX)
#if defined(_SC_CHILD_MAX)
                             _SC_CHILD_MAX,
#endif  // defined(_SC_CHILD_MAX)
#if defined(_SC_NGROUPS_MAX)
                             _SC_NGROUPS_MAX,
#endif  // defined(_SC_NGROUPS_MAX)
#if defined(_SC_OPEN_MAX)
                             _SC_OPEN_MAX,
#endif  // defined(_SC_OPEN_MAX)
#if defined(_SC_STREAM_MAX)
                             _SC_STREAM_MAX,
#endif  // defined(_SC_STREAM_MAX)
#if defined(_SC_TZNAME_MAX)
                             _SC_TZNAME_MAX,
#endif  // defined(_SC_TZNAME_MAX)
#if defined(_SC_JOB_CONTROL)
                             _SC_JOB_CONTROL,
#endif  // defined(_SC_JOB_CONTROL)
#if defined(_SC_SAVED_IDS)
                             _SC_SAVED_IDS,
#endif  // defined(_SC_SAVED_IDS)
#if defined(_SC_REALTIME_SIGNALS)
                             _SC_REALTIME_SIGNALS,
#endif  // defined(_SC_REALTIME_SIGNALS)
#if defined(_SC_PRIORITY_SCHEDULING)
                             _SC_PRIORITY_SCHEDULING,
#endif  // defined(_SC_PRIORITY_SCHEDULING)
#if defined(_SC_TIMERS)
                             _SC_TIMERS,
#endif  // defined(_SC_TIMERS)
#if defined(_SC_ASYNCHRONOUS_IO)
                             _SC_ASYNCHRONOUS_IO,
#endif  // defined(_SC_ASYNCHRONOUS_IO)
#if defined(_SC_PRIORITIZED_IO)
                             _SC_PRIORITIZED_IO,
#endif  // defined(_SC_PRIORITIZED_IO)
#if defined(_SC_SYNCHRONIZED_IO)
                             _SC_SYNCHRONIZED_IO,
#endif  // defined(_SC_SYNCHRONIZED_IO)
#if defined(_SC_FSYNC)
                             _SC_FSYNC,
#endif  // defined(_SC_FSYNC)
#if defined(_SC_MAPPED_FILES)
                             _SC_MAPPED_FILES,
#endif  // defined(_SC_MAPPED_FILES)
#if defined(_SC_MEMLOCK)
                             _SC_MEMLOCK,
#endif  // defined(_SC_MEMLOCK)
#if defined(_SC_MEMLOCK_RANGE)
                             _SC_MEMLOCK_RANGE,
#endif  // defined(_SC_MEMLOCK_RANGE)
#if defined(_SC_MEMORY_PROTECTION)
                             _SC_MEMORY_PROTECTION,
#endif  // defined(_SC_MEMORY_PROTECTION)
#if defined(_SC_MESSAGE_PASSING)
                             _SC_MESSAGE_PASSING,
#endif  // defined(_SC_MESSAGE_PASSING)
#if defined(_SC_SEMAPHORES)
                             _SC_SEMAPHORES,
#endif  // defined(_SC_SEMAPHORES)
#if defined(_SC_SHARED_MEMORY_OBJECTS)
                             _SC_SHARED_MEMORY_OBJECTS,
#endif  // defined(_SC_SHARED_MEMORY_OBJECTS)
#if defined(_SC_AIO_LISTIO_MAX)
                             _SC_AIO_LISTIO_MAX,
#endif  // defined(_SC_AIO_LISTIO_MAX)
#if defined(_SC_AIO_MAX)
                             _SC_AIO_MAX,
#endif  // defined(_SC_AIO_MAX)
#if defined(_SC_AIO_PRIO_DELTA_MAX)
                             _SC_AIO_PRIO_DELTA_MAX,
#endif  // defined(_SC_AIO_PRIO_DELTA_MAX)
#if defined(_SC_DELAYTIMER_MAX)
                             _SC_DELAYTIMER_MAX,
#endif  // defined(_SC_DELAYTIMER_MAX)
#if defined(_SC_MQ_OPEN_MAX)
                             _SC_MQ_OPEN_MAX,
#endif  // defined(_SC_MQ_OPEN_MAX)
#if defined(_SC_MQ_PRIO_MAX)
                             _SC_MQ_PRIO_MAX,
#endif  // defined(_SC_MQ_PRIO_MAX)
#if defined(_SC_VERSION)
                             _SC_VERSION,
#endif  // defined(_SC_VERSION)
#if defined(_SC_PAGESIZE)
                             _SC_PAGESIZE,
#endif  // defined(_SC_PAGESIZE)
#if defined(_SC_RTSIG_MAX)
                             _SC_RTSIG_MAX,
#endif  // defined(_SC_RTSIG_MAX)
#if defined(_SC_SEM_NSEMS_MAX)
                             _SC_SEM_NSEMS_MAX,
#endif  // defined(_SC_SEM_NSEMS_MAX)
#if defined(_SC_SEM_VALUE_MAX)
                             _SC_SEM_VALUE_MAX,
#endif  // defined(_SC_SEM_VALUE_MAX)
#if defined(_SC_SIGQUEUE_MAX)
                             _SC_SIGQUEUE_MAX,
#endif  // defined(_SC_SIGQUEUE_MAX)
#if defined(_SC_TIMER_MAX)
                             _SC_TIMER_MAX,
#endif  // defined(_SC_TIMER_MAX)
#if defined(_SC_BC_BASE_MAX)
                             _SC_BC_BASE_MAX,
#endif  // defined(_SC_BC_BASE_MAX)
#if defined(_SC_BC_DIM_MAX)
                             _SC_BC_DIM_MAX,
#endif  // defined(_SC_BC_DIM_MAX)
#if defined(_SC_BC_SCALE_MAX)
                             _SC_BC_SCALE_MAX,
#endif  // defined(_SC_BC_SCALE_MAX)
#if defined(_SC_BC_STRING_MAX)
                             _SC_BC_STRING_MAX,
#endif  // defined(_SC_BC_STRING_MAX)
#if defined(_SC_COLL_WEIGHTS_MAX)
                             _SC_COLL_WEIGHTS_MAX,
#endif  // defined(_SC_COLL_WEIGHTS_MAX)
#if defined(_SC_EXPR_NEST_MAX)
                             _SC_EXPR_NEST_MAX,
#endif  // defined(_SC_EXPR_NEST_MAX)
#if defined(_SC_LINE_MAX)
                             _SC_LINE_MAX,
#endif  // defined(_SC_LINE_MAX)
#if defined(_SC_RE_DUP_MAX)
                             _SC_RE_DUP_MAX,
#endif  // defined(_SC_RE_DUP_MAX)
#if defined(_SC_2_VERSION)
                             _SC_2_VERSION,
#endif  // defined(_SC_2_VERSION)
#if defined(_SC_2_C_BIND)
                             _SC_2_C_BIND,
#endif  // defined(_SC_2_C_BIND)
#if defined(_SC_2_C_DEV)
                             _SC_2_C_DEV,
#endif  // defined(_SC_2_C_DEV)
#if defined(_SC_2_FORT_DEV)
                             _SC_2_FORT_DEV,
#endif  // defined(_SC_2_FORT_DEV)
#if defined(_SC_2_FORT_RUN)
                             _SC_2_FORT_RUN,
#endif  // defined(_SC_2_FORT_RUN)
#if defined(_SC_2_SW_DEV)
                             _SC_2_SW_DEV,
#endif  // defined(_SC_2_SW_DEV)
#if defined(_SC_2_LOCALEDEF)
                             _SC_2_LOCALEDEF,
#endif  // defined(_SC_2_LOCALEDEF)
#if defined(_SC_IOV_MAX)
                             _SC_IOV_MAX,
#endif  // defined(_SC_IOV_MAX)
#if defined(_SC_THREADS)
                             _SC_THREADS,
#endif  // defined(_SC_THREADS)
#if defined(_SC_THREAD_SAFE_FUNCTIONS)
                             _SC_THREAD_SAFE_FUNCTIONS,
#endif  // defined(_SC_THREAD_SAFE_FUNCTIONS)
#if defined(_SC_GETGR_R_SIZE_MAX)
                             _SC_GETGR_R_SIZE_MAX,
#endif  // defined(_SC_GETGR_R_SIZE_MAX)
#if defined(_SC_GETPW_R_SIZE_MAX)
                             _SC_GETPW_R_SIZE_MAX,
#endif  // defined(_SC_GETPW_R_SIZE_MAX)
#if defined(_SC_LOGIN_NAME_MAX)
                             _SC_LOGIN_NAME_MAX,
#endif  // defined(_SC_LOGIN_NAME_MAX)
#if defined(_SC_TTY_NAME_MAX)
                             _SC_TTY_NAME_MAX,
#endif  // defined(_SC_TTY_NAME_MAX)
#if defined(_SC_THREAD_DESTRUCTOR_ITERATIONS)
                             _SC_THREAD_DESTRUCTOR_ITERATIONS,
#endif  // defined(_SC_THREAD_DESTRUCTOR_ITERATIONS)
#if defined(_SC_THREAD_KEYS_MAX)
                             _SC_THREAD_KEYS_MAX,
#endif  // defined(_SC_THREAD_KEYS_MAX)
#if defined(_SC_THREAD_STACK_MIN)
                             _SC_THREAD_STACK_MIN,
#endif  // defined(_SC_THREAD_STACK_MIN)
#if defined(_SC_THREAD_THREADS_MAX)
                             _SC_THREAD_THREADS_MAX,
#endif  // defined(_SC_THREAD_THREADS_MAX)
#if defined(_SC_THREAD_ATTR_STACKADDR)
                             _SC_THREAD_ATTR_STACKADDR,
#endif  // defined(_SC_THREAD_ATTR_STACKADDR)
#if defined(_SC_THREAD_ATTR_STACKSIZE)
                             _SC_THREAD_ATTR_STACKSIZE,
#endif  // defined(_SC_THREAD_ATTR_STACKSIZE)
#if defined(_SC_THREAD_PRIORITY_SCHEDULING)
                             _SC_THREAD_PRIORITY_SCHEDULING,
#endif  // defined(_SC_THREAD_PRIORITY_SCHEDULING)
#if defined(_SC_THREAD_PRIO_INHERIT)
                             _SC_THREAD_PRIO_INHERIT,
#endif  // defined(_SC_THREAD_PRIO_INHERIT)
#if defined(_SC_THREAD_PRIO_PROTECT)
                             _SC_THREAD_PRIO_PROTECT,
#endif  // defined(_SC_THREAD_PRIO_PROTECT)
#if defined(_SC_THREAD_PROCESS_SHARED)
                             _SC_THREAD_PROCESS_SHARED,
#endif  // defined(_SC_THREAD_PROCESS_SHARED)
#if defined(_SC_NPROCESSORS_CONF)
                             _SC_NPROCESSORS_CONF,
#endif  // defined(_SC_NPROCESSORS_CONF)
#if defined(_SC_ATEXIT_MAX)
                             _SC_ATEXIT_MAX,
#endif  // defined(_SC_ATEXIT_MAX)
#if defined(_SC_XOPEN_VERSION)
                             _SC_XOPEN_VERSION,
#endif  // defined(_SC_XOPEN_VERSION)
#if defined(_SC_XOPEN_UNIX)
                             _SC_XOPEN_UNIX,
#endif  // defined(_SC_XOPEN_UNIX)
#if defined(_SC_XOPEN_CRYPT)
                             _SC_XOPEN_CRYPT,
#endif  // defined(_SC_XOPEN_CRYPT)
#if defined(_SC_XOPEN_ENH_I18N)
                             _SC_XOPEN_ENH_I18N,
#endif  // defined(_SC_XOPEN_ENH_I18N)
#if defined(_SC_XOPEN_SHM)
                             _SC_XOPEN_SHM,
#endif  // defined(_SC_XOPEN_SHM)
#if defined(_SC_2_CHAR_TERM)
                             _SC_2_CHAR_TERM,
#endif  // defined(_SC_2_CHAR_TERM)
#if defined(_SC_2_UPE)
                             _SC_2_UPE,
#endif  // defined(_SC_2_UPE)
#if defined(_SC_XBS5_ILP32_OFF32)
                             _SC_XBS5_ILP32_OFF32,
#endif  // defined(_SC_XBS5_ILP32_OFF32)
#if defined(_SC_XBS5_ILP32_OFFBIG)
                             _SC_XBS5_ILP32_OFFBIG,
#endif  // defined(_SC_XBS5_ILP32_OFFBIG)
#if defined(_SC_XBS5_LP64_OFF64)
                             _SC_XBS5_LP64_OFF64,
#endif  // defined(_SC_XBS5_LP64_OFF64)
#if defined(_SC_XBS5_LPBIG_OFFBIG)
                             _SC_XBS5_LPBIG_OFFBIG,
#endif  // defined(_SC_XBS5_LPBIG_OFFBIG)
#if defined(_SC_XOPEN_LEGACY)
                             _SC_XOPEN_LEGACY,
#endif  // defined(_SC_XOPEN_LEGACY)
#if defined(_SC_XOPEN_REALTIME)
                             _SC_XOPEN_REALTIME,
#endif  // defined(_SC_XOPEN_REALTIME)
#if defined(_SC_XOPEN_REALTIME_THREADS)
                             _SC_XOPEN_REALTIME_THREADS,
#endif  // defined(_SC_XOPEN_REALTIME_THREADS)
#if defined(_SC_ADVISORY_INFO)
                             _SC_ADVISORY_INFO,
#endif  // defined(_SC_ADVISORY_INFO)
#if defined(_SC_BARRIERS)
                             _SC_BARRIERS,
#endif  // defined(_SC_BARRIERS)
#if defined(_SC_CLOCK_SELECTION)
                             _SC_CLOCK_SELECTION,
#endif  // defined(_SC_CLOCK_SELECTION)
#if defined(_SC_CPUTIME)
                             _SC_CPUTIME,
#endif  // defined(_SC_CPUTIME)
#if defined(_SC_THREAD_CPUTIME)
                             _SC_THREAD_CPUTIME,
#endif  // defined(_SC_THREAD_CPUTIME)
#if defined(_SC_MONOTONIC_CLOCK)
                             _SC_MONOTONIC_CLOCK,
#endif  // defined(_SC_MONOTONIC_CLOCK)
#if defined(_SC_READER_WRITER_LOCKS)
                             _SC_READER_WRITER_LOCKS,
#endif  // defined(_SC_READER_WRITER_LOCKS)
#if defined(_SC_SPIN_LOCKS)
                             _SC_SPIN_LOCKS,
#endif  // defined(_SC_SPIN_LOCKS)
#if defined(_SC_REGEXP)
                             _SC_REGEXP,
#endif  // defined(_SC_REGEXP)
#if defined(_SC_SHELL)
                             _SC_SHELL,
#endif  // defined(_SC_SHELL)
#if defined(_SC_SPAWN)
                             _SC_SPAWN,
#endif  // defined(_SC_SPAWN)
#if defined(_SC_SPORADIC_SERVER)
                             _SC_SPORADIC_SERVER,
#endif  // defined(_SC_SPORADIC_SERVER)
#if defined(_SC_THREAD_SPORADIC_SERVER)
                             _SC_THREAD_SPORADIC_SERVER,
#endif  // defined(_SC_THREAD_SPORADIC_SERVER)
#if defined(_SC_TIMEOUTS)
                             _SC_TIMEOUTS,
#endif  // defined(_SC_TIMEOUTS)
#if defined(_SC_TYPED_MEMORY_OBJECTS)
                             _SC_TYPED_MEMORY_OBJECTS,
#endif  // defined(_SC_TYPED_MEMORY_OBJECTS)
#if defined(_SC_2_PBS)
                             _SC_2_PBS,
#endif  // defined(_SC_2_PBS)
#if defined(_SC_2_PBS_ACCOUNTING)
                             _SC_2_PBS_ACCOUNTING,
#endif  // defined(_SC_2_PBS_ACCOUNTING)
#if defined(_SC_2_PBS_LOCATE)
                             _SC_2_PBS_LOCATE,
#endif  // defined(_SC_2_PBS_LOCATE)
#if defined(_SC_2_PBS_MESSAGE)
                             _SC_2_PBS_MESSAGE,
#endif  // defined(_SC_2_PBS_MESSAGE)
#if defined(_SC_2_PBS_TRACK)
                             _SC_2_PBS_TRACK,
#endif  // defined(_SC_2_PBS_TRACK)
#if defined(_SC_SYMLOOP_MAX)
                             _SC_SYMLOOP_MAX,
#endif  // defined(_SC_SYMLOOP_MAX)
#if defined(_SC_2_PBS_CHECKPOINT)
                             _SC_2_PBS_CHECKPOINT,
#endif  // defined(_SC_2_PBS_CHECKPOINT)
#if defined(_SC_V6_ILP32_OFF32)
                             _SC_V6_ILP32_OFF32,
#endif  // defined(_SC_V6_ILP32_OFF32)
#if defined(_SC_V6_ILP32_OFFBIG)
                             _SC_V6_ILP32_OFFBIG,
#endif  // defined(_SC_V6_ILP32_OFFBIG)
#if defined(_SC_V6_LP64_OFF64)
                             _SC_V6_LP64_OFF64,
#endif  // defined(_SC_V6_LP64_OFF64)
#if defined(_SC_V6_LPBIG_OFFBIG)
                             _SC_V6_LPBIG_OFFBIG,
#endif  // defined(_SC_V6_LPBIG_OFFBIG)
#if defined(_SC_HOST_NAME_MAX)
                             _SC_HOST_NAME_MAX,
#endif  // defined(_SC_HOST_NAME_MAX)
#if defined(_SC_TRACE)
                             _SC_TRACE,
#endif  // defined(_SC_TRACE)
#if defined(_SC_TRACE_EVENT_FILTER)
                             _SC_TRACE_EVENT_FILTER,
#endif  // defined(_SC_TRACE_EVENT_FILTER)
#if defined(_SC_TRACE_INHERIT)
                             _SC_TRACE_INHERIT,
#endif  // defined(_SC_TRACE_INHERIT)
#if defined(_SC_TRACE_LOG)
                             _SC_TRACE_LOG,
#endif  // defined(_SC_TRACE_LOG)
#if defined(_SC_IPV6)
                             _SC_IPV6,
#endif  // defined(_SC_IPV6)
#if defined(_SC_RAW_SOCKETS)
                             _SC_RAW_SOCKETS,
#endif  // defined(_SC_RAW_SOCKETS)
#if defined(_SC_V7_ILP32_OFF32)
                             _SC_V7_ILP32_OFF32,
#endif  // defined(_SC_V7_ILP32_OFF32)
#if defined(_SC_V7_ILP32_OFFBIG)
                             _SC_V7_ILP32_OFFBIG,
#endif  // defined(_SC_V7_ILP32_OFFBIG)
#if defined(_SC_V7_LP64_OFF64)
                             _SC_V7_LP64_OFF64,
#endif  // defined(_SC_V7_LP64_OFF64)
#if defined(_SC_V7_LPBIG_OFFBIG)
                             _SC_V7_LPBIG_OFFBIG,
#endif  // defined(_SC_V7_LPBIG_OFFBIG)
#if defined(_SC_TRACE_EVENT_NAME_MAX)
                             _SC_TRACE_EVENT_NAME_MAX,
#endif  // defined(_SC_TRACE_EVENT_NAME_MAX)
#if defined(_SC_TRACE_NAME_MAX)
                             _SC_TRACE_NAME_MAX,
#endif  // defined(_SC_TRACE_NAME_MAX)
#if defined(_SC_TRACE_SYS_MAX)
                             _SC_TRACE_SYS_MAX,
#endif  // defined(_SC_TRACE_SYS_MAX)
#if defined(_SC_TRACE_USER_EVENT_MAX)
                             _SC_TRACE_USER_EVENT_MAX,
#endif  // defined(_SC_TRACE_USER_EVENT_MAX)
#if defined(_SC_XOPEN_STREAMS)
                             _SC_XOPEN_STREAMS
#endif  // defined(_SC_XOPEN_STREAMS)
                             ));

}  // namespace
}  // namespace nplb
}  // namespace starboard
