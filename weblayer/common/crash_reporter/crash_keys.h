// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_COMMON_CRASH_REPORTER_CRASH_KEYS_H_
#define WEBLAYER_COMMON_CRASH_REPORTER_CRASH_KEYS_H_

namespace weblayer {
namespace crash_keys {

// Crash Key Name Constants ////////////////////////////////////////////////////

// Application information.
extern const char kAppPackageName[];
extern const char kAppPackageVersionCode[];

extern const char kAndroidSdkInt[];

extern const char kWeblayerWebViewCompatMode[];

extern const char* const kWebLayerCrashKeyAllowList[];

}  // namespace crash_keys

void SetWebLayerCrashKeys();

}  // namespace weblayer

#endif  // WEBLAYER_COMMON_CRASH_REPORTER_CRASH_KEYS_H_
