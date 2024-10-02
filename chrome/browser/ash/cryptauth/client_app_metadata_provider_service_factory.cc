// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cryptauth/client_app_metadata_provider_service_factory.h"

#include "chrome/browser/ash/cryptauth/client_app_metadata_provider_service.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/network_handler.h"

namespace ash {

// static
ClientAppMetadataProviderService*
ClientAppMetadataProviderServiceFactory::GetForProfile(Profile* profile) {
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<ClientAppMetadataProviderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ClientAppMetadataProviderServiceFactory*
ClientAppMetadataProviderServiceFactory::GetInstance() {
  return base::Singleton<ClientAppMetadataProviderServiceFactory>::get();
}

ClientAppMetadataProviderServiceFactory::
    ClientAppMetadataProviderServiceFactory()
    : ProfileKeyedServiceFactory(
          "ClientAppMetadataProviderService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
}

ClientAppMetadataProviderServiceFactory::
    ~ClientAppMetadataProviderServiceFactory() = default;

KeyedService* ClientAppMetadataProviderServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return new ClientAppMetadataProviderService(
      profile->GetPrefs(), NetworkHandler::Get()->network_state_handler(),
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile));
}

}  // namespace ash
