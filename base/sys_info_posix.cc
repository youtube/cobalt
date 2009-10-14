// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <errno.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#if defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#endif

#if defined(OS_OPENBSD)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace base {

int SysInfo::NumberOfProcessors() {
#if defined(OS_OPENBSD)
  int mib[] = { CTL_HW, HW_NCPU };
  int ncpu;
  size_t size = sizeof(ncpu);
  if (sysctl(mib, 2, &ncpu, &size, NULL, 0) == -1) {
    NOTREACHED();
    return 1;
  }
  return ncpu;
#else
  // It seems that sysconf returns the number of "logical" processors on both
  // mac and linux.  So we get the number of "online logical" processors.
  long res = sysconf(_SC_NPROCESSORS_ONLN);
  if (res == -1) {
    NOTREACHED();
    return 1;
  }

  return static_cast<int>(res);
#endif
}

// static
int64 SysInfo::AmountOfPhysicalMemory() {
  // _SC_PHYS_PAGES is not part of POSIX and not available on OS X or
  // FreeBSD
#if defined(OS_MACOSX)
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
#elif defined(OS_FREEBSD)
  // TODO(benl): I have no idea how to get this
  NOTIMPLEMENTED();
  return 0;
#else
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages == -1 || page_size == -1) {
    NOTREACHED();
    return 0;
  }

  return static_cast<int64>(pages) * page_size;
#endif
}

// static
int64 SysInfo::AmountOfFreeDiskSpace(const FilePath& path) {
  struct statvfs stats;
  if (statvfs(path.value().c_str(), &stats) != 0) {
    return -1;
  }
  return static_cast<int64>(stats.f_bavail) * stats.f_frsize;
}

// static
bool SysInfo::HasEnvVar(const wchar_t* var) {
  std::string var_utf8 = WideToUTF8(std::wstring(var));
  return getenv(var_utf8.c_str()) != NULL;
}

// static
std::wstring SysInfo::GetEnvVar(const wchar_t* var) {
  std::string var_utf8 = WideToUTF8(std::wstring(var));
  char* value = getenv(var_utf8.c_str());
  if (!value) {
    return std::wstring();
  } else {
    return UTF8ToWide(value);
  }
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

// static
void SysInfo::GetPrimaryDisplayDimensions(int* width, int* height) {
#if defined(OS_MACOSX)
  CGDirectDisplayID main_display = CGMainDisplayID();
  if (width)
    *width = CGDisplayPixelsWide(main_display);
  if (height)
    *height = CGDisplayPixelsHigh(main_display);
#else
  // TODO(port): http://crbug.com/21732
  NOTIMPLEMENTED();
  if (width)
    *width = 0;
  if (height)
    *height = 0;
#endif
}

// static
int SysInfo::DisplayCount() {
#if defined(OS_MACOSX)
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
#else
  // TODO(port): http://crbug.com/21732
  NOTIMPLEMENTED();
  return 1;
#endif
}

// static
size_t SysInfo::VMAllocationGranularity() {
  return getpagesize();
}

#if defined(OS_LINUX)
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
#endif

}  // namespace base
