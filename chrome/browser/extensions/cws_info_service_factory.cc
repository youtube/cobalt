// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/cws_info_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"

namespace extensions {

// static
CWSInfoService* CWSInfoServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<CWSInfoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
CWSInfoServiceFactory* CWSInfoServiceFactory::GetInstance() {
  static base::NoDestructor<CWSInfoServiceFactory> instance;
  return instance.get();
}

CWSInfoServiceFactory::CWSInfoServiceFactory()
    : ProfileKeyedServiceFactory(
          "CWSInfoService",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionManagementFactory::GetInstance());
}

KeyedService* CWSInfoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (base::FeatureList::IsEnabled(kCWSInfoService) == false) {
    return nullptr;
  }
  return new CWSInfoService(Profile::FromBrowserContext(context));
}

bool CWSInfoServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool CWSInfoServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

void CWSInfoServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(prefs::kCWSInfoTimestamp, base::Time());
}

}  // namespace extensions
