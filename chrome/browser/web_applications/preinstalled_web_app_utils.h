// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {
class FilePath;
class Value;
}  // namespace base

class Profile;

namespace web_app {

class FileUtilsWrapper;

using OptionsOrError = absl::variant<ExternalInstallOptions, std::string>;

OptionsOrError ParseConfig(FileUtilsWrapper& file_utils,
                           const base::FilePath& dir,
                           const base::FilePath& file,
                           const base::Value& app_config);

using IconBitmapsOrError =
    base::expected<std::map<SquareSizePx, SkBitmap>, std::string>;

IconBitmapsOrError ParseOfflineManifestIconBitmaps(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& manifest_file,
    const char* icon_key,
    const base::Value* icon_files);

using WebAppInstallInfoFactoryOrError =
    absl::variant<WebAppInstallInfoFactory, std::string>;

WebAppInstallInfoFactoryOrError ParseOfflineManifest(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& file,
    const base::Value& offline_manifest);

// Returns true if we need to update the app. |current_milestone_str| is the
// browser's binary milestone number.
// |last_preinstall_synchronize_milestone_str| is a previous milestone for the
// user's data state. For example, if |force_reinstall_for_milestone| value is
// 89 then we need to update the app on all browser upgrades from <89 to >=89.
bool IsReinstallPastMilestoneNeeded(
    base::StringPiece last_preinstall_synchronize_milestone_str,
    base::StringPiece current_milestone_str,
    int force_reinstall_for_milestone);

// Returns and sets whether the app indicated by `app_id` was migrated to a
// web app.
bool WasAppMigratedToWebApp(Profile* profile, const std::string& app_id);
void MarkAppAsMigratedToWebApp(Profile* profile,
                               const std::string& app_id,
                               bool was_migrated);

// Returns and sets whether the app indicated by `app_id` was unisntalled
bool WasPreinstalledAppUninstalled(Profile* profile, const std::string& app_id);
void MarkPreinstalledAppAsUninstalled(Profile* profile,
                                      const std::string& app_id);

// Returns and sets whether the migration was run for the feature.
bool WasMigrationRun(Profile* profile, base::StringPiece feature_name);
void SetMigrationRun(Profile* profile,
                     base::StringPiece feature_name,
                     bool was_migrated);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_UTILS_H_
