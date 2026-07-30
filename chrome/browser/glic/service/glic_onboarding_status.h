// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_ONBOARDING_STATUS_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_ONBOARDING_STATUS_H_

#include "base/memory/raw_ptr.h"

class PrefService;

namespace glic {

// Values for the "glic.onboarding_status" pref.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OnboardingStatus)
enum class OnboardingStatus : int {
  kNoInteraction = 0,
  kOptedInButNotInvoked = 1,
  kNotOptedInButInvoked = 2,
  kOptedInAndInvoked = 3,
  kPromptWithNoOptIn = 4,
  kPromptAndOptIn = 5,
  kMaxValue = kPromptAndOptIn,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicOnboardingStatus)

// Encapsulates reading and mutating the Glic onboarding interaction status
// stored in profile preferences.
class GlicOnboardingStatus {
 public:
  explicit GlicOnboardingStatus(PrefService* pref_service);
  GlicOnboardingStatus(const GlicOnboardingStatus&) = delete;
  GlicOnboardingStatus& operator=(const GlicOnboardingStatus&) = delete;
  ~GlicOnboardingStatus();

  OnboardingStatus GetStatus() const;
  bool IsInvoked() const;
  bool IsOptedIn() const;
  bool HasPrompt() const;

  void SetStatus(OnboardingStatus status);

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_ONBOARDING_STATUS_H_
