// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/login_screen_auth_policy_connector.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {
bool IsUserManaged(PrefService* local_state, const AccountId& account) {
  user_manager::KnownUser known_user(local_state);
  return known_user.GetIsEnterpriseManaged(account);
}
}  // namespace

LoginScreenAuthPolicyConnector::LoginScreenAuthPolicyConnector(
    PrefService* local_state)
    : local_state_(local_state) {}

LoginScreenAuthPolicyConnector::~LoginScreenAuthPolicyConnector() = default;

absl::optional<bool> LoginScreenAuthPolicyConnector::GetRecoveryInitialState(
    const AccountId& account) {
  return features::IsCryptohomeRecoveryEnabled() &&
         !IsUserManaged(local_state_, account);
}

absl::optional<bool> LoginScreenAuthPolicyConnector::GetRecoveryDefaultState(
    const AccountId& account) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

absl::optional<bool> LoginScreenAuthPolicyConnector::GetRecoveryMandatoryState(
    const AccountId& account) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

bool LoginScreenAuthPolicyConnector::IsAuthFactorManaged(
    const AccountId& account,
    AshAuthFactor auth_factor) {
  NOTIMPLEMENTED();
  return false;
}

bool LoginScreenAuthPolicyConnector::IsAuthFactorUserModifiable(
    const AccountId& account,
    AshAuthFactor auth_factor) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ash
