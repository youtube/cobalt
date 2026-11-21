// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SYS_UTILS_H_
#define BASE_ANDROID_SYS_UTILS_H_

#include "base/android/jni_android.h"
#include "base/base_export.h"

namespace base {
namespace android {

class BASE_EXPORT SysUtils {
 public:
  // Returns true if system has low available memory.
  static bool IsCurrentlyLowMemory();
};

// Returns the RAM thresholds below which a device is considered low-RAM,
// obtained from a feature param
BASE_EXPORT int GetCachedLowMemoryDeviceThresholdMb();

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_SYS_UTILS_H_
