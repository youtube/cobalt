// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <limits.h>
#include <mach/task.h>
#include <mach/vm_region.h>
#include <malloc/malloc.h>
#include <stddef.h>

#include "base/check_op.h"
#include "base/mac/scoped_mach_port.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "build/blink_buildflags.h"

namespace base {

ProcessMetrics::ProcessMetrics(ProcessHandle process) {}

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process) {
  return WrapUnique(new ProcessMetrics(process));
}

TimeDelta ProcessMetrics::GetCumulativeCPUUsage() {
  NOTIMPLEMENTED();
  return TimeDelta();
}

// The blink code path pulls in process_metrics_posix.cc which
// is used for the following implementations.
#if !BUILDFLAG(USE_BLINK)

size_t GetMaxFds() {
  static const rlim_t kSystemDefaultMaxFds = 256;
  rlim_t max_fds;
  struct rlimit nofile;
  if (getrlimit(RLIMIT_NOFILE, &nofile)) {
    // Error case: Take a best guess.
    max_fds = kSystemDefaultMaxFds;
  } else {
    max_fds = nofile.rlim_cur;
  }

  if (max_fds > INT_MAX)
    max_fds = INT_MAX;

  return static_cast<size_t>(max_fds);
}

void IncreaseFdLimitTo(unsigned int max_descriptors) {
  // Unimplemented.
}

size_t ProcessMetrics::GetMallocUsage() {
  malloc_statistics_t stats;
  malloc_zone_statistics(nullptr, &stats);
  return stats.size_in_use;
}
#endif  // !BUILDFLAG(USE_BLINK)

// Bytes committed by the system.
size_t GetSystemCommitCharge() {
  NOTIMPLEMENTED();
  return 0;
}

bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  struct host_basic_info hostinfo;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  base::mac::ScopedMachSendRight host(mach_host_self());
  int result = host_info(host.get(), HOST_BASIC_INFO,
                         reinterpret_cast<host_info_t>(&hostinfo), &count);
  if (result != KERN_SUCCESS)
    return false;

  DCHECK_EQ(HOST_BASIC_INFO_COUNT, count);
  meminfo->total = static_cast<int>(hostinfo.max_mem / 1024);

  vm_statistics64_data_t vm_info;
  count = HOST_VM_INFO64_COUNT;

  if (host_statistics64(host.get(), HOST_VM_INFO64,
                        reinterpret_cast<host_info64_t>(&vm_info),
                        &count) != KERN_SUCCESS) {
    return false;
  }
  DCHECK_EQ(HOST_VM_INFO64_COUNT, count);

  // Check that PAGE_SIZE is divisible by 1024 (2^10).
  CHECK_EQ(PAGE_SIZE, (PAGE_SIZE >> 10) << 10);
  meminfo->free = saturated_cast<int>(
      PAGE_SIZE / 1024 * (vm_info.free_count - vm_info.speculative_count));
  meminfo->speculative =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.speculative_count);
  meminfo->file_backed =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.external_page_count);
  meminfo->purgeable =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.purgeable_count);

  return true;
}

MachVMRegionResult ParseOutputFromVMRegion(kern_return_t kr) {
  if (kr == KERN_INVALID_ADDRESS) {
    // We're at the end of the address space.
    return MachVMRegionResult::Finished;
  } else if (kr != KERN_SUCCESS) {
    return MachVMRegionResult::Error;
  }
  return MachVMRegionResult::Success;
}

MachVMRegionResult GetBasicInfo(mach_port_t task,
                                mach_vm_size_t* size,
                                mach_vm_address_t* address,
                                vm_region_basic_info_64* info) {
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  mac::ScopedMachSendRight object_name;
  kern_return_t kr =
      vm_region_64(task, reinterpret_cast<vm_address_t*>(address),
                   reinterpret_cast<vm_size_t*>(size), VM_REGION_BASIC_INFO_64,
                   reinterpret_cast<vm_region_info_t>(info), &info_count,
                   mac::ScopedMachSendRight::Receiver(object_name).get());
  return ParseOutputFromVMRegion(kr);
}

}  // namespace base
