// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#if !defined(OS_MACOSX)
#include <gdk/gdk.h>
#endif

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace base {

#if !defined(OS_OPENBSD)
int SysInfo::NumberOfProcessors() {
  // It seems that sysconf returns the number of "logical" processors on both
  // Mac and Linux.  So we get the number of "online logical" processors.
  long res = sysconf(_SC_NPROCESSORS_ONLN);
  if (res == -1) {
    NOTREACHED();
    return 1;
  }

  return static_cast<int>(res);
}
#endif

// static
int64 SysInfo::AmountOfFreeDiskSpace(const FilePath& path) {
  struct statvfs stats;
  if (statvfs(path.value().c_str(), &stats) != 0) {
    return -1;
  }
  return static_cast<int64>(stats.f_bavail) * stats.f_frsize;
}

// static
std::string SysInfo::OperatingSystemName() {
  utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return "";
  }
  return std::string(info.sysname);
}

// static
std::string SysInfo::OperatingSystemVersion() {
  utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return "";
  }
  return std::string(info.release);
}

// static
std::string SysInfo::CPUArchitecture() {
  utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return "";
  }
  return std::string(info.machine);
}

#if !defined(OS_MACOSX)
// static
void SysInfo::GetPrimaryDisplayDimensions(int* width, int* height) {
  // Note that Bad Things Happen if this isn't called from the UI thread,
  // but also that there's no way to check that from here.  :(
  GdkScreen* screen = gdk_screen_get_default();
  if (width)
    *width = gdk_screen_get_width(screen);
  if (height)
    *height = gdk_screen_get_height(screen);
}

// static
int SysInfo::DisplayCount() {
  // Note that Bad Things Happen if this isn't called from the UI thread,
  // but also that there's no way to check that from here.  :(

  // This query is kinda bogus for Linux -- do we want number of X screens?
  // The number of monitors Xinerama has?  We'll just use whatever GDK uses.
  GdkScreen* screen = gdk_screen_get_default();
  return gdk_screen_get_n_monitors(screen);
}
#endif

// static
size_t SysInfo::VMAllocationGranularity() {
  return getpagesize();
}

}  // namespace base
