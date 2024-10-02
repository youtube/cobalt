// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/titlebar_config.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

bool ShouldCustomDrawSystemTitlebar() {
  // Cache flag lookup.
  static const bool custom_titlebar_disabled =
      base::CommandLine::InitializedForCurrentProcess() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWindows10CustomTitlebar);

  return !custom_titlebar_disabled;
}
