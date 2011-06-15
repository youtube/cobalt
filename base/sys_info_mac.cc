// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>

#include "base/logging.h"
#include "base/stringprintf.h"

namespace base {

// static
std::string SysInfo::OperatingSystemName() {
  return "Mac OS X";
}

// static
std::string SysInfo::OperatingSystemVersion() {
  int32 major, minor, bugfix;
  OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  return base::StringPrintf("%d.%d.%d", major, minor, bugfix);
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32* major_version,
                                            int32* minor_version,
                                            int32* bugfix_version) {
  Gestalt(gestaltSystemVersionMajor,
      reinterpret_cast<SInt32*>(major_version));
  Gestalt(gestaltSystemVersionMinor,
      reinterpret_cast<SInt32*>(minor_version));
  Gestalt(gestaltSystemVersionBugFix,
      reinterpret_cast<SInt32*>(bugfix_version));
}

// static
int64 SysInfo::AmountOfPhysicalMemory() {
  struct host_basic_info hostinfo;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  int result = host_info(mach_host_self(),
                         HOST_BASIC_INFO,
                         reinterpret_cast<host_info_t>(&hostinfo),
                         &count);
  DCHECK_EQ(HOST_BASIC_INFO_COUNT, count);
  if (result != KERN_SUCCESS) {
    NOTREACHED();
    return 0;
  }

  return static_cast<int64>(hostinfo.max_mem);
}

}  // namespace base
