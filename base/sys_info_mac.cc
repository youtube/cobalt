// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>

#include "base/logging.h"

namespace base {

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

// static
void SysInfo::GetPrimaryDisplayDimensions(int* width, int* height) {
  CGDirectDisplayID main_display = CGMainDisplayID();
  if (width)
    *width = CGDisplayPixelsWide(main_display);
  if (height)
    *height = CGDisplayPixelsHigh(main_display);
}

// static
int SysInfo::DisplayCount() {
  // Don't just return the number of online displays.  It includes displays
  // that mirror other displays, which are not desired in the count.  It's
  // tempting to use the count returned by CGGetActiveDisplayList, but active
  // displays exclude sleeping displays, and those are desired in the count.

  // It would be ridiculous to have this many displays connected, but
  // CGDirectDisplayID is just an integer, so supporting up to this many
  // doesn't hurt.
  CGDirectDisplayID online_displays[128];
  CGDisplayCount online_display_count = 0;
  if (CGGetOnlineDisplayList(arraysize(online_displays),
                             online_displays,
                             &online_display_count) != kCGErrorSuccess) {
    // 1 is a reasonable assumption.
    return 1;
  }

  int display_count = 0;
  for (CGDisplayCount online_display_index = 0;
       online_display_index < online_display_count;
       ++online_display_index) {
    CGDirectDisplayID online_display = online_displays[online_display_index];
    if (CGDisplayMirrorsDisplay(online_display) == kCGNullDirectDisplay) {
      // If this display doesn't mirror any other, include it in the count.
      // The primary display in a mirrored set will be counted, but those that
      // mirror it will not be.
      ++display_count;
    }
  }

  return display_count;
}

}  // namespace base
