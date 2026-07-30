// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/themes_and_customization_step_eligibility_checker.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"

ThemesAndCustomizationStepEligibilityChecker::
    ThemesAndCustomizationStepEligibilityChecker() = default;

ThemesAndCustomizationStepEligibilityChecker::
    ~ThemesAndCustomizationStepEligibilityChecker() = default;

void ThemesAndCustomizationStepEligibilityChecker::CheckEligibility(
    Profile& profile,
    base::OnceCallback<void(bool)> callback) {
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(&profile);
  std::move(callback).Run(theme_service && !theme_service->UsingPolicyTheme());
}

std::string ThemesAndCustomizationStepEligibilityChecker::GetStepIdentifier()
    const {
  return std::string(kFeatureShowcaseThemesAndCustomizationStepIdentifier);
}
