// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SYS_UTILS_H_
#define BASE_ANDROID_SYS_UTILS_H_

<<<<<<< HEAD
=======
#include "base/android/jni_android.h"
>>>>>>> ab415d8daaf (BACKPORT: Enable low-end device mode on Android (#12967))
#include "base/base_export.h"

namespace base::android {

// Returns the RAM thresholds below which a device is considered low-RAM,
// obtained from a feature param
BASE_EXPORT int GetCachedLowMemoryDeviceThresholdMb();

<<<<<<< HEAD
// Queries ActivityManager and checks if the process importance is Cached or
// below.
BASE_EXPORT bool IsProcessInBackground();

}  // namespace base::android
=======
// Returns the RAM thresholds below which a device is considered low-RAM,
// obtained from a feature param
BASE_EXPORT int GetCachedLowMemoryDeviceThresholdMb();

}  // namespace android
}  // namespace base
>>>>>>> ab415d8daaf (BACKPORT: Enable low-end device mode on Android (#12967))

#endif  // BASE_ANDROID_SYS_UTILS_H_
