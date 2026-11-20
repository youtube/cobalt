// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update/version_change_validator.h"

namespace web_app {

VersionChangeValidationResult ValidateVersionChangeFeasibility(
    const IwaVersion& expected_version,
    const IwaVersion& installed_version,
    bool allow_downgrades,
    bool same_version_update_allowed_by_key_rotation) {
  if (expected_version < installed_version && !allow_downgrades) {
    return VersionChangeValidationResult::kDowngradeDisallowed;
  }
  if (expected_version == installed_version &&
      !same_version_update_allowed_by_key_rotation) {
    return VersionChangeValidationResult::kSameVersionUpdateDisallowed;
  }
  return VersionChangeValidationResult::kAllowed;
}

}  // namespace web_app
