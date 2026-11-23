// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_onboarding_status.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "components/prefs/pref_service.h"

namespace glic {

GlicOnboardingStatus::GlicOnboardingStatus(PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
}

GlicOnboardingStatus::~GlicOnboardingStatus() = default;

OnboardingStatus GlicOnboardingStatus::GetStatus() const {
  int status = pref_service_->GetInteger(prefs::kGlicOnboardingStatus);
  if (status < 0 || status > static_cast<int>(OnboardingStatus::kMaxValue)) {
    return OnboardingStatus::kNoInteraction;
  }
  return static_cast<OnboardingStatus>(status);
}

bool GlicOnboardingStatus::IsInvoked() const {
  OnboardingStatus status = GetStatus();
  return status == OnboardingStatus::kNotOptedInButInvoked ||
         status == OnboardingStatus::kOptedInAndInvoked || HasPrompt();
}

bool GlicOnboardingStatus::IsOptedIn() const {
  OnboardingStatus status = GetStatus();
  return status == OnboardingStatus::kOptedInButNotInvoked ||
         status == OnboardingStatus::kOptedInAndInvoked ||
         status == OnboardingStatus::kPromptAndOptIn;
}

bool GlicOnboardingStatus::HasPrompt() const {
  OnboardingStatus status = GetStatus();
  return status == OnboardingStatus::kPromptWithNoOptIn ||
         status == OnboardingStatus::kPromptAndOptIn;
}

void GlicOnboardingStatus::SetStatus(OnboardingStatus status) {
  if (GetStatus() == status) {
    return;
  }
  pref_service_->SetInteger(prefs::kGlicOnboardingStatus,
                            std::to_underlying(status));
}

}  // namespace glic
