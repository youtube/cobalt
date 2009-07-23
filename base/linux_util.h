// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LINUX_UTIL_H__
#define BASE_LINUX_UTIL_H__

#include <stdint.h>

#include <string>

namespace base {

// Makes a copy of |pixels| with the ordering changed from BGRA to RGBA.
// The caller is responsible for free()ing the data. If |stride| is 0,
// it's assumed to be 4 * |width|.
uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride);

// Get the Linux Distro if we can, or return "Unknown", similar to
// GetWinVersion() in base/win_util.h.
std::string GetLinuxDistro();

// These are used to derive mocks for unittests.
class EnvironmentVariableGetter {
 public:
  virtual ~EnvironmentVariableGetter() {}
  // Gets an environment variable's value and stores it in
  // result. Returns false if the key is unset.
  virtual bool Getenv(const char* variable_name, std::string* result) = 0;

  // Create an instance of EnvironmentVariableGetter
  static EnvironmentVariableGetter* Create();
};

enum DesktopEnvironment {
  DESKTOP_ENVIRONMENT_OTHER,
  DESKTOP_ENVIRONMENT_GNOME,
  DESKTOP_ENVIRONMENT_KDE,
};

// Return an entry from the DesktopEnvironment enum with a best guess
// of which desktop environment we're using.  We use this to know when
// to attempt to use preferences from the desktop environment --
// proxy settings, password manager, etc.
DesktopEnvironment GetDesktopEnvironment(EnvironmentVariableGetter* env);

}  // namespace base

#endif  // BASE_LINUX_UTIL_H__
