// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_ONBOARDING_TRACKER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_ONBOARDING_TRACKER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/service/glic_onboarding_status.h"

class PrefService;
class Profile;

namespace glic {

class GlicEnabling;

// Tracks profile onboarding milestones (Invoke, OptIn, and Prompt) and persists
// them in local profile preferences.
class GlicOnboardingTracker {
 public:
  GlicOnboardingTracker(Profile* profile, GlicEnabling* enabling);
  GlicOnboardingTracker(const GlicOnboardingTracker&) = delete;
  GlicOnboardingTracker& operator=(const GlicOnboardingTracker&) = delete;
  ~GlicOnboardingTracker();

  void OnInvoke();
  void OnPrompt();

  OnboardingStatus GetStatus() const;

 private:
  void MigrateInitialOnboardingStatus(Profile* profile);
  void OnConsentChanged();

  GlicOnboardingStatus onboarding_status_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<GlicEnabling> enabling_;
  base::CallbackListSubscription consent_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_ONBOARDING_TRACKER_H_
