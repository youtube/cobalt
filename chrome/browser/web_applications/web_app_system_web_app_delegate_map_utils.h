// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYSTEM_WEB_APP_DELEGATE_MAP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYSTEM_WEB_APP_DELEGATE_MAP_UTILS_H_

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class WebAppRegistrar;

// Returns the app id for the given System App |type|.
absl::optional<AppId> GetAppIdForSystemApp(
    const WebAppRegistrar& registrar,
    const ash::SystemWebAppDelegateMap& delegates,
    ash::SystemWebAppType type);

// Returns the System App Type for the given |app_id|.
absl::optional<ash::SystemWebAppType> GetSystemAppTypeForAppId(
    const WebAppRegistrar& registrar,
    const ash::SystemWebAppDelegateMap& delegates,
    const AppId& app_id);

// Returns whether |app_id| points to an installed System App.
bool IsSystemWebApp(const WebAppRegistrar& registrar,
                    const ash::SystemWebAppDelegateMap& delegates,
                    const AppId& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYSTEM_WEB_APP_DELEGATE_MAP_UTILS_H_
