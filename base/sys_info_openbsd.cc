// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

}  // namespace base
