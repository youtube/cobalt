// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_android_background_service_factory.h"

#include <optional>
#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/themes/ntp_background_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
NtpBackgroundService* NtpAndroidBackgroundServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NtpBackgroundService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NtpAndroidBackgroundServiceFactory* NtpAndroidBackgroundServiceFactory::GetInstance() {
  static base::NoDestructor<NtpAndroidBackgroundServiceFactory> instance;
  return instance.get();
}

NtpAndroidBackgroundServiceFactory::NtpAndroidBackgroundServiceFactory()
    : ProfileKeyedServiceFactory(
          "NtpAndroidBackgroundService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

NtpAndroidBackgroundServiceFactory::~NtpAndroidBackgroundServiceFactory() = default;

std::unique_ptr<KeyedService>
NtpAndroidBackgroundServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<NtpBackgroundService>(
      g_browser_process->GetFeatures()->application_locale_storage(),
      Profile::FromBrowserContext(context)->GetURLLoaderFactory());
}
