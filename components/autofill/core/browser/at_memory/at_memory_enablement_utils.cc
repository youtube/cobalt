// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"

#include "base/feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"

namespace autofill {

namespace {

[[nodiscard]] bool IsPersonalContextEligible(
    personal_context::PersonalContextEnablementService*
        personal_context_service) {
  if (!personal_context_service) {
    return false;
  }
  using enum personal_context::PersonalContextEnablementState;
  switch (personal_context_service->GetEnablementState()) {
    case kDisabledNotEligible:
      return false;
    case kDisabledNeedsOptIn:
    case kDisabledViaPersonalIntelligenceInAutofillToggle:
    case kEnabledShouldShowNotice:
    case kEnabled:
      return true;
  }
  NOTREACHED();
}

[[nodiscard]] bool IsPersonalContextToggleOn(const PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }
  return pref_service->GetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus);
}

// Returns true if AtMemory is supported for the user.
//
// Checks that AtMemory feature flags are enabled, At-Memory eligibility
// criteria and PersonalContext eligibility criteria are met.
// Contrary to `MayPerformAtMemoryAction`, does not check user-controlled
// toggles.
[[nodiscard]] bool IsAtMemorySupported(
    personal_context::PersonalContextEnablementService*
        personal_context_service) {
  // TODO(crbug.com/522695178) Allow overriding the eligibility checks for
  // testing and dogfooding.

  if (!IsPersonalContextEligible(personal_context_service)) {
    return false;
  }
  // TODO(crbug.com/517490748) Check blocklist.
  // TODO(crbug.com/521270638) Check enterprise policy implementation.
  // TODO(crbug.com/505644871) Disable atmemory in non-branded Chromium builds.
  // TODO(crbug.com/517838959) Check subscription tier eligibility.

  // TODO(crbug.com/509479886) Add unit test to ensure this is checked last.
  return base::FeatureList::IsEnabled(features::kAutofillAtMemory);
}

[[nodiscard]] bool SatisfiesPersonalContextToggleRequirement(
    AtMemoryAction action,
    const PrefService* pref_service) {
  switch (action) {
    case AtMemoryAction::kTriggerSearchUI:
    case AtMemoryAction::kAllowCustomizeAtMemoryShortcut:
      return IsPersonalContextToggleOn(pref_service);
    case AtMemoryAction::kShowAtMemoryInSettings:
      return true;
  }
  NOTREACHED();
}

}  // namespace

bool MayPerformAtMemoryAction(
    AtMemoryAction action,
    personal_context::PersonalContextEnablementService*
        personal_context_service,
    const PrefService* pref_service) {
  if (!IsAtMemorySupported(personal_context_service)) {
    return false;
  }

  return SatisfiesPersonalContextToggleRequirement(action, pref_service);
}

}  // namespace autofill
