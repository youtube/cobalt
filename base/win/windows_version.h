// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_WINDOWS_VERSION_H_
#define BASE_WIN_WINDOWS_VERSION_H_
#pragma once

typedef void* HANDLE;

namespace base {
namespace win {

// NOTE: Keep these in order so callers can do things like
// "if (GetWinVersion() > WINVERSION_2000) ...".  It's OK to change the values,
// though.
enum Version {
  VERSION_PRE_2000 = 0,     // Not supported
  VERSION_2000 = 1,         // Not supported
  VERSION_XP = 2,
  VERSION_SERVER_2003 = 3,  // Also includes Windows XP Professional x64 edition
  VERSION_VISTA = 4,
  VERSION_2008 = 5,
  VERSION_WIN7 = 6,
};

// Returns the running version of Windows.
Version GetVersion();

// Returns the major and minor version of the service pack installed.
void GetServicePackLevel(int* major, int* minor);

enum WindowsArchitecture {
  X86_ARCHITECTURE,
  X64_ARCHITECTURE,
  IA64_ARCHITECTURE,
  OTHER_ARCHITECTURE,
};

// Returns the processor architecture this copy of Windows natively uses.
// For example, given an x64-capable processor, we have three possibilities:
//   32-bit Chrome running on 32-bit Windows:           X86_ARCHITECTURE
//   32-bit Chrome running on 64-bit Windows via WOW64: X64_ARCHITECTURE
//   64-bit Chrome running on 64-bit Windows:           X64_ARCHITECTURE
WindowsArchitecture GetWindowsArchitecture();

enum WOW64Status {
  WOW64_DISABLED,
  WOW64_ENABLED,
  WOW64_UNKNOWN,
};

// Returns whether this process is running under WOW64 (the wrapper that allows
// 32-bit processes to run on 64-bit versions of Windows).  This will return
// WOW64_DISABLED for both "32-bit Chrome on 32-bit Windows" and "64-bit Chrome
// on 64-bit Windows".  WOW64_UNKNOWN means "an error occurred", e.g. the
// process does not have sufficient access rights to determine this.
WOW64Status GetWOW64Status();

// Like GetWOW64Status(), but for the supplied handle instead of the current
// process.
WOW64Status GetWOW64StatusForProcess(HANDLE process_handle);

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_WINDOWS_VERSION_H_
