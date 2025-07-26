// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"
#include "components/regional_capabilities/regional_capabilities_service.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "components/variations/service/variations_service.h"
#endif

namespace regional_capabilities {

// static
RegionalCapabilitiesService* RegionalCapabilitiesServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<RegionalCapabilitiesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RegionalCapabilitiesServiceFactory*
RegionalCapabilitiesServiceFactory::GetInstance() {
  static base::NoDestructor<RegionalCapabilitiesServiceFactory> instance;
  return instance.get();
}

RegionalCapabilitiesServiceFactory::RegionalCapabilitiesServiceFactory()
    : ProfileKeyedServiceFactory(
          "RegionalCapabilitiesService",
          ProfileSelections::Builder()
              // Scope rationale: Service needed where we have some features
              // or checks that could be dependent on the country or some other
              // profile or device state. OTR profiles should defer to the
              // parent profile.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithSystem(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

RegionalCapabilitiesServiceFactory::~RegionalCapabilitiesServiceFactory() =
    default;

std::unique_ptr<KeyedService>
RegionalCapabilitiesServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto regional_capabilities_service_client =
      std::make_unique<RegionalCapabilitiesServiceClient>(
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
          g_browser_process->variations_service()
#endif
      );

  return std::make_unique<RegionalCapabilitiesService>(
      CHECK_DEREF(profile->GetPrefs()),
      std::move(regional_capabilities_service_client));
}

}  // namespace regional_capabilities
