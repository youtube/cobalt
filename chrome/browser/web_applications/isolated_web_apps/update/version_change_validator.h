// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_VERSION_CHANGE_VALIDATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_VERSION_CHANGE_VALIDATOR_H_

#include "components/webapps/isolated_web_apps/types/iwa_version.h"

namespace web_app {

enum class VersionChangeValidationResult {
  kSameVersionUpdateDisallowed,
  kDowngradeDisallowed,
  kAllowed
};

// Checks if version change is allowed for given arguments.
VersionChangeValidationResult ValidateVersionChangeFeasibility(
    const IwaVersion& expected_version,
    const IwaVersion& installed_version,
    bool allow_downgrades,
    bool same_version_update_allowed_by_key_rotation);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_VERSION_CHANGE_VALIDATOR_H_
