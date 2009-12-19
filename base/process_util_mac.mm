// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/process_util.h"

#import <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include <malloc/malloc.h>
#include <spawn.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string>

#include "base/debug_util.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"

namespace base {

void RestoreDefaultExceptionHandler() {
  // This function is tailored to remove the Breakpad exception handler.
  // exception_mask matches s_exception_mask in
  // breakpad/src/client/mac/handler/exception_handler.cc
  const exception_mask_t exception_mask = EXC_MASK_BAD_ACCESS |
                                          EXC_MASK_BAD_INSTRUCTION |
                                          EXC_MASK_ARITHMETIC |
                                          EXC_MASK_BREAKPOINT;

  // Setting the exception port to MACH_PORT_NULL may not be entirely
  // kosher to restore the default exception handler, but in practice,
  // it results in the exception port being set to Apple Crash Reporter,
  // the desired behavior.
  task_set_exception_ports(mach_task_self(), exception_mask, MACH_PORT_NULL,
                           EXCEPTION_DEFAULT, THREAD_STATE_NONE);
}

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
  std::string executable_name_utf8(base::SysWideToUTF8(executable_name_));

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

size_t ProcessMetrics::GetPrivateBytes() const {
  return 0;
}

void ProcessMetrics::GetCommittedKBytes(CommittedKBytes* usage) const {
}

bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const {
  return false;
}

// ------------------------------------------------------------------------

// Bytes committed by the system.
size_t GetSystemCommitCharge() {
  host_name_port_t host = mach_host_self();
  mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
  vm_statistics_data_t data;
  kern_return_t kr = host_statistics(host, HOST_VM_INFO,
                                     reinterpret_cast<host_info_t>(&data),
                                     &count);
  if (kr) {
    LOG(WARNING) << "Failed to fetch host statistics.";
    return 0;
  }

  vm_size_t page_size;
  kr = host_page_size(host, &page_size);
  if (kr) {
    LOG(ERROR) << "Failed to fetch host page size.";
    return 0;
  }

  return (data.active_count * page_size) / 1024;
}

// ------------------------------------------------------------------------

namespace {

typedef void* (*malloc_type)(struct _malloc_zone_t* zone,
                             size_t size);
typedef void* (*calloc_type)(struct _malloc_zone_t* zone,
                             size_t num_items,
                             size_t size);
typedef void* (*valloc_type)(struct _malloc_zone_t* zone,
                             size_t size);
typedef void* (*realloc_type)(struct _malloc_zone_t* zone,
                              void* ptr,
                              size_t size);

malloc_type g_old_malloc;
calloc_type g_old_calloc;
valloc_type g_old_valloc;
realloc_type g_old_realloc;

void* oom_killer_malloc(struct _malloc_zone_t* zone,
                        size_t size) {
  void* result = g_old_malloc(zone, size);
  if (size && !result)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_calloc(struct _malloc_zone_t* zone,
                        size_t num_items,
                        size_t size) {
  void* result = g_old_calloc(zone, num_items, size);
  if (num_items && size && !result)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_valloc(struct _malloc_zone_t* zone,
                        size_t size) {
  void* result = g_old_valloc(zone, size);
  if (size && !result)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_realloc(struct _malloc_zone_t* zone,
                         void* ptr,
                         size_t size) {
  void* result = g_old_realloc(zone, ptr, size);
  if (size && !result)
    DebugUtil::BreakDebugger();
  return result;
}

}  // namespace

void EnableTerminationOnOutOfMemory() {
  CHECK(!g_old_malloc && !g_old_calloc && !g_old_valloc && !g_old_realloc)
      << "EnableTerminationOnOutOfMemory() called twice!";

  // This approach is sub-optimal:
  // - Requests for amounts of memory larger than MALLOC_ABSOLUTE_MAX_SIZE
  //   (currently SIZE_T_MAX - (2 * PAGE_SIZE)) will still fail with a NULL
  //   rather than dying (see
  //   http://opensource.apple.com/source/Libc/Libc-583/gen/malloc.c for
  //   details).
  // - It is unclear whether allocations via the C++ operator new() are affected
  //   by this (although it is likely).
  // - This does not affect allocations from non-default zones.
  // - It is unclear whether allocations from CoreFoundation's
  //   kCFAllocatorDefault or +[NSObject alloc] are affected by this.
  // Nevertheless this is better than nothing for now.
  // TODO(avi):Do better. http://crbug.com/12673

  malloc_zone_t* default_zone = malloc_default_zone();
  g_old_malloc = default_zone->malloc;
  g_old_calloc = default_zone->calloc;
  g_old_valloc = default_zone->valloc;
  g_old_realloc = default_zone->realloc;
  CHECK(g_old_malloc && g_old_calloc && g_old_valloc && g_old_realloc)
      << "Failed to get system allocation functions.";

  default_zone->malloc = oom_killer_malloc;
  default_zone->calloc = oom_killer_calloc;
  default_zone->valloc = oom_killer_valloc;
  default_zone->realloc = oom_killer_realloc;
}

}  // namespace base
