// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/google_lens_step_eligibility_checker.h"

#include <utility>

#include "base/functional/callback.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_permission_utils.h"

GoogleLensStepEligibilityChecker::GoogleLensStepEligibilityChecker() = default;

GoogleLensStepEligibilityChecker::~GoogleLensStepEligibilityChecker() = default;

void GoogleLensStepEligibilityChecker::CheckEligibility(
    Profile& profile,
    base::OnceCallback<void(bool)> callback) {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(crbug.com/506864805): Add Chromium path / adjustments.
  std::move(callback).Run(false);
#else
  if (!lens::LensOverlayEntryPointController::IsEnabledOnInit(&profile)) {
    std::move(callback).Run(false);
    return;
  }

  bool is_eligible = !lens::DidUserGrantLensOverlayNeededPermissions(&profile);
  std::move(callback).Run(is_eligible);
#endif
}

std::string GoogleLensStepEligibilityChecker::GetStepIdentifier() const {
  return std::string(kFeatureShowcaseGoogleLensStepIdentifier);
}
