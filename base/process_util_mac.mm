// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#import <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <dlfcn.h>
#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <mach/task.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <malloc/malloc.h>
#import <objc/runtime.h>
#include <signal.h>
#include <spawn.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <new>
#include <string>

#include "base/debug/debugger.h"
#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "third_party/apple_apsl/CFBase.h"
#include "third_party/apple_apsl/malloc.h"
#include "third_party/mach_override/mach_override.h"

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
      DLOG(ERROR) << "failed to get the size needed for the process list";
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
          DLOG(ERROR) << "failed to get the process list";
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
    DLOG(ERROR) << "failed to collect the process list in a few tries";
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
      DVPLOG(1) << "failed to figure out the buffer size for a commandline";
      continue;
    }

    data.resize(data_len);
    if (sysctl(mib, arraysize(mib), &data[0], &data_len, NULL, 0) < 0) {
      DVPLOG(1) << "failed to fetch a commandline";
      continue;
    }

    // |data| contains all the command line parameters of the process, separated
    // by blocks of one or more null characters. We tokenize |data| into a
    // vector of strings using '\0' as a delimiter and populate
    // |entry_.cmd_line_args_|.
    std::string delimiters;
    delimiters.push_back('\0');
    Tokenize(data, delimiters, &entry_.cmd_line_args_);

    // |data| starts with the full executable path followed by a null character.
    // We search for the first instance of '\0' and extract everything before it
    // to populate |entry_.exe_file_|.
    size_t exec_name_end = data.find('\0');
    if (exec_name_end == std::string::npos) {
      DLOG(ERROR) << "command line data didn't match expected format";
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
  return (executable_name_ == entry().exe_file() &&
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
  processor_count_ = SysInfo::NumberOfProcessors();
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

static bool GetCPUTypeForProcess(pid_t pid, cpu_type_t* cpu_type) {
  size_t len = sizeof(*cpu_type);
  int result = sysctlbyname("sysctl.proc_cputype",
                            cpu_type,
                            &len,
                            NULL,
                            0);
  if (result != 0) {
    DPLOG(ERROR) << "sysctlbyname(""sysctl.proc_cputype"")";
    return false;
  }

  return true;
}

static bool IsAddressInSharedRegion(mach_vm_address_t addr, cpu_type_t type) {
  if (type == CPU_TYPE_I386)
    return addr >= SHARED_REGION_BASE_I386 &&
           addr < (SHARED_REGION_BASE_I386 + SHARED_REGION_SIZE_I386);
  else if (type == CPU_TYPE_X86_64)
    return addr >= SHARED_REGION_BASE_X86_64 &&
           addr < (SHARED_REGION_BASE_X86_64 + SHARED_REGION_SIZE_X86_64);
  else
    return false;
}

// This is a rough approximation of the algorithm that libtop uses.
// private_bytes is the size of private resident memory.
// shared_bytes is the size of shared resident memory.
bool ProcessMetrics::GetMemoryBytes(size_t* private_bytes,
                                    size_t* shared_bytes) {
  kern_return_t kr;
  size_t private_pages_count = 0;
  size_t shared_pages_count = 0;

  if (!private_bytes && !shared_bytes)
    return true;

  mach_port_t task = TaskForPid(process_);
  if (task == MACH_PORT_NULL) {
    DLOG(ERROR) << "Invalid process";
    return false;
  }

  cpu_type_t cpu_type;
  if (!GetCPUTypeForProcess(process_, &cpu_type))
    return false;

  // The same region can be referenced multiple times. To avoid double counting
  // we need to keep track of which regions we've already counted.
  base::hash_set<int> seen_objects;

  // We iterate through each VM region in the task's address map. For shared
  // memory we add up all the pages that are marked as shared. Like libtop we
  // try to avoid counting pages that are also referenced by other tasks. Since
  // we don't have access to the VM regions of other tasks the only hint we have
  // is if the address is in the shared region area.
  //
  // Private memory is much simpler. We simply count the pages that are marked
  // as private or copy on write (COW).
  //
  // See libtop_update_vm_regions in
  // http://www.opensource.apple.com/source/top/top-67/libtop.c
  mach_vm_size_t size = 0;
  for (mach_vm_address_t address = MACH_VM_MIN_ADDRESS;; address += size) {
    vm_region_top_info_data_t info;
    mach_msg_type_number_t info_count = VM_REGION_TOP_INFO_COUNT;
    mach_port_t object_name;
    kr = mach_vm_region(task,
                        &address,
                        &size,
                        VM_REGION_TOP_INFO,
                        (vm_region_info_t)&info,
                        &info_count,
                        &object_name);
    if (kr == KERN_INVALID_ADDRESS) {
      // We're at the end of the address space.
      break;
    } else if (kr != KERN_SUCCESS) {
      DLOG(ERROR) << "Calling mach_vm_region failed with error: "
                 << mach_error_string(kr);
      return false;
    }

    if (IsAddressInSharedRegion(address, cpu_type) &&
        info.share_mode != SM_PRIVATE)
      continue;

    if (info.share_mode == SM_COW && info.ref_count == 1)
      info.share_mode = SM_PRIVATE;

    switch (info.share_mode) {
      case SM_PRIVATE:
        private_pages_count += info.private_pages_resident;
        private_pages_count += info.shared_pages_resident;
        break;
      case SM_COW:
        private_pages_count += info.private_pages_resident;
        // Fall through
      case SM_SHARED:
        if (seen_objects.count(info.obj_id) == 0) {
          // Only count the first reference to this region.
          seen_objects.insert(info.obj_id);
          shared_pages_count += info.shared_pages_resident;
        }
        break;
      default:
        break;
    }
  }

  vm_size_t page_size;
  kr = host_page_size(task, &page_size);
  if (kr != KERN_SUCCESS) {
    DLOG(ERROR) << "Failed to fetch host page size, error: "
               << mach_error_string(kr);
    return false;
  }

  if (private_bytes)
    *private_bytes = private_pages_count * page_size;
  if (shared_bytes)
    *shared_bytes = shared_pages_count * page_size;

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
  DCHECK_NE(0U, time_delta);
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
    DLOG(WARNING) << "Failed to fetch host statistics.";
    return 0;
  }

  vm_size_t page_size;
  kr = host_page_size(host, &page_size);
  if (kr) {
    DLOG(ERROR) << "Failed to fetch host page size.";
    return 0;
  }

  return (data.active_count * page_size) / 1024;
}

namespace {

// Finds the library path for malloc() and thus the libC part of libSystem,
// which in Lion is in a separate image.
const char* LookUpLibCPath() {
  const void* addr = reinterpret_cast<void*>(&malloc);

  Dl_info info;
  if (dladdr(addr, &info))
    return info.dli_fname;

  DLOG(WARNING) << "Could not find image path for malloc()";
  return NULL;
}

typedef void(*malloc_error_break_t)(void);
malloc_error_break_t g_original_malloc_error_break = NULL;

// Returns the function pointer for malloc_error_break. This symbol is declared
// as __private_extern__ and cannot be dlsym()ed. Instead, use nlist() to
// get it.
malloc_error_break_t LookUpMallocErrorBreak() {
#if ARCH_CPU_32_BITS
  const char* lib_c_path = LookUpLibCPath();
  if (!lib_c_path)
    return NULL;

  // Only need to look up two symbols, but nlist() requires a NULL-terminated
  // array and takes no count.
  struct nlist nl[3];
  bzero(&nl, sizeof(nl));

  // The symbol to find.
  nl[0].n_un.n_name = const_cast<char*>("_malloc_error_break");

  // A reference symbol by which the address of the desired symbol will be
  // calculated.
  nl[1].n_un.n_name = const_cast<char*>("_malloc");

  int rv = nlist(lib_c_path, nl);
  if (rv != 0 || nl[0].n_type == N_UNDF || nl[1].n_type == N_UNDF) {
    return NULL;
  }

  // nlist() returns addresses as offsets in the image, not the instruction
  // pointer in memory. Use the known in-memory address of malloc()
  // to compute the offset for malloc_error_break().
  uintptr_t reference_addr = reinterpret_cast<uintptr_t>(&malloc);
  reference_addr -= nl[1].n_value;
  reference_addr += nl[0].n_value;

  return reinterpret_cast<malloc_error_break_t>(reference_addr);
#endif  // ARCH_CPU_32_BITS

  return NULL;
}

void CrMallocErrorBreak() {
  g_original_malloc_error_break();
  // A unit test checks this error message, so it needs to be in release builds.
  LOG(ERROR) <<
      "Terminating process due to a potential for future heap corruption";
  int* death_ptr = NULL;
  *death_ptr = 0xf00bad;
}

}  // namespace

void EnableTerminationOnHeapCorruption() {
#ifdef ADDRESS_SANITIZER
  // Don't do anything special on heap corruption, because it should be handled
  // by AddressSanitizer.
  return;
#endif
  malloc_error_break_t malloc_error_break = LookUpMallocErrorBreak();
  if (!malloc_error_break) {
    DLOG(WARNING) << "Could not find malloc_error_break";
    return;
  }

  mach_error_t err = mach_override_ptr(
     (void*)malloc_error_break,
     (void*)&CrMallocErrorBreak,
     (void**)&g_original_malloc_error_break);

  if (err != err_none)
    DLOG(WARNING) << "Could not override malloc_error_break; error = " << err;
}

// ------------------------------------------------------------------------

namespace {

bool g_oom_killer_enabled;

// === C malloc/calloc/valloc/realloc/posix_memalign ===

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
    debug::BreakDebugger();
  return result;
}

void* oom_killer_calloc(struct _malloc_zone_t* zone,
                        size_t num_items,
                        size_t size) {
  void* result = g_old_calloc(zone, num_items, size);
  if (!result && num_items && size)
    debug::BreakDebugger();
  return result;
}

void* oom_killer_valloc(struct _malloc_zone_t* zone,
                        size_t size) {
  void* result = g_old_valloc(zone, size);
  if (!result && size)
    debug::BreakDebugger();
  return result;
}

void* oom_killer_realloc(struct _malloc_zone_t* zone,
                         void* ptr,
                         size_t size) {
  void* result = g_old_realloc(zone, ptr, size);
  if (!result && size)
    debug::BreakDebugger();
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
    debug::BreakDebugger();
  }
  return result;
}

void* oom_killer_malloc_purgeable(struct _malloc_zone_t* zone,
                                  size_t size) {
  void* result = g_old_malloc_purgeable(zone, size);
  if (!result && size)
    debug::BreakDebugger();
  return result;
}

void* oom_killer_calloc_purgeable(struct _malloc_zone_t* zone,
                                  size_t num_items,
                                  size_t size) {
  void* result = g_old_calloc_purgeable(zone, num_items, size);
  if (!result && num_items && size)
    debug::BreakDebugger();
  return result;
}

void* oom_killer_valloc_purgeable(struct _malloc_zone_t* zone,
                                  size_t size) {
  void* result = g_old_valloc_purgeable(zone, size);
  if (!result && size)
    debug::BreakDebugger();
  return result;
}

void* oom_killer_realloc_purgeable(struct _malloc_zone_t* zone,
                                   void* ptr,
                                   size_t size) {
  void* result = g_old_realloc_purgeable(zone, ptr, size);
  if (!result && size)
    debug::BreakDebugger();
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
    debug::BreakDebugger();
  }
  return result;
}

// === C++ operator new ===

void oom_killer_new() {
  debug::BreakDebugger();
}

// === Core Foundation CFAllocators ===

bool CanGetContextForCFAllocator() {
  return !base::mac::IsOSLaterThanLion();
}

CFAllocatorContext* ContextForCFAllocator(CFAllocatorRef allocator) {
  if (base::mac::IsOSLeopard() || base::mac::IsOSSnowLeopard()) {
    ChromeCFAllocatorLeopards* our_allocator =
        const_cast<ChromeCFAllocatorLeopards*>(
            reinterpret_cast<const ChromeCFAllocatorLeopards*>(allocator));
    return &our_allocator->_context;
  } else if (base::mac::IsOSLion()) {
    ChromeCFAllocatorLion* our_allocator =
        const_cast<ChromeCFAllocatorLion*>(
            reinterpret_cast<const ChromeCFAllocatorLion*>(allocator));
    return &our_allocator->_context;
  } else {
    return NULL;
  }
}

CFAllocatorAllocateCallBack g_old_cfallocator_system_default;
CFAllocatorAllocateCallBack g_old_cfallocator_malloc;
CFAllocatorAllocateCallBack g_old_cfallocator_malloc_zone;

void* oom_killer_cfallocator_system_default(CFIndex alloc_size,
                                            CFOptionFlags hint,
                                            void* info) {
  void* result = g_old_cfallocator_system_default(alloc_size, hint, info);
  if (!result)
    debug::BreakDebugger();
  return result;
}

void* oom_killer_cfallocator_malloc(CFIndex alloc_size,
                                    CFOptionFlags hint,
                                    void* info) {
  void* result = g_old_cfallocator_malloc(alloc_size, hint, info);
  if (!result)
    debug::BreakDebugger();
  return result;
}

void* oom_killer_cfallocator_malloc_zone(CFIndex alloc_size,
                                         CFOptionFlags hint,
                                         void* info) {
  void* result = g_old_cfallocator_malloc_zone(alloc_size, hint, info);
  if (!result)
    debug::BreakDebugger();
  return result;
}

// === Cocoa NSObject allocation ===

typedef id (*allocWithZone_t)(id, SEL, NSZone*);
allocWithZone_t g_old_allocWithZone;

id oom_killer_allocWithZone(id self, SEL _cmd, NSZone* zone)
{
  id result = g_old_allocWithZone(self, _cmd, zone);
  if (!result)
    debug::BreakDebugger();
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

  // See http://trac.webkit.org/changeset/53362/trunk/Tools/DumpRenderTree/mac
  bool zone_allocators_protected = base::mac::IsOSLionOrLater();

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

  bool cf_allocator_internals_known = CanGetContextForCFAllocator();

  if (cf_allocator_internals_known) {
    CFAllocatorContext* context =
        ContextForCFAllocator(kCFAllocatorSystemDefault);
    CHECK(context) << "Failed to get context for kCFAllocatorSystemDefault.";
    g_old_cfallocator_system_default = context->allocate;
    CHECK(g_old_cfallocator_system_default)
        << "Failed to get kCFAllocatorSystemDefault allocation function.";
    context->allocate = oom_killer_cfallocator_system_default;

    context = ContextForCFAllocator(kCFAllocatorMalloc);
    CHECK(context) << "Failed to get context for kCFAllocatorMalloc.";
    g_old_cfallocator_malloc = context->allocate;
    CHECK(g_old_cfallocator_malloc)
        << "Failed to get kCFAllocatorMalloc allocation function.";
    context->allocate = oom_killer_cfallocator_malloc;

    context = ContextForCFAllocator(kCFAllocatorMallocZone);
    CHECK(context) << "Failed to get context for kCFAllocatorMallocZone.";
    g_old_cfallocator_malloc_zone = context->allocate;
    CHECK(g_old_cfallocator_malloc_zone)
        << "Failed to get kCFAllocatorMallocZone allocation function.";
    context->allocate = oom_killer_cfallocator_malloc_zone;
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

ProcessId GetParentProcessId(ProcessHandle process) {
  struct kinfo_proc info;
  size_t length = sizeof(struct kinfo_proc);
  int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, process };
  if (sysctl(mib, 4, &info, &length, NULL, 0) < 0) {
    DPLOG(ERROR) << "sysctl";
    return -1;
  }
  if (length == 0)
    return -1;
  return info.kp_eproc.e_ppid;
}

namespace {

const int kWaitBeforeKillSeconds = 2;

// Reap |child| process. This call blocks until completion.
void BlockingReap(pid_t child) {
  const pid_t result = HANDLE_EINTR(waitpid(child, NULL, 0));
  if (result == -1) {
    DPLOG(ERROR) << "waitpid(" << child << ", NULL, 0)";
  }
}

// Waits for |timeout| seconds for the given |child| to exit and reap it. If
// the child doesn't exit within the time specified, kills it.
//
// This function takes two approaches: first, it tries to use kqueue to
// observe when the process exits. kevent can monitor a kqueue with a
// timeout, so this method is preferred to wait for a specified period of
// time. Once the kqueue indicates the process has exited, waitpid will reap
// the exited child. If the kqueue doesn't provide an exit event notification,
// before the timeout expires, or if the kqueue fails or misbehaves, the
// process will be mercilessly killed and reaped.
//
// A child process passed to this function may be in one of several states:
// running, terminated and not yet reaped, and (apparently, and unfortunately)
// terminated and already reaped. Normally, a process will at least have been
// asked to exit before this function is called, but this is not required.
// If a process is terminating and unreaped, there may be a window between the
// time that kqueue will no longer recognize it and when it becomes an actual
// zombie that a non-blocking (WNOHANG) waitpid can reap. This condition is
// detected when kqueue indicates that the process is not running and a
// non-blocking waitpid fails to reap the process but indicates that it is
// still running. In this event, a blocking attempt to reap the process
// collects the known-dying child, preventing zombies from congregating.
//
// In the event that the kqueue misbehaves entirely, as it might under a
// EMFILE condition ("too many open files", or out of file descriptors), this
// function will forcibly kill and reap the child without delay. This
// eliminates another potential zombie vector. (If you're out of file
// descriptors, you're probably deep into something else, but that doesn't
// mean that zombies be allowed to kick you while you're down.)
//
// The fact that this function seemingly can be called to wait on a child
// that's not only already terminated but already reaped is a bit of a
// problem: a reaped child's pid can be reclaimed and may refer to a distinct
// process in that case. The fact that this function can seemingly be called
// to wait on a process that's not even a child is also a problem: kqueue will
// work in that case, but waitpid won't, and killing a non-child might not be
// the best approach.
void WaitForChildToDie(pid_t child, int timeout) {
  DCHECK(child > 0);
  DCHECK(timeout > 0);

  // DON'T ADD ANY EARLY RETURNS TO THIS FUNCTION without ensuring that
  // |child| has been reaped. Specifically, even if a kqueue, kevent, or other
  // call fails, this function should fall back to the last resort of trying
  // to kill and reap the process. Not observing this rule will resurrect
  // zombies.

  int result;

  int kq = HANDLE_EINTR(kqueue());
  if (kq == -1) {
    DPLOG(ERROR) << "kqueue()";
  } else {
    file_util::ScopedFD auto_close_kq(&kq);

    struct kevent change = {0};
    EV_SET(&change, child, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
    result = HANDLE_EINTR(kevent(kq, &change, 1, NULL, 0, NULL));

    if (result == -1) {
      if (errno != ESRCH) {
        DPLOG(ERROR) << "kevent (setup " << child << ")";
      } else {
        // At this point, one of the following has occurred:
        // 1. The process has died but has not yet been reaped.
        // 2. The process has died and has already been reaped.
        // 3. The process is in the process of dying. It's no longer
        //    kqueueable, but it may not be waitable yet either. Mark calls
        //    this case the "zombie death race".

        result = HANDLE_EINTR(waitpid(child, NULL, WNOHANG));

        if (result != 0) {
          // A positive result indicates case 1. waitpid succeeded and reaped
          // the child. A result of -1 indicates case 2. The child has already
          // been reaped. In both of these cases, no further action is
          // necessary.
          return;
        }

        // |result| is 0, indicating case 3. The process will be waitable in
        // short order. Fall back out of the kqueue code to kill it (for good
        // measure) and reap it.
      }
    } else {
      // Keep track of the elapsed time to be able to restart kevent if it's
      // interrupted.
      TimeDelta remaining_delta = TimeDelta::FromSeconds(timeout);
      Time deadline = Time::Now() + remaining_delta;
      result = -1;
      struct kevent event = {0};
      while (remaining_delta.InMilliseconds() > 0) {
        const struct timespec remaining_timespec = remaining_delta.ToTimeSpec();
        result = kevent(kq, NULL, 0, &event, 1, &remaining_timespec);
        if (result == -1 && errno == EINTR) {
          remaining_delta = deadline - Time::Now();
          result = 0;
        } else {
          break;
        }
      }

      if (result == -1) {
        DPLOG(ERROR) << "kevent (wait " << child << ")";
      } else if (result > 1) {
        DLOG(ERROR) << "kevent (wait " << child << "): unexpected result "
                    << result;
      } else if (result == 1) {
        if ((event.fflags & NOTE_EXIT) &&
            (event.ident == static_cast<uintptr_t>(child))) {
          // The process is dead or dying. This won't block for long, if at
          // all.
          BlockingReap(child);
          return;
        } else {
          DLOG(ERROR) << "kevent (wait " << child
                      << "): unexpected event: fflags=" << event.fflags
                      << ", ident=" << event.ident;
        }
      }
    }
  }

  // The child is still alive, or is very freshly dead. Be sure by sending it
  // a signal. This is safe even if it's freshly dead, because it will be a
  // zombie (or on the way to zombiedom) and kill will return 0 even if the
  // signal is not delivered to a live process.
  result = kill(child, SIGKILL);
  if (result == -1) {
    DPLOG(ERROR) << "kill(" << child << ", SIGKILL)";
  } else {
    // The child is definitely on the way out now. BlockingReap won't need to
    // wait for long, if at all.
    BlockingReap(child);
  }
}

}  // namespace

void EnsureProcessTerminated(ProcessHandle process) {
  WaitForChildToDie(process, kWaitBeforeKillSeconds);
}

}  // namespace base
