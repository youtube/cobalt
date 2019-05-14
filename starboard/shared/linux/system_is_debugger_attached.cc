// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/system.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/common/log.h"

#include "starboard/shared/posix/file_internal.h"
#include "starboard/shared/posix/handle_eintr.h"

// Adapted from base/debug/debugger_posix.cc
bool SbSystemIsDebuggerAttached() {
  // We can look in /proc/self/status for TracerPid.  We are likely used in
  // crash handling, so we are careful not to use the heap or have side effects.
  // Another option that is common is to try to ptrace yourself, but then we
  // can't detach without forking(), and that's not so great.

  // NOTE: This code MUST be async-signal safe (it's used by in-process stack
  // dumping signal handler). NO malloc or stdio is allowed here.

  int status_fd = open("/proc/self/status", O_RDONLY);
  if (status_fd == -1)
    return false;

  // We assume our line will be in the first 1024 characters and that we can
  // read this much all at once.  In practice this will generally be true.
  // This simplifies and speeds up things considerably.
  char buf[1024];

  ssize_t num_read = HANDLE_EINTR(read(status_fd, buf, sizeof(buf) - 1));
  SB_DCHECK(num_read < sizeof(buf));
  if (HANDLE_EINTR(close(status_fd)) < 0)
    return false;

  if (num_read <= 0)
    return false;

  buf[num_read] = '\0';
  const char tracer[] = "TracerPid:\t";
  char* pid_index = strstr(buf, tracer);
  if (pid_index == NULL)
    return false;

  // Our pid is 0 without a debugger, assume this for any pid starting with 0.
  pid_index += strlen(tracer);
  return pid_index < (buf + num_read) && *pid_index != '0';
}
