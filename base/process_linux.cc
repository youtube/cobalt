// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process.h"

#include <errno.h>
#include <sys/resource.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"

#if defined(OS_CHROMEOS)
static bool use_cgroups = false;
static bool cgroups_inited = false;
static const char kForegroundTasks[] =
    "/tmp/cgroup/cpu/chrome_renderers/foreground/tasks";
static const char kBackgroundTasks[] =
    "/tmp/cgroup/cpu/chrome_renderers/background/tasks";
static FilePath foreground_tasks;
static FilePath background_tasks;
#endif

namespace base {

#if defined(OS_CHROMEOS)
// We are more aggressive in our lowering of background process priority
// for chromeos as we have much more control over other processes running
// on the machine.
const int kPriorityAdjustment = 19;
#else
const int kPriorityAdjustment = 5;
#endif

bool Process::IsProcessBackgrounded() const {
  DCHECK(process_);
  return saved_priority_ == kUnsetProcessPriority;
}

bool Process::SetProcessBackgrounded(bool background) {
  DCHECK(process_);

#if defined(OS_CHROMEOS)
  // Check for cgroups files. ChromeOS supports these by default. It creates
  // a cgroup mount in /tmp/cgroup and then configures two cpu task groups,
  // one contains at most a single foreground renderer and the other contains
  // all background renderers. This allows us to limit the impact of background
  // renderers on foreground ones to a greater level than simple renicing.
  if (!cgroups_inited) {
    cgroups_inited = true;
    foreground_tasks = FilePath(kForegroundTasks);
    background_tasks = FilePath(kBackgroundTasks);
    file_util::FileSystemType foreground_type;
    file_util::FileSystemType background_type;
    use_cgroups =
        file_util::GetFileSystemType(foreground_tasks, &foreground_type) &&
        file_util::GetFileSystemType(background_tasks, &background_type) &&
        foreground_type == file_util::FILE_SYSTEM_CGROUP &&
        background_type == file_util::FILE_SYSTEM_CGROUP;
  }

  if (use_cgroups) {
    if (background) {
      std::string pid = StringPrintf("%d", process_);
      if (file_util::WriteFile(background_tasks, pid.c_str(), pid.size()) > 0) {
        // With cgroups there's no real notion of priority as an int, but this
        // will ensure we only move renderers back to the foreground group
        // if we've ever put them in the background one.
        saved_priority_ = 0;
        return true;
      } else {
        return false;
      }
    } else {
      if (saved_priority_ == kUnsetProcessPriority) {
        // Can't restore if we were never backgrounded.
        return false;
      }
      std::string pid = StringPrintf("%d", process_);
      if (file_util::WriteFile(foreground_tasks, pid.c_str(), pid.size()) > 0) {
        saved_priority_ = kUnsetProcessPriority;
        return true;
      } else {
        return false;
      }
    }
  }
#endif // OS_CHROMEOS

  if (background) {
    // We won't be able to raise the priority if we don't have the right rlimit.
    // The limit may be adjusted in /etc/security/limits.conf for PAM systems.
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NICE, &rlim) != 0) {
      // Call to getrlimit failed, don't background.
      return false;
    }
    errno = 0;
    int current_priority = GetPriority();
    if (errno) {
      // Couldn't get priority.
      return false;
    }
    // {set,get}priority values are in the range -20 to 19, where -1 is higher
    // priority than 0. But rlimit's are in the range from 0 to 39 where
    // 1 is higher than 0.
    if ((20 - current_priority) > static_cast<int>(rlim.rlim_cur)) {
      // User is not allowed to raise the priority back to where it is now.
      return false;
    }
    int result =
        setpriority(
            PRIO_PROCESS, process_, current_priority + kPriorityAdjustment);
    if (result == -1) {
      LOG(ERROR) << "Failed to lower priority, errno: " << errno;
      return false;
    }
    saved_priority_ = current_priority;
    return true;
  } else {
    if (saved_priority_ == kUnsetProcessPriority) {
      // Can't restore if we were never backgrounded.
      return false;
    }
    int result = setpriority(PRIO_PROCESS, process_, saved_priority_);
    // If we can't restore something has gone terribly wrong.
    DPCHECK(result == 0);
    saved_priority_ = kUnsetProcessPriority;
    return true;
  }
}

}  // namespace base
