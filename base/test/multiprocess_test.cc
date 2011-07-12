// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/multiprocess_test.h"

#include "base/base_switches.h"
#include "base/command_line.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <unistd.h>
#endif

namespace base {

MultiProcessTest::MultiProcessTest() {
}

ProcessHandle MultiProcessTest::SpawnChild(const std::string& procname,
                                           bool debug_on_start) {
  file_handle_mapping_vector empty_file_list;
  return SpawnChildImpl(procname, empty_file_list, debug_on_start);
}

#if defined(OS_POSIX)
ProcessHandle MultiProcessTest::SpawnChild(
    const std::string& procname,
    const file_handle_mapping_vector& fds_to_map,
    bool debug_on_start) {
  return SpawnChildImpl(procname, fds_to_map, debug_on_start);
}
#endif

CommandLine MultiProcessTest::MakeCmdLine(const std::string& procname,
                                          bool debug_on_start) {
  CommandLine cl(*CommandLine::ForCurrentProcess());
  cl.AppendSwitchASCII(switches::kTestChildProcess, procname);
  if (debug_on_start)
    cl.AppendSwitch(switches::kDebugOnStart);
  return cl;
}

ProcessHandle MultiProcessTest::SpawnChildImpl(
    const std::string& procname,
    const file_handle_mapping_vector& fds_to_map,
    bool debug_on_start) {
  ProcessHandle handle = kNullProcessHandle;
  base::LaunchOptions options;
  options.process_handle = &handle;
#if defined(OS_WIN)
  options.start_hidden = true;
#else
  options.fds_to_remap = &fds_to_map;
#endif
  base::LaunchProcess(MakeCmdLine(procname, debug_on_start), options);
  return handle;
}

}  // namespace base
