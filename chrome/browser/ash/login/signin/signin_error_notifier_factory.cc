// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/signin_error_notifier_factory.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"

namespace ash {

SigninErrorNotifierFactory::SigninErrorNotifierFactory()
    : ProfileKeyedServiceFactory(
          "SigninErrorNotifier",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(SigninErrorControllerFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

SigninErrorNotifierFactory::~SigninErrorNotifierFactory() {}

// static
SigninErrorNotifier* SigninErrorNotifierFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninErrorNotifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninErrorNotifierFactory* SigninErrorNotifierFactory::GetInstance() {
  return base::Singleton<SigninErrorNotifierFactory>::get();
}

KeyedService* SigninErrorNotifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // If this is during dummy login from tests, suppress the notification.
  if (switches::IsGaiaServicesDisabled())
    return nullptr;

  Profile* profile = static_cast<Profile*>(context);
  return new SigninErrorNotifier(
      SigninErrorControllerFactory::GetForProfile(profile), profile);
}

}  // namespace ash
