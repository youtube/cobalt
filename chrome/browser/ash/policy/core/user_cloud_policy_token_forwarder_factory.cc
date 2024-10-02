// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/user_cloud_policy_token_forwarder_factory.h"

#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_token_forwarder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace policy {

// static
UserCloudPolicyTokenForwarderFactory*
UserCloudPolicyTokenForwarderFactory::GetInstance() {
  return base::Singleton<UserCloudPolicyTokenForwarderFactory>::get();
}

UserCloudPolicyTokenForwarderFactory::UserCloudPolicyTokenForwarderFactory()
    : ProfileKeyedServiceFactory(
          "UserCloudPolicyTokenForwarder",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

UserCloudPolicyTokenForwarderFactory::~UserCloudPolicyTokenForwarderFactory() {}

KeyedService* UserCloudPolicyTokenForwarderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  UserCloudPolicyManagerAsh* manager = profile->GetUserCloudPolicyManagerAsh();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!manager || !identity_manager)
    return nullptr;
  return new UserCloudPolicyTokenForwarder(manager, identity_manager);
}

bool UserCloudPolicyTokenForwarderFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Create this object when the profile is created so it fetches the token
  // during startup.
  return true;
}

bool UserCloudPolicyTokenForwarderFactory::ServiceIsNULLWhileTesting() const {
  // This is only needed in a handful of tests that manually create the object.
  return true;
}

}  // namespace policy
