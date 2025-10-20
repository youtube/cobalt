// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_base/ios/ios_util.h"

#include <array>

#include "base/allocator/partition_allocator/partition_alloc_base/system/sys_info.h"

namespace partition_alloc::internal::base::ios {

bool IsRunningOnIOS12OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(12, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS13OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(13, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS14OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(14, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS15OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(15, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnOrLater(int32_t major, int32_t minor, int32_t bug_fix) {
  static const class OSVersion {
   public:
    OSVersion() {
      SysInfo::OperatingSystemVersionNumbers(
          &current_version_[0], &current_version_[1], &current_version_[2]);
    }

    bool IsRunningOnOrLater(int32_t version[3]) const {
      for (size_t i = 0; i < std::size(current_version_); ++i) {
        if (current_version_[i] != version[i])
          return current_version_[i] > version[i];
      }
      return true;
    }

   private:
    int32_t current_version_[3];
  } kOSVersion;

  int32_t version[3] = {major, minor, bug_fix};
  return kOSVersion.IsRunningOnOrLater(version);
}

}  // namespace partition_alloc::internal::base::ios
