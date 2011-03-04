// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/windows_version.h"

#include <windows.h>

#include "base/logging.h"

namespace base {
namespace win {

Version GetVersion() {
  static bool checked_version = false;
  static Version win_version = VERSION_PRE_2000;
  if (!checked_version) {
    OSVERSIONINFOEX version_info;
    version_info.dwOSVersionInfoSize = sizeof version_info;
    GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version_info));
    if (version_info.dwMajorVersion == 5) {
      switch (version_info.dwMinorVersion) {
        case 0:
          win_version = VERSION_2000;
          break;
        case 1:
          win_version = VERSION_XP;
          break;
        case 2:
        default:
          win_version = VERSION_SERVER_2003;
          break;
      }
    } else if (version_info.dwMajorVersion == 6) {
      if (version_info.wProductType != VER_NT_WORKSTATION) {
        // 2008 is 6.0, and 2008 R2 is 6.1.
        win_version = VERSION_2008;
      } else {
        if (version_info.dwMinorVersion == 0) {
          win_version = VERSION_VISTA;
        } else {
          win_version = VERSION_WIN7;
        }
      }
    } else if (version_info.dwMajorVersion > 6) {
      win_version = VERSION_WIN7;
    }
    checked_version = true;
  }
  return win_version;
}

void GetServicePackLevel(int* major, int* minor) {
  DCHECK(major && minor);
  static bool checked_version = false;
  static int service_pack_major = -1;
  static int service_pack_minor = -1;
  if (!checked_version) {
    OSVERSIONINFOEX version_info = {0};
    version_info.dwOSVersionInfoSize = sizeof(version_info);
    GetVersionEx(reinterpret_cast<OSVERSIONINFOW*>(&version_info));
    service_pack_major = version_info.wServicePackMajor;
    service_pack_minor = version_info.wServicePackMinor;
    checked_version = true;
  }

  *major = service_pack_major;
  *minor = service_pack_minor;
}

WOW64Status GetWOW64Status() {
  static WOW64Status wow64_status =
      GetWOW64StatusForProcess(GetCurrentProcess());
  return wow64_status;
}

WOW64Status GetWOW64StatusForProcess(HANDLE process_handle) {
  typedef BOOL (WINAPI* IsWow64ProcessFunc)(HANDLE, PBOOL);
  IsWow64ProcessFunc is_wow64_process = reinterpret_cast<IsWow64ProcessFunc>(
      GetProcAddress(GetModuleHandle(L"kernel32.dll"), "IsWow64Process"));
  if (!is_wow64_process)
    return WOW64_DISABLED;
  BOOL is_wow64 = FALSE;
  if (!(*is_wow64_process)(process_handle, &is_wow64))
    return WOW64_UNKNOWN;
  return is_wow64 ? WOW64_ENABLED : WOW64_DISABLED;
}

}  // namespace win
}  // namespace base
