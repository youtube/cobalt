// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <string>

#include "base/strings/sys_string_conversions.h"

namespace syncer {

// On iOS 16+, UIDevice.currentDevice.name no longer returns the user-assigned
// device name (e.g., "Jane's iPhone") by default; instead, it returns a
// generic model name (e.g., "iPhone" or "iPad"). Accessing the user-assigned
// name requires a special entitlement.
// See: https://developer.apple.com/documentation/uikit/uidevice/name#Discussion
std::string GetPersonalizableDeviceNameInternal() {
  return base::SysNSStringToUTF8(UIDevice.currentDevice.name);
}

}  // namespace syncer
