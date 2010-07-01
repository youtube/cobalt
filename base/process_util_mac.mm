// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/process_util.h"

#import <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include <malloc/malloc.h>
#import <objc/runtime.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <new>
#include <string>

#include "base/debug_util.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/sys_info.h"
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

ProcessIterator::ProcessIterator(const ProcessFilter* filter)
    : index_of_kinfo_proc_(0),
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
      num_of_kinfo_proc += 16;
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

ProcessIterator::~ProcessIterator() {
}

bool ProcessIterator::CheckForNextProcess() {
  std::string data;
  for (; index_of_kinfo_proc_ < kinfo_procs_.size(); ++index_of_kinfo_proc_) {
    kinfo_proc& kinfo = kinfo_procs_[index_of_kinfo_proc_];

    // Skip processes just awaiting collection
    if ((kinfo.kp_proc.p_pid > 0) && (kinfo.kp_proc.p_stat == SZOMB))
      continue;

    int mib[] = { CTL_KERN, KERN_PROCARGS, kinfo.kp_proc.p_pid };

    // Find out what size buffer we need.
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
    entry_.pid_ = kinfo.kp_proc.p_pid;
    entry_.ppid_ = kinfo.kp_eproc.e_ppid;
    entry_.gid_ = kinfo.kp_eproc.e_pgid;
    size_t last_slash = data.rfind('/', exec_name_end);
    if (last_slash == std::string::npos)
      entry_.exe_file_.assign(data, 0, exec_name_end);
    else
      entry_.exe_file_.assign(data, last_slash + 1,
                              exec_name_end - last_slash - 1);
    // Start w/ the next entry next time through
    ++index_of_kinfo_proc_;
    // Done
    return true;
  }
  return false;
}

bool NamedProcessIterator::IncludeEntry() {
  return (base::SysWideToUTF8(executable_name_) == entry().exe_file() &&
          ProcessIterator::IncludeEntry());
}


// ------------------------------------------------------------------------
// NOTE: about ProcessMetrics
//
// Getting a mach task from a pid for another process requires permissions in
// general, so there doesn't really seem to be a way to do these (and spinning
// up ps to fetch each stats seems dangerous to put in a base api for anyone to
// call). Child processes ipc their port, so return something if available,
// otherwise return 0.
//

ProcessMetrics::ProcessMetrics(ProcessHandle process,
                               ProcessMetrics::PortProvider* port_provider)
    : process_(process),
      last_time_(0),
      last_system_time_(0),
      port_provider_(port_provider) {
  processor_count_ = base::SysInfo::NumberOfProcessors();
}

// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process,
    ProcessMetrics::PortProvider* port_provider) {
  return new ProcessMetrics(process, port_provider);
}

bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  return false;
}

static bool GetTaskInfo(mach_port_t task, task_basic_info_64* task_info_data) {
  if (task == MACH_PORT_NULL)
    return false;
  mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;
  kern_return_t kr = task_info(task,
                               TASK_BASIC_INFO_64,
                               reinterpret_cast<task_info_t>(task_info_data),
                               &count);
  // Most likely cause for failure: |task| is a zombie.
  return kr == KERN_SUCCESS;
}

size_t ProcessMetrics::GetPagefileUsage() const {
  task_basic_info_64 task_info_data;
  if (!GetTaskInfo(TaskForPid(process_), &task_info_data))
    return 0;
  return task_info_data.virtual_size;
}

size_t ProcessMetrics::GetPeakPagefileUsage() const {
  return 0;
}

size_t ProcessMetrics::GetWorkingSetSize() const {
  task_basic_info_64 task_info_data;
  if (!GetTaskInfo(TaskForPid(process_), &task_info_data))
    return 0;
  return task_info_data.resident_size;
}

size_t ProcessMetrics::GetPeakWorkingSetSize() const {
  return 0;
}

// OSX appears to use a different system to get its memory.
bool ProcessMetrics::GetMemoryBytes(size_t* private_bytes,
                                    size_t* shared_bytes) {
  if (private_bytes)
    *private_bytes = 0;
  if (shared_bytes)
    *shared_bytes = 0;
  return true;
}

void ProcessMetrics::GetCommittedKBytes(CommittedKBytes* usage) const {
}

bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const {
  size_t priv = GetWorkingSetSize();
  if (!priv)
    return false;
  ws_usage->priv = priv / 1024;
  ws_usage->shareable = 0;
  ws_usage->shared = 0;
  return true;
}

#define TIME_VALUE_TO_TIMEVAL(a, r) do {  \
  (r)->tv_sec = (a)->seconds;             \
  (r)->tv_usec = (a)->microseconds;       \
} while (0)

double ProcessMetrics::GetCPUUsage() {
  mach_port_t task = TaskForPid(process_);
  if (task == MACH_PORT_NULL)
    return 0;

  kern_return_t kr;

  // Libtop explicitly loops over the threads (libtop_pinfo_update_cpu_usage()
  // in libtop.c), but this is more concise and gives the same results:
  task_thread_times_info thread_info_data;
  mach_msg_type_number_t thread_info_count = TASK_THREAD_TIMES_INFO_COUNT;
  kr = task_info(task,
                 TASK_THREAD_TIMES_INFO,
                 reinterpret_cast<task_info_t>(&thread_info_data),
                 &thread_info_count);
  if (kr != KERN_SUCCESS) {
    // Most likely cause: |task| is a zombie.
    return 0;
  }

  task_basic_info_64 task_info_data;
  if (!GetTaskInfo(task, &task_info_data))
    return 0;

  /* Set total_time. */
  // thread info contains live time...
  struct timeval user_timeval, system_timeval, task_timeval;
  TIME_VALUE_TO_TIMEVAL(&thread_info_data.user_time, &user_timeval);
  TIME_VALUE_TO_TIMEVAL(&thread_info_data.system_time, &system_timeval);
  timeradd(&user_timeval, &system_timeval, &task_timeval);

  // ... task info contains terminated time.
  TIME_VALUE_TO_TIMEVAL(&task_info_data.user_time, &user_timeval);
  TIME_VALUE_TO_TIMEVAL(&task_info_data.system_time, &system_timeval);
  timeradd(&user_timeval, &task_timeval, &task_timeval);
  timeradd(&system_timeval, &task_timeval, &task_timeval);

  struct timeval now;
  int retval = gettimeofday(&now, NULL);
  if (retval)
    return 0;

  int64 time = TimeValToMicroseconds(now);
  int64 task_time = TimeValToMicroseconds(task_timeval);

  if ((last_system_time_ == 0) || (last_time_ == 0)) {
    // First call, just set the last values.
    last_system_time_ = task_time;
    last_time_ = time;
    return 0;
  }

  int64 system_time_delta = task_time - last_system_time_;
  int64 time_delta = time - last_time_;
  DCHECK(time_delta != 0);
  if (time_delta == 0)
    return 0;

  // We add time_delta / 2 so the result is rounded.
  double cpu = static_cast<double>((system_time_delta * 100.0) / time_delta);

  last_system_time_ = task_time;
  last_time_ = time;

  return cpu;
}

mach_port_t ProcessMetrics::TaskForPid(ProcessHandle process) const {
  mach_port_t task = MACH_PORT_NULL;
  if (port_provider_)
    task = port_provider_->TaskForPid(process_);
  if (task == MACH_PORT_NULL && process_ == getpid())
    task = mach_task_self();
  return task;
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

bool g_oom_killer_enabled;

// === C malloc/calloc/valloc/realloc/posix_memalign ===

// The extended version of malloc_zone_t from the 10.6 SDK's <malloc/malloc.h>,
// included here to allow for compilation in 10.5. (10.5 has version 3 zone
// allocators, while 10.6 has version 6 allocators.)
struct ChromeMallocZone {
  void* reserved1;
  void* reserved2;
  size_t (*size)(struct _malloc_zone_t* zone, const void* ptr);
  void* (*malloc)(struct _malloc_zone_t* zone, size_t size);
  void* (*calloc)(struct _malloc_zone_t* zone, size_t num_items, size_t size);
  void* (*valloc)(struct _malloc_zone_t* zone, size_t size);
  void (*free)(struct _malloc_zone_t* zone, void* ptr);
  void* (*realloc)(struct _malloc_zone_t* zone, void* ptr, size_t size);
  void (*destroy)(struct _malloc_zone_t* zone);
  const char* zone_name;
  unsigned (*batch_malloc)(struct _malloc_zone_t* zone, size_t size,
                           void** results, unsigned num_requested);
  void (*batch_free)(struct _malloc_zone_t* zone, void** to_be_freed,
                     unsigned num_to_be_freed);
  struct malloc_introspection_t* introspect;
  unsigned version;
  void* (*memalign)(struct _malloc_zone_t* zone, size_t alignment,
                    size_t size);  // version >= 5
  void (*free_definite_size)(struct _malloc_zone_t* zone, void* ptr,
                             size_t size);  // version >= 6
};

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
typedef void* (*memalign_type)(struct _malloc_zone_t* zone,
                               size_t alignment,
                               size_t size);

malloc_type g_old_malloc;
calloc_type g_old_calloc;
valloc_type g_old_valloc;
realloc_type g_old_realloc;
memalign_type g_old_memalign;

malloc_type g_old_malloc_purgeable;
calloc_type g_old_calloc_purgeable;
valloc_type g_old_valloc_purgeable;
realloc_type g_old_realloc_purgeable;
memalign_type g_old_memalign_purgeable;

void* oom_killer_malloc(struct _malloc_zone_t* zone,
                        size_t size) {
  void* result = g_old_malloc(zone, size);
  if (!result && size)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_calloc(struct _malloc_zone_t* zone,
                        size_t num_items,
                        size_t size) {
  void* result = g_old_calloc(zone, num_items, size);
  if (!result && num_items && size)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_valloc(struct _malloc_zone_t* zone,
                        size_t size) {
  void* result = g_old_valloc(zone, size);
  if (!result && size)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_realloc(struct _malloc_zone_t* zone,
                         void* ptr,
                         size_t size) {
  void* result = g_old_realloc(zone, ptr, size);
  if (!result && size)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_memalign(struct _malloc_zone_t* zone,
                          size_t alignment,
                          size_t size) {
  void* result = g_old_memalign(zone, alignment, size);
  // Only die if posix_memalign would have returned ENOMEM, since there are
  // other reasons why NULL might be returned (see
  // http://opensource.apple.com/source/Libc/Libc-583/gen/malloc.c ).
  if (!result && size && alignment >= sizeof(void*)
      && (alignment & (alignment - 1)) == 0) {
    DebugUtil::BreakDebugger();
  }
  return result;
}

void* oom_killer_malloc_purgeable(struct _malloc_zone_t* zone,
                                  size_t size) {
  void* result = g_old_malloc_purgeable(zone, size);
  if (!result && size)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_calloc_purgeable(struct _malloc_zone_t* zone,
                                  size_t num_items,
                                  size_t size) {
  void* result = g_old_calloc_purgeable(zone, num_items, size);
  if (!result && num_items && size)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_valloc_purgeable(struct _malloc_zone_t* zone,
                                  size_t size) {
  void* result = g_old_valloc_purgeable(zone, size);
  if (!result && size)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_realloc_purgeable(struct _malloc_zone_t* zone,
                                   void* ptr,
                                   size_t size) {
  void* result = g_old_realloc_purgeable(zone, ptr, size);
  if (!result && size)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_memalign_purgeable(struct _malloc_zone_t* zone,
                                    size_t alignment,
                                    size_t size) {
  void* result = g_old_memalign_purgeable(zone, alignment, size);
  // Only die if posix_memalign would have returned ENOMEM, since there are
  // other reasons why NULL might be returned (see
  // http://opensource.apple.com/source/Libc/Libc-583/gen/malloc.c ).
  if (!result && size && alignment >= sizeof(void*)
      && (alignment & (alignment - 1)) == 0) {
    DebugUtil::BreakDebugger();
  }
  return result;
}

// === C++ operator new ===

void oom_killer_new() {
  DebugUtil::BreakDebugger();
}

// === Core Foundation CFAllocators ===

// This is the real structure of a CFAllocatorRef behind the scenes. See
// http://opensource.apple.com/source/CF/CF-476.19/CFBase.c (10.5.8) and
// http://opensource.apple.com/source/CF/CF-550/CFBase.c (10.6) for details.
struct ChromeCFRuntimeBase {
  uintptr_t _cfisa;
  uint8_t _cfinfo[4];
#if __LP64__
  uint32_t _rc;
#endif
};

struct ChromeCFAllocator {
  ChromeCFRuntimeBase cf_runtime_base;
  size_t (*size)(struct _malloc_zone_t* zone, const void* ptr);
  void* (*malloc)(struct _malloc_zone_t* zone, size_t size);
  void* (*calloc)(struct _malloc_zone_t* zone, size_t num_items, size_t size);
  void* (*valloc)(struct _malloc_zone_t* zone, size_t size);
  void (*free)(struct _malloc_zone_t* zone, void* ptr);
  void* (*realloc)(struct _malloc_zone_t* zone, void* ptr, size_t size);
  void (*destroy)(struct _malloc_zone_t* zone);
  const char* zone_name;
  unsigned (*batch_malloc)(struct _malloc_zone_t* zone, size_t size,
                           void** results, unsigned num_requested);
  void (*batch_free)(struct _malloc_zone_t* zone, void** to_be_freed,
                     unsigned num_to_be_freed);
  struct malloc_introspection_t* introspect;
  void* reserved5;

  void* allocator;
  CFAllocatorContext context;
};
typedef ChromeCFAllocator* ChromeCFAllocatorRef;

CFAllocatorAllocateCallBack g_old_cfallocator_system_default;
CFAllocatorAllocateCallBack g_old_cfallocator_malloc;
CFAllocatorAllocateCallBack g_old_cfallocator_malloc_zone;

void* oom_killer_cfallocator_system_default(CFIndex alloc_size,
                                            CFOptionFlags hint,
                                            void* info) {
  void* result = g_old_cfallocator_system_default(alloc_size, hint, info);
  if (!result)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_cfallocator_malloc(CFIndex alloc_size,
                                    CFOptionFlags hint,
                                    void* info) {
  void* result = g_old_cfallocator_malloc(alloc_size, hint, info);
  if (!result)
    DebugUtil::BreakDebugger();
  return result;
}

void* oom_killer_cfallocator_malloc_zone(CFIndex alloc_size,
                                         CFOptionFlags hint,
                                         void* info) {
  void* result = g_old_cfallocator_malloc_zone(alloc_size, hint, info);
  if (!result)
    DebugUtil::BreakDebugger();
  return result;
}

// === Cocoa NSObject allocation ===

typedef id (*allocWithZone_t)(id, SEL, NSZone*);
allocWithZone_t g_old_allocWithZone;

id oom_killer_allocWithZone(id self, SEL _cmd, NSZone* zone)
{
  id result = g_old_allocWithZone(self, _cmd, zone);
  if (!result)
    DebugUtil::BreakDebugger();
  return result;
}

}  // namespace

malloc_zone_t* GetPurgeableZone() {
  // malloc_default_purgeable_zone only exists on >= 10.6. Use dlsym to grab it
  // at runtime because it may not be present in the SDK used for compilation.
  typedef malloc_zone_t* (*malloc_default_purgeable_zone_t)(void);
  malloc_default_purgeable_zone_t malloc_purgeable_zone =
      reinterpret_cast<malloc_default_purgeable_zone_t>(
          dlsym(RTLD_DEFAULT, "malloc_default_purgeable_zone"));
  if (malloc_purgeable_zone)
    return malloc_purgeable_zone();
  return NULL;
}

void EnableTerminationOnOutOfMemory() {
  if (g_oom_killer_enabled)
    return;

  g_oom_killer_enabled = true;

  int32 os_major;
  int32 os_minor;
  int32 os_bugfix;
  SysInfo::OperatingSystemVersionNumbers(&os_major, &os_minor, &os_bugfix);

  // === C malloc/calloc/valloc/realloc/posix_memalign ===

  // This approach is not perfect, as requests for amounts of memory larger than
  // MALLOC_ABSOLUTE_MAX_SIZE (currently SIZE_T_MAX - (2 * PAGE_SIZE)) will
  // still fail with a NULL rather than dying (see
  // http://opensource.apple.com/source/Libc/Libc-583/gen/malloc.c for details).
  // Unfortunately, it's the best we can do. Also note that this does not affect
  // allocations from non-default zones.

  CHECK(!g_old_malloc && !g_old_calloc && !g_old_valloc && !g_old_realloc &&
        !g_old_memalign) << "Old allocators unexpectedly non-null";

  CHECK(!g_old_malloc_purgeable && !g_old_calloc_purgeable &&
        !g_old_valloc_purgeable && !g_old_realloc_purgeable &&
        !g_old_memalign_purgeable) << "Old allocators unexpectedly non-null";

  // See http://trac.webkit.org/changeset/53362/trunk/WebKitTools/DumpRenderTree/mac
  bool zone_allocators_protected =
      ((os_major == 10 && os_minor > 6) || os_major > 10);

  ChromeMallocZone* default_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  ChromeMallocZone* purgeable_zone =
      reinterpret_cast<ChromeMallocZone*>(GetPurgeableZone());

  vm_address_t page_start_default = NULL;
  vm_address_t page_start_purgeable = NULL;
  vm_size_t len_default = 0;
  vm_size_t len_purgeable = 0;
  if (zone_allocators_protected) {
    page_start_default = reinterpret_cast<vm_address_t>(default_zone) &
        static_cast<vm_size_t>(~(getpagesize() - 1));
    len_default = reinterpret_cast<vm_address_t>(default_zone) -
        page_start_default + sizeof(ChromeMallocZone);
    mprotect(reinterpret_cast<void*>(page_start_default), len_default,
             PROT_READ | PROT_WRITE);

    if (purgeable_zone) {
      page_start_purgeable = reinterpret_cast<vm_address_t>(purgeable_zone) &
          static_cast<vm_size_t>(~(getpagesize() - 1));
      len_purgeable = reinterpret_cast<vm_address_t>(purgeable_zone) -
          page_start_purgeable + sizeof(ChromeMallocZone);
      mprotect(reinterpret_cast<void*>(page_start_purgeable), len_purgeable,
               PROT_READ | PROT_WRITE);
    }
  }

  // Default zone

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

  if (default_zone->version >= 5) {
    g_old_memalign = default_zone->memalign;
    if (g_old_memalign)
      default_zone->memalign = oom_killer_memalign;
  }

  // Purgeable zone (if it exists)

  if (purgeable_zone) {
    g_old_malloc_purgeable = purgeable_zone->malloc;
    g_old_calloc_purgeable = purgeable_zone->calloc;
    g_old_valloc_purgeable = purgeable_zone->valloc;
    g_old_realloc_purgeable = purgeable_zone->realloc;
    CHECK(g_old_malloc_purgeable && g_old_calloc_purgeable &&
          g_old_valloc_purgeable && g_old_realloc_purgeable)
        << "Failed to get system allocation functions.";

    purgeable_zone->malloc = oom_killer_malloc_purgeable;
    purgeable_zone->calloc = oom_killer_calloc_purgeable;
    purgeable_zone->valloc = oom_killer_valloc_purgeable;
    purgeable_zone->realloc = oom_killer_realloc_purgeable;

    if (purgeable_zone->version >= 5) {
      g_old_memalign_purgeable = purgeable_zone->memalign;
      if (g_old_memalign_purgeable)
        purgeable_zone->memalign = oom_killer_memalign_purgeable;
    }
  }

  if (zone_allocators_protected) {
    mprotect(reinterpret_cast<void*>(page_start_default), len_default,
             PROT_READ);
    if (purgeable_zone) {
      mprotect(reinterpret_cast<void*>(page_start_purgeable), len_purgeable,
               PROT_READ);
    }
  }

  // === C malloc_zone_batch_malloc ===

  // batch_malloc is omitted because the default malloc zone's implementation
  // only supports batch_malloc for "tiny" allocations from the free list. It
  // will fail for allocations larger than "tiny", and will only allocate as
  // many blocks as it's able to from the free list. These factors mean that it
  // can return less than the requested memory even in a non-out-of-memory
  // situation. There's no good way to detect whether a batch_malloc failure is
  // due to these other factors, or due to genuine memory or address space
  // exhaustion. The fact that it only allocates space from the "tiny" free list
  // means that it's likely that a failure will not be due to memory exhaustion.
  // Similarly, these constraints on batch_malloc mean that callers must always
  // be expecting to receive less memory than was requested, even in situations
  // where memory pressure is not a concern. Finally, the only public interface
  // to batch_malloc is malloc_zone_batch_malloc, which is specific to the
  // system's malloc implementation. It's unlikely that anyone's even heard of
  // it.

  // === C++ operator new ===

  // Yes, operator new does call through to malloc, but this will catch failures
  // that our imperfect handling of malloc cannot.

  std::set_new_handler(oom_killer_new);

  // === Core Foundation CFAllocators ===

  // This will not catch allocation done by custom allocators, but will catch
  // all allocation done by system-provided ones.

  CHECK(!g_old_cfallocator_system_default && !g_old_cfallocator_malloc &&
        !g_old_cfallocator_malloc_zone)
      << "Old allocators unexpectedly non-null";

  bool cf_allocator_internals_known =
      (os_major == 10 && (os_minor == 5 || os_minor == 6));

  if (cf_allocator_internals_known) {
    ChromeCFAllocatorRef allocator = const_cast<ChromeCFAllocatorRef>(
        reinterpret_cast<const ChromeCFAllocator*>(kCFAllocatorSystemDefault));
    g_old_cfallocator_system_default = allocator->context.allocate;
    CHECK(g_old_cfallocator_system_default)
        << "Failed to get kCFAllocatorSystemDefault allocation function.";
    allocator->context.allocate = oom_killer_cfallocator_system_default;

    allocator = const_cast<ChromeCFAllocatorRef>(
        reinterpret_cast<const ChromeCFAllocator*>(kCFAllocatorMalloc));
    g_old_cfallocator_malloc = allocator->context.allocate;
    CHECK(g_old_cfallocator_malloc)
        << "Failed to get kCFAllocatorMalloc allocation function.";
    allocator->context.allocate = oom_killer_cfallocator_malloc;

    allocator = const_cast<ChromeCFAllocatorRef>(
        reinterpret_cast<const ChromeCFAllocator*>(kCFAllocatorMallocZone));
    g_old_cfallocator_malloc_zone = allocator->context.allocate;
    CHECK(g_old_cfallocator_malloc_zone)
        << "Failed to get kCFAllocatorMallocZone allocation function.";
    allocator->context.allocate = oom_killer_cfallocator_malloc_zone;
  } else {
    NSLog(@"Internals of CFAllocator not known; out-of-memory failures via "
        "CFAllocator will not result in termination. http://crbug.com/45650");
  }

  // === Cocoa NSObject allocation ===

  // Note that both +[NSObject new] and +[NSObject alloc] call through to
  // +[NSObject allocWithZone:].

  CHECK(!g_old_allocWithZone)
      << "Old allocator unexpectedly non-null";

  Class nsobject_class = [NSObject class];
  Method orig_method = class_getClassMethod(nsobject_class,
                                            @selector(allocWithZone:));
  g_old_allocWithZone = reinterpret_cast<allocWithZone_t>(
      method_getImplementation(orig_method));
  CHECK(g_old_allocWithZone)
      << "Failed to get allocWithZone allocation function.";
  method_setImplementation(orig_method,
                           reinterpret_cast<IMP>(oom_killer_allocWithZone));
}

}  // namespace base
