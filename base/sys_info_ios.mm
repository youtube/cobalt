// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#import <UIKit/UIKit.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"

namespace base {

// static
std::string SysInfo::OperatingSystemName() {
  base::mac::ScopedNSAutoreleasePool pool;
  // Examples of returned value: 'iPhone OS' on iPad 5.1.1
  // and iPhone 5.1.1.
  return SysNSStringToUTF8([[UIDevice currentDevice] systemName]);
}

// static
std::string SysInfo::OperatingSystemVersion() {
  base::mac::ScopedNSAutoreleasePool pool;
  return SysNSStringToUTF8([[UIDevice currentDevice] systemVersion]);
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32* major_version,
                                            int32* minor_version,
                                            int32* bugfix_version) {
  base::mac::ScopedNSAutoreleasePool pool;
  NSString* version = [[UIDevice currentDevice] systemVersion];
  NSArray* version_info = [version componentsSeparatedByString:@"."];
  NSUInteger length = [version_info count];

  *major_version = [[version_info objectAtIndex:0] intValue];

  if (length >= 2)
    *minor_version = [[version_info objectAtIndex:1] intValue];
  else
    *minor_version = 0;

  if (length >= 3)
    *bugfix_version = [[version_info objectAtIndex:2] intValue];
  else
    *bugfix_version = 0;
}

// static
int64 SysInfo::AmountOfPhysicalMemory() {
  struct host_basic_info hostinfo;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  int result = host_info(mach_host_self(),
                         HOST_BASIC_INFO,
                         reinterpret_cast<host_info_t>(&hostinfo),
                         &count);
  if (result != KERN_SUCCESS) {
    NOTREACHED();
    return 0;
  }
  DCHECK_EQ(HOST_BASIC_INFO_COUNT, count);
  return static_cast<int64>(hostinfo.max_mem);
}

// static
std::string SysInfo::CPUModelName() {
  char name[256];
  size_t len = arraysize(name);
  if (sysctlbyname("machdep.cpu.brand_string", &name, &len, NULL, 0) == 0)
    return name;
  return std::string();
}

}  // namespace base
