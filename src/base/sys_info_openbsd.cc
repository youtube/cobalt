// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <sys/param.h>
#include <sys/shm.h>
#include <sys/sysctl.h>

#include "base/logging.h"

namespace base {

int SysInfo::NumberOfProcessors() {
  int mib[] = { CTL_HW, HW_NCPU };
  int ncpu;
  size_t size = sizeof(ncpu);
  if (sysctl(mib, arraysize(mib), &ncpu, &size, NULL, 0) == -1) {
    NOTREACHED();
    return 1;
  }
  return ncpu;
}

int64 SysInfo::AmountOfPhysicalMemory() {
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGESIZE);
  if (pages == -1 || page_size == -1) {
    NOTREACHED();
    return 0;
  }

  return static_cast<int64>(pages) * page_size;
}

size_t SysInfo::MaxSharedMemorySize() {
  int mib[] = { CTL_KERN, KERN_SHMINFO, KERN_SHMINFO_SHMMAX };
  size_t limit;
  size_t size = sizeof(limit);
  if (sysctl(mib, arraysize(mib), &limit, &size, NULL, 0) < 0) {
    NOTREACHED();
    return 0;
  }
  return limit;
}

}  // namespace base
