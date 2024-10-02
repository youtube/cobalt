// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/auth_error_observer_factory.h"

#include "chrome/browser/ash/login/signin/auth_error_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"

namespace ash {

AuthErrorObserverFactory::AuthErrorObserverFactory()
    : ProfileKeyedServiceFactory(
          "AuthErrorObserver",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SigninErrorControllerFactory::GetInstance());
}

AuthErrorObserverFactory::~AuthErrorObserverFactory() = default;

// static
AuthErrorObserver* AuthErrorObserverFactory::GetForProfile(Profile* profile) {
  if (!AuthErrorObserver::ShouldObserve(profile))
    return nullptr;

  return static_cast<AuthErrorObserver*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AuthErrorObserverFactory* AuthErrorObserverFactory::GetInstance() {
  return base::Singleton<AuthErrorObserverFactory>::get();
}

KeyedService* AuthErrorObserverFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new AuthErrorObserver(profile);
}

}  // namespace ash
