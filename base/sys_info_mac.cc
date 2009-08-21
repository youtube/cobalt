// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <CoreServices/CoreServices.h>

namespace base {

// static
void SysInfo::OperatingSystemVersionNumbers(int32 *major_version,
                                            int32 *minor_version,
                                            int32 *bugfix_version) {
  Gestalt(gestaltSystemVersionMajor,
      reinterpret_cast<SInt32*>(major_version));
  Gestalt(gestaltSystemVersionMinor,
      reinterpret_cast<SInt32*>(minor_version));
  Gestalt(gestaltSystemVersionBugFix,
      reinterpret_cast<SInt32*>(bugfix_version));
}

}  // namespace base
