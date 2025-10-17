// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_availability.h"

#include "ash/constants/generative_ai_country_restrictions.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/user_manager/user_manager.h"
#include "components/variations/service/variations_service.h"

namespace ash::mahi_availability {

std::optional<bool> CanUseMahiService() {
  if (!manta::features::IsMantaServiceEnabled()) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kMahiRestrictionsOverride)) {
    return true;
  }

  if (!ash::DemoSession::IsDeviceInDemoMode()) {
    if (!user_manager::UserManager::IsInitialized() ||
        !user_manager::UserManager::Get()->IsUserLoggedIn()) {
      return false;
    }

    Profile* profile = ProfileManager::GetActiveUserProfile();
    if (!profile) {
      return false;
    }

    // Controls for managed users.
    if (profile->GetProfilePolicyConnector()->IsManaged() &&
        !chromeos::features::IsMahiManagedEnabled()) {
      return false;
    }

    // Guest session is not allowed when not in demo mode.
    if (profile->IsGuestSession()) {
      return false;
    }

    // MantaService might not be available in tests.
    if (manta::MantaService* service =
            manta::MantaServiceFactory::GetForProfile(profile);
        service) {
      switch (service->CanAccessMantaFeaturesWithoutMinorRestrictions()) {
        case manta::FeatureSupportStatus::kSupported:
          break;
        case manta::FeatureSupportStatus::kUnsupported:
          return false;
        case manta::FeatureSupportStatus::kUnknown:
          return std::nullopt;
      }
    }
  }

  const std::string country_code =
      (g_browser_process != nullptr &&
       g_browser_process->variations_service() != nullptr)
          ? g_browser_process->variations_service()->GetLatestCountry()
          : std::string();

  return IsGenerativeAiAllowedForCountry(country_code);
}

std::optional<bool> IsMahiAvailable() {
  if (!chromeos::features::IsMahiEnabled()) {
    return false;
  }

  return CanUseMahiService();
}

bool IsPompanoAvailable() {
  return chromeos::features::IsPompanoEnabled() && IsMahiAvailable();
}

}  // namespace ash::mahi_availability
