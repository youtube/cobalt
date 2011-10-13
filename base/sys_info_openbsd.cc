// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <sys/param.h>
#include <sys/sysctl.h>

#include "base/logging.h"

namespace base {

int SysInfo::NumberOfProcessors() {
  int mib[] = { CTL_HW, HW_NCPU };
  int ncpu;
  size_t size = sizeof(ncpu);
  if (sysctl(mib, 2, &ncpu, &size, NULL, 0) == -1) {
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

}  // namespace base
