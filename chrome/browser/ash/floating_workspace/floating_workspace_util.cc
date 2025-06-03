// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

PrefService* GetPrimaryUserPrefService() {
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  auto* user_profile = ProfileHelper::Get()->GetProfileByUser(primary_user);
  return user_profile->GetPrefs();
}

}  // namespace

namespace floating_workspace_util {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFloatingWorkspaceEnabled, false);
  registry->RegisterBooleanPref(prefs::kFloatingWorkspaceV2Enabled, false);
}

// TODO(b/297795546): Clean up V1 code path and feature flag check.
bool IsFloatingWorkspaceV1Enabled() {
  return features::IsFloatingWorkspaceEnabled();
}

bool IsFloatingWorkspaceV2Enabled() {
  PrefService* pref_service = GetPrimaryUserPrefService();
  DCHECK(pref_service);

  const PrefService::Preference* floating_workspace_pref =
      pref_service->FindPreference(prefs::kFloatingWorkspaceEnabled);

  DCHECK(floating_workspace_pref);

  if (floating_workspace_pref->IsManaged()) {
    // If there is a policy managing the pref, return what is set by policy.
    return pref_service->GetBoolean(prefs::kFloatingWorkspaceEnabled);
  }
  // TODO(b/297795546): Remove external ash feature flag.
  return features::IsFloatingWorkspaceV2Enabled();
}

bool IsInternetConnected() {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  return nsh != nullptr &&
         nsh->ConnectedNetworkByType(NetworkTypePattern::Default()) != nullptr;
}

}  // namespace floating_workspace_util
}  // namespace ash
