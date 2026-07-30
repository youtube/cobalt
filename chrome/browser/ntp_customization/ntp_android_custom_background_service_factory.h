// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class NtpAndroidCustomBackgroundService;
class Profile;

namespace content {
class BrowserContext;
}

// Factory for creating NtpAndroidCustomBackgroundService instances per Profile.
class NtpAndroidCustomBackgroundServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static NtpAndroidCustomBackgroundService* GetForProfile(Profile* profile);
  static NtpAndroidCustomBackgroundServiceFactory* GetInstance();

  NtpAndroidCustomBackgroundServiceFactory(
      const NtpAndroidCustomBackgroundServiceFactory&) = delete;
  NtpAndroidCustomBackgroundServiceFactory& operator=(
      const NtpAndroidCustomBackgroundServiceFactory&) = delete;

 private:
  friend base::NoDestructor<NtpAndroidCustomBackgroundServiceFactory>;

  NtpAndroidCustomBackgroundServiceFactory();
  ~NtpAndroidCustomBackgroundServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_FACTORY_H_
