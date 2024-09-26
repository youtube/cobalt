// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_UTILS_H_
#define CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_UTILS_H_

#include "ash/components/arc/mojom/app.mojom.h"

namespace app_restore {
struct AppRestoreData;
}

namespace arc {

// This is use for indecating the launch source of ghost window.
enum class GhostWindowType {
  // App Restore.
  kFullRestore = 0,
  // User launch action (e.g. user launch app).
  kAppLaunch = 1,
  // launch ARC app which need fixup.
  kFixup = 2,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kFixup,
};

// Is the the window info provide enough data to create corresponding ARC ghost
// window.
bool CanLaunchGhostWindowByRestoreData(
    const app_restore::AppRestoreData& restore_data);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_UTILS_H_
