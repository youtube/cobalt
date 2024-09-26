// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
namespace unified_consent {
class UnifiedConsentService;
}

class UnifiedConsentServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of UnifiedConsentService associated with |profile|
  // (creating one if none exists). Returns nullptr if this profile cannot have
  // a UnifiedConsentService (e.g. |profile| is an off-the-record profile, a
  // Guest, System, other irregular profile or sync is disabled for |profile|).
  static unified_consent::UnifiedConsentService* GetForProfile(
      Profile* profile);

  // Returns an instance of the factory singleton.
  static UnifiedConsentServiceFactory* GetInstance();

  UnifiedConsentServiceFactory(const UnifiedConsentServiceFactory&) = delete;
  UnifiedConsentServiceFactory& operator=(const UnifiedConsentServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<UnifiedConsentServiceFactory>;

  UnifiedConsentServiceFactory();
  ~UnifiedConsentServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_FACTORY_H_
