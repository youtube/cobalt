// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_COOKIE_SETTINGS_FACTORY_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_COOKIE_SETTINGS_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

namespace content_settings {
class CookieSettings;
}

class Profile;

class CookieSettingsFactory : public RefcountedProfileKeyedServiceFactory {
 public:
  // Returns the |CookieSettings| associated with the |profile|.
  //
  // This should only be called on the UI thread.
  static scoped_refptr<content_settings::CookieSettings> GetForProfile(
      Profile* profile);

  static CookieSettingsFactory* GetInstance();

  CookieSettingsFactory(const CookieSettingsFactory&) = delete;
  CookieSettingsFactory& operator=(const CookieSettingsFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<CookieSettingsFactory>;

  CookieSettingsFactory();
  ~CookieSettingsFactory() override;

  // |RefcountedBrowserContextKeyedServiceFactory| methods:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_COOKIE_SETTINGS_FACTORY_H_
