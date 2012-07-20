// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/multiprocess_test.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/process.h"
#include "testing/multiprocess_func_list.h"

namespace base {

// A very basic implementation for android. On Android tests can run in an APK
// and we don't have an executable to exec*. This implementation does the bare
// minimum to execute the method specified by procname (in the child process).
//  - File descriptors are not closed and hence |fds_to_map| is ignored.
//  - |debug_on_start| is ignored.
ProcessHandle MultiProcessTest::SpawnChildImpl(
    const std::string& procname,
    const FileHandleMappingVector& fds_to_map,
    bool debug_on_start) {
  pid_t pid = fork();

  if (pid < 0) {
    DPLOG(ERROR) << "fork";
    return kNullProcessHandle;
  } else if (pid == 0) {
    // Child process.
    _exit(multi_process_function_list::InvokeChildProcessTest(procname));
  }

  // Parent process.
  return pid;
}

}  // namespace base
