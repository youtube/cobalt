// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_WINDOWS_VERSION_H_
#define BASE_WIN_WINDOWS_VERSION_H_
#pragma once

namespace base {
namespace win {

// NOTE: Keep these in order so callers can do things like
// "if (GetWinVersion() > WINVERSION_2000) ...".  It's OK to change the values,
// though.
enum Version {
  VERSION_PRE_2000 = 0,  // Not supported
  VERSION_2000 = 1,      // Not supported
  VERSION_XP = 2,
  VERSION_SERVER_2003 = 3,
  VERSION_VISTA = 4,
  VERSION_2008 = 5,
  VERSION_WIN7 = 6,
};

// Returns the running version of Windows.
Version GetVersion();

// Returns the major and minor version of the service pack installed.
void GetServicePackLevel(int* major, int* minor);

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_WINDOWS_VERSION_H_
