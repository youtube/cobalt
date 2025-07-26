// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flags.h"

#include <optional>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

std::optional<bool> IsFedCmAuthzOverridden() {
  return base::FeatureList::GetStateIfOverridden(features::kFedCmAuthz);
}

bool IsFedCmAuthzFlagEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmAuthz);
}

bool IsFedCmMultipleIdentityProvidersEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmMultipleIdentityProviders);
}

bool IsFedCmMetricsEndpointEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmMetricsEndpoint);
}

bool IsFedCmSelectiveDisclosureEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmSelectiveDisclosure);
}

bool IsFedCmDelegationEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmDelegation);
}

bool IsFedCmIdPRegistrationEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmIdPRegistration);
}

bool IsFedCmWithoutWellKnownEnforcementEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmWithoutWellKnownEnforcement);
}

bool IsWebIdentityDigitalCredentialsEnabled() {
  return base::FeatureList::IsEnabled(features::kWebIdentityDigitalCredentials);
}

bool IsWebIdentityDigitalCredentialsCreationEnabled() {
  return base::FeatureList::IsEnabled(
      features::kWebIdentityDigitalCredentialsCreation);
}

bool IsFedCmUseOtherAccountEnabled() {
  // The active mode origin trial can also enable this feature at this moment.
  return base::FeatureList::IsEnabled(features::kFedCmUseOtherAccount) ||
         IsFedCmActiveModeEnabled();
}

bool IsFedCmActiveModeEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmButtonMode);
}

bool IsFedCmSameSiteLaxEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmSameSiteLax);
}

bool IsFedCmFlexibleFieldsEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmFlexibleFields);
}

bool IsFedCmShowFilteredAccountsEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmShowFilteredAccounts);
}

bool IsFedCmLightweightModeEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmLightweightMode);
}

bool IsFedCmAlternativeIdentifiersEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmAlternativeIdentifiers);
}

}  // namespace content
