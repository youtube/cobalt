// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/nearby/nearby_dependencies_provider_factory.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace nearby {
namespace {

bool g_bypass_primary_user_check_for_testing = false;

}  // namespace

// static
NearbyProcessManager* NearbyProcessManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NearbyProcessManager*>(
      NearbyProcessManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
bool NearbyProcessManagerFactory::CanBeLaunchedForProfile(Profile* profile) {
  // We allow NearbyProcessManager to be used with the signin profile since it
  // is required for OOBE Quick Start.
  if (ProfileHelper::IsSigninProfile(profile) &&
      features::IsOobeQuickStartEnabled()) {
    return true;
  }

  // Guest/incognito profiles cannot use Nearby Connections.
  if (profile->IsOffTheRecord()) {
    return false;
  }

  // Likewise, kiosk users are ineligible.
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return false;
  }

  // Nearby Connections is not supported for secondary profiles.
  return ProfileHelper::IsPrimaryProfile(profile);
}

// static
NearbyProcessManagerFactory* NearbyProcessManagerFactory::GetInstance() {
  return base::Singleton<NearbyProcessManagerFactory>::get();
}

// static
void NearbyProcessManagerFactory::SetBypassPrimaryUserCheckForTesting(
    bool bypass_primary_user_check_for_testing) {
  g_bypass_primary_user_check_for_testing =
      bypass_primary_user_check_for_testing;
}

NearbyProcessManagerFactory::NearbyProcessManagerFactory()
    : ProfileKeyedServiceFactory("NearbyProcessManager",
                                 ProfileSelections::BuildForAllProfiles()) {
  DependsOn(NearbyDependenciesProviderFactory::GetInstance());
}

NearbyProcessManagerFactory::~NearbyProcessManagerFactory() = default;

KeyedService* NearbyProcessManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // The service is meant to be a singleton, since multiple simultaneous process
  // managers could interfere with each other. Provide access only to the
  // primary user.
  if (CanBeLaunchedForProfile(profile) ||
      g_bypass_primary_user_check_for_testing) {
    return NearbyProcessManagerImpl::Factory::Create(
               NearbyDependenciesProviderFactory::GetForProfile(profile))
        .release();
  }

  return nullptr;
}

bool NearbyProcessManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace nearby
}  // namespace ash
