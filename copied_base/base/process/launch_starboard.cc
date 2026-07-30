// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/launch.h"

#include "base/notreached.h"

namespace base {

void RaiseProcessToHighPriority() {
  // We don't actually do anything here.  We could try to nice() or
  // setpriority() or sched_getscheduler, but these all require extra rights.
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  NOTREACHED() << "GetAppOutput called, but Cobalt does not "
               << "support multi-processing. This call will fail.";
  return false;
}

bool GetAppOutputAndError(const CommandLine& cl, std::string* output) {
  NOTREACHED();
  return false;
}

Process LaunchProcess(const CommandLine& cmdline,
                      const LaunchOptions& options) {
  NOTREACHED() << "LaunchProcess called, but Cobalt does not support "
               << "multi-processing. This call will return a fake "
               << "Process object.";
  return Process();
}

Process LaunchProcess(const std::vector<std::string>& argv,
                      const LaunchOptions& options) {
  NOTREACHED() << "LaunchProcess called, but Cobalt does not support "
               << "multi-processing. This call will return a fake "
               << "Process object.";
  return Process();
}

pid_t ForkWithFlags(int flags, pid_t* ptid, pid_t* ctid) {
  return 0;
}

bool GetAppOutputWithExitCode(const CommandLine& cl,
                              std::string* output,
                              int* exit_code) {
  NOTREACHED()
      << "GetAppOutputWithExitCode called, but Cobalt does not support "
      << "multi-processing. This call will return a fake "
      << "Process object.";
  return false;
}
}  // namespace base
