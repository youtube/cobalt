// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/android_info.h"

namespace syncer {

// On Android, retrieving the actual user-defined device name (e.g., "John's
// Phone") is restricted due to a combination of OEM customizations and
// privacy permissions.
//
// While it is sometimes available via Bluetooth settings, accessing it requires
// extra permissions. Therefore, the function falls back to returning the
// hardware model name.
std::string GetPersonalizableDeviceNameInternal() {
  return base::android::android_info::model();
}

}  // namespace syncer
