// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_BACKGROUND_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_BACKGROUND_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class NtpBackgroundService;
class Profile;

namespace content {
class BrowserContext;
}

// Factory for creating NtpBackgroundService instances per Profile on Android.
class NtpAndroidBackgroundServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the NtpBackgroundService for |profile|.
  static NtpBackgroundService* GetForProfile(Profile* profile);
  static NtpAndroidBackgroundServiceFactory* GetInstance();

  NtpAndroidBackgroundServiceFactory(
      const NtpAndroidBackgroundServiceFactory&) = delete;
  NtpAndroidBackgroundServiceFactory& operator=(
      const NtpAndroidBackgroundServiceFactory&) = delete;

 private:
  friend base::NoDestructor<NtpAndroidBackgroundServiceFactory>;

  NtpAndroidBackgroundServiceFactory();
  ~NtpAndroidBackgroundServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_BACKGROUND_SERVICE_FACTORY_H_
