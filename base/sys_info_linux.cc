// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include "base/file_util.h"
#include "base/logging.h"

namespace base {

int64 SysInfo::AmountOfPhysicalMemory() {
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages == -1 || page_size == -1) {
    NOTREACHED();
    return 0;
  }

  return static_cast<int64>(pages) * page_size;
}

// static
size_t SysInfo::MaxSharedMemorySize() {
  static size_t limit;
  static bool limit_valid = false;
  if (!limit_valid) {
    std::string contents;
    file_util::ReadFileToString(FilePath("/proc/sys/kernel/shmmax"), &contents);
    limit = strtoul(contents.c_str(), NULL, 0);
    limit_valid = true;
  }
  return limit;
}

}  // namespace base
