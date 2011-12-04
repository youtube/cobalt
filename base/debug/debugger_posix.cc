// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "build/build_config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#if !defined(OS_ANDROID)
#include <execinfo.h>
#endif

#if defined(__GLIBCXX__)
#include <cxxabi.h>
#endif

#if defined(OS_MACOSX)
#include <AvailabilityMacros.h>
#endif

#if defined(OS_MACOSX) || defined(OS_BSD)
#include <sys/sysctl.h>
#endif

#if defined(OS_FREEBSD)
#include <sys/user.h>
#endif

#include <ostream>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/safe_strerror_posix.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"

#if defined(USE_SYMBOLIZE)
#include "base/third_party/symbolize/symbolize.h"
#endif

#if defined(OS_ANDROID)
#include "base/threading/platform_thread.h"
#endif

namespace base {
namespace debug {

bool SpawnDebuggerOnProcess(unsigned /* process_id */) {
  NOTIMPLEMENTED();
  return false;
}

#if defined(OS_MACOSX) || defined(OS_BSD)

// Based on Apple's recommended method as described in
// http://developer.apple.com/qa/qa2004/qa1361.html
bool BeingDebugged() {
  // If the process is sandboxed then we can't use the sysctl, so cache the
  // value.
  static bool is_set = false;
  static bool being_debugged = false;

  if (is_set) {
    return being_debugged;
  }

  // Initialize mib, which tells sysctl what info we want.  In this case,
  // we're looking for information about a specific process ID.
  int mib[] = {
    CTL_KERN,
    KERN_PROC,
    KERN_PROC_PID,
    getpid()
#if defined(OS_OPENBSD)
    , sizeof(struct kinfo_proc),
    0
#endif
  };

  // Caution: struct kinfo_proc is marked __APPLE_API_UNSTABLE.  The source and
  // binary interfaces may change.
  struct kinfo_proc info;
  size_t info_size = sizeof(info);

#if defined(OS_OPENBSD)
  if (sysctl(mib, arraysize(mib), NULL, &info_size, NULL, 0) < 0)
    return -1;

  mib[5] = (info_size / sizeof(struct kinfo_proc));
#endif

  int sysctl_result = sysctl(mib, arraysize(mib), &info, &info_size, NULL, 0);
  DCHECK_EQ(sysctl_result, 0);
  if (sysctl_result != 0) {
    is_set = true;
    being_debugged = false;
    return being_debugged;
  }

  // This process is being debugged if the P_TRACED flag is set.
  is_set = true;
#if defined(OS_FREEBSD)
  being_debugged = (info.ki_flag & P_TRACED) != 0;
#elif defined(OS_BSD)
  being_debugged = (info.p_flag & P_TRACED) != 0;
#else
  being_debugged = (info.kp_proc.p_flag & P_TRACED) != 0;
#endif
  return being_debugged;
}

#elif defined(OS_LINUX) || defined(OS_ANDROID)

// We can look in /proc/self/status for TracerPid.  We are likely used in crash
// handling, so we are careful not to use the heap or have side effects.
// Another option that is common is to try to ptrace yourself, but then we
// can't detach without forking(), and that's not so great.
// static
bool BeingDebugged() {
  int status_fd = open("/proc/self/status", O_RDONLY);
  if (status_fd == -1)
    return false;

  // We assume our line will be in the first 1024 characters and that we can
  // read this much all at once.  In practice this will generally be true.
  // This simplifies and speeds up things considerably.
  char buf[1024];

  ssize_t num_read = HANDLE_EINTR(read(status_fd, buf, sizeof(buf)));
  if (HANDLE_EINTR(close(status_fd)) < 0)
    return false;

  if (num_read <= 0)
    return false;

  StringPiece status(buf, num_read);
  StringPiece tracer("TracerPid:\t");

  StringPiece::size_type pid_index = status.find(tracer);
  if (pid_index == StringPiece::npos)
    return false;

  // Our pid is 0 without a debugger, assume this for any pid starting with 0.
  pid_index += tracer.size();
  return pid_index < status.size() && status[pid_index] != '0';
}

#else

bool BeingDebugged() {
  NOTIMPLEMENTED();
  return false;
}

#endif

// We want to break into the debugger in Debug mode, and cause a crash dump in
// Release mode. Breakpad behaves as follows:
//
// +-------+-----------------+-----------------+
// | OS    | Dump on SIGTRAP | Dump on SIGABRT |
// +-------+-----------------+-----------------+
// | Linux |       N         |        Y        |
// | Mac   |       Y         |        N        |
// +-------+-----------------+-----------------+
//
// Thus we do the following:
// Linux: Debug mode, send SIGTRAP; Release mode, send SIGABRT.
// Mac: Always send SIGTRAP.

#if defined(NDEBUG) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#define DEBUG_BREAK() abort()
#elif defined(OS_NACL)
// The NaCl verifier doesn't let use use int3.  For now, we call abort().  We
// should ask for advice from some NaCl experts about the optimum thing here.
// http://code.google.com/p/nativeclient/issues/detail?id=645
#define DEBUG_BREAK() abort()
#elif defined(ARCH_CPU_ARM_FAMILY)
#if defined(OS_ANDROID)
// Though Android has a "helpful" process called debuggerd to catch native
// signals on the general assumption that they are fatal errors, we've had great
// difficulty continuing in a debugger once we stop from SIGINT triggered by
// native code.
//
// Use GDB to set |go| to 1 to resume execution.
#define DEBUG_BREAK() do { \
  volatile int go = 0;             \
  while (!go) { base::PlatformThread::Sleep(100); }   \
} while (0)
#else
// ARM && !ANDROID
#define DEBUG_BREAK() asm("bkpt 0")
#endif
#else
#define DEBUG_BREAK() asm("int3")
#endif

void BreakDebugger() {
  DEBUG_BREAK();
#if defined(NDEBUG)
  _exit(1);
#endif
}

}  // namespace debug
}  // namespace base
