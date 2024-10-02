// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_TRIGGERED_PROFILE_RESETTER_FACTORY_H_
#define CHROME_BROWSER_PROFILE_RESETTER_TRIGGERED_PROFILE_RESETTER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class TriggeredProfileResetter;

class TriggeredProfileResetterFactory : public ProfileKeyedServiceFactory {
 public:
  static TriggeredProfileResetter* GetForBrowserContext(
      content::BrowserContext* context);
  static TriggeredProfileResetterFactory* GetInstance();

  TriggeredProfileResetterFactory(const TriggeredProfileResetterFactory&) =
      delete;
  TriggeredProfileResetterFactory& operator=(
      const TriggeredProfileResetterFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<TriggeredProfileResetterFactory>;
  friend class TriggeredProfileResetterTest;

  TriggeredProfileResetterFactory();
  ~TriggeredProfileResetterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_TRIGGERED_PROFILE_RESETTER_FACTORY_H_
