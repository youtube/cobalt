// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/process_util.h"

#import <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <spawn.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"

namespace base {

NamedProcessIterator::NamedProcessIterator(const std::wstring& executable_name,
                                           const ProcessFilter* filter)
  : executable_name_(executable_name),
    index_of_kinfo_proc_(0),
    filter_(filter) {
  // Get a snapshot of all of my processes (yes, as we loop it can go stale, but
  // but trying to find where we were in a constantly changing list is basically
  // impossible.

  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_UID, geteuid() };

  // Since more processes could start between when we get the size and when
  // we get the list, we do a loop to keep trying until we get it.
  bool done = false;
  int try_num = 1;
  const int max_tries = 10;
  do {
    // Get the size of the buffer
    size_t len = 0;
    if (sysctl(mib, arraysize(mib), NULL, &len, NULL, 0) < 0) {
      LOG(ERROR) << "failed to get the size needed for the process list";
      kinfo_procs_.resize(0);
      done = true;
    } else {
      size_t num_of_kinfo_proc = len / sizeof(struct kinfo_proc);
      // Leave some spare room for process table growth (more could show up
      // between when we check and now)
      num_of_kinfo_proc += 4;
      kinfo_procs_.resize(num_of_kinfo_proc);
      len = num_of_kinfo_proc * sizeof(struct kinfo_proc);
      // Load the list of processes
      if (sysctl(mib, arraysize(mib), &kinfo_procs_[0], &len, NULL, 0) < 0) {
        // If we get a mem error, it just means we need a bigger buffer, so
        // loop around again.  Anything else is a real error and give up.
        if (errno != ENOMEM) {
          LOG(ERROR) << "failed to get the process list";
          kinfo_procs_.resize(0);
          done = true;
        }
      } else {
        // Got the list, just make sure we're sized exactly right
        size_t num_of_kinfo_proc = len / sizeof(struct kinfo_proc);
        kinfo_procs_.resize(num_of_kinfo_proc);
        done = true;
      }
    }
  } while (!done && (try_num++ < max_tries));

  if (!done) {
    LOG(ERROR) << "failed to collect the process list in a few tries";
    kinfo_procs_.resize(0);
  }
}

NamedProcessIterator::~NamedProcessIterator() {
}

const ProcessEntry* NamedProcessIterator::NextProcessEntry() {
  bool result = false;
  do {
    result = CheckForNextProcess();
  } while (result && !IncludeEntry());

  if (result) {
    return &entry_;
  }

  return NULL;
}

bool NamedProcessIterator::CheckForNextProcess() {
  std::string executable_name_utf8(WideToUTF8(executable_name_));

  std::string data;
  std::string exec_name;

  for (; index_of_kinfo_proc_ < kinfo_procs_.size(); ++index_of_kinfo_proc_) {
    kinfo_proc* kinfo = &kinfo_procs_[index_of_kinfo_proc_];

    // Skip processes just awaiting collection
    if ((kinfo->kp_proc.p_pid > 0) && (kinfo->kp_proc.p_stat == SZOMB))
      continue;

    int mib[] = { CTL_KERN, KERN_PROCARGS, kinfo->kp_proc.p_pid };

    // Found out what size buffer we need
    size_t data_len = 0;
    if (sysctl(mib, arraysize(mib), NULL, &data_len, NULL, 0) < 0) {
      LOG(ERROR) << "failed to figure out the buffer size for a commandline";
      continue;
    }

    data.resize(data_len);
    if (sysctl(mib, arraysize(mib), &data[0], &data_len, NULL, 0) < 0) {
      LOG(ERROR) << "failed to fetch a commandline";
      continue;
    }

    // Data starts w/ the full path null termed, so we have to extract just the
    // executable name from the path.

    size_t exec_name_end = data.find('\0');
    if (exec_name_end == std::string::npos) {
      LOG(ERROR) << "command line data didn't match expected format";
      continue;
    }
    size_t last_slash = data.rfind('/', exec_name_end);
    if (last_slash == std::string::npos)
      exec_name = data.substr(0, exec_name_end);
    else
      exec_name = data.substr(last_slash + 1, exec_name_end - last_slash - 1);

    // Check the name
    if (executable_name_utf8 == exec_name) {
      entry_.pid = kinfo->kp_proc.p_pid;
      entry_.ppid = kinfo->kp_eproc.e_ppid;
      base::strlcpy(entry_.szExeFile, exec_name.c_str(),
                    sizeof(entry_.szExeFile));
      // Start w/ the next entry next time through
      ++index_of_kinfo_proc_;
      // Done
      return true;
    }
  }
  return false;
}

bool NamedProcessIterator::IncludeEntry() {
  // Don't need to check the name, we did that w/in CheckForNextProcess.
  if (!filter_)
    return true;
  return filter_->Includes(entry_.pid, entry_.ppid);
}


// ------------------------------------------------------------------------
// NOTE: about ProcessMetrics
//
// Mac doesn't have /proc, and getting a mach task from a pid for another
// process requires permissions, so there doesn't really seem to be a way
// to do these (and spinning up ps to fetch each stats seems dangerous to
// put in a base api for anyone to call.
//
bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  return false;
}
size_t ProcessMetrics::GetPagefileUsage() const {
  return 0;
}
size_t ProcessMetrics::GetPeakPagefileUsage() const {
  return 0;
}
size_t ProcessMetrics::GetWorkingSetSize() const {
  return 0;
}
size_t ProcessMetrics::GetPeakWorkingSetSize() const {
  return 0;
}

// ------------------------------------------------------------------------

}  // namespace base
