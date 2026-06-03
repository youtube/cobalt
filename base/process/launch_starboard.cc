// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/launch.h"

#include "base/notreached.h"

namespace base {

void CheckPThreadStackMinIsSafe() {
  NOTIMPLEMENTED();
}

Process LaunchProcess(const CommandLine& cmdline,
                      const LaunchOptions& options) {
  NOTREACHED() << "LaunchProcess called, but Cobalt does not support "
               << "multi-processing. This call will fail.";
}

Process LaunchProcess(const std::vector<std::string>& argv,
                      const LaunchOptions& options) {
  NOTREACHED() << "LaunchProcess called, but Cobalt does not support "
               << "multi-processing. This call will fail.";
}

void RaiseProcessToHighPriority() {
  NOTIMPLEMENTED();
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  NOTREACHED() << "GetAppOutput called, but Cobalt does not support "
               << "multi-processing. This call will fail.";
}

bool GetAppOutputAndError(const CommandLine& cl, std::string* output) {
  NOTREACHED() << "GetAppOutputAndError called, but Cobalt does not support "
               << "multi-processing. This call will fail.";
}

bool GetAppOutputWithExitCode(const CommandLine& cl,
                              std::string* output,
                              int* exit_code) {
  NOTREACHED() << "GetAppOutputWithExitCode called, but Cobalt does not "
               << "support multi-processing. This call will fail.";
}

pid_t ForkWithFlags(int flags, pid_t* ptid, pid_t* ctid) {
  return 0;
}

}  // namespace base
