// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/package_id_util.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps_util {

#if BUILDFLAG(IS_CHROMEOS_ASH)
absl::optional<apps::PackageId> GetPackageIdForApp(
    Profile* profile,
    const apps::AppUpdate& update) {
  if (update.AppType() != apps::AppType::kArc &&
      update.AppType() != apps::AppType::kWeb) {
    return absl::nullopt;
  }
  if (update.PublisherId().empty()) {
    return absl::nullopt;
  }

  // TWAs should have an ARC package ID despite being a webapp because the
  // package ID should represent how the app was installed.
  ash::ApkWebAppService* apk_web_app_service =
      ash::ApkWebAppService::Get(profile);
  if (apk_web_app_service && update.AppType() == apps::AppType::kWeb) {
    // Check whether the app is an installed (or installing) web app apk.
    absl::optional<std::string> package_name =
        apk_web_app_service->GetPackageNameForWebApp(
            update.AppId(), /*include_installing_apks=*/true);
    if (package_name.has_value()) {
      return apps::PackageId(apps::AppType::kArc, package_name.value());
    }
  }
  return apps::PackageId(update.AppType(), update.PublisherId());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps_util
