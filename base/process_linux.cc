// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process.h"

#include <errno.h>
#include <sys/resource.h>

#include "base/logging.h"

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
