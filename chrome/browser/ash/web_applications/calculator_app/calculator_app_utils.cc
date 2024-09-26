// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/calculator_app/calculator_app_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "extensions/common/constants.h"

namespace ash {
namespace calculator_app {

std::string GetInstalledCalculatorAppId(Profile* profile) {
  if (extensions::IsExtensionInstalled(profile,
                                       extension_misc::kCalculatorAppId)) {
    return extension_misc::kCalculatorAppId;
  }
  return web_app::kCalculatorAppId;
}

}  // namespace calculator_app
}  // namespace ash
