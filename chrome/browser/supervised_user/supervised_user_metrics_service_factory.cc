// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_metrics_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "content/public/browser/browser_context.h"

// static
supervised_user::SupervisedUserMetricsService*
SupervisedUserMetricsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<supervised_user::SupervisedUserMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
SupervisedUserMetricsServiceFactory*
SupervisedUserMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserMetricsServiceFactory> factory;
  return factory.get();
}

SupervisedUserMetricsServiceFactory::SupervisedUserMetricsServiceFactory()
    : ProfileKeyedServiceFactory("SupervisedUserMetricsServiceFactory",
                                 ProfileSelections::BuildForRegularProfile()) {
  // Used for tracking web filter metrics.
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

SupervisedUserMetricsServiceFactory::~SupervisedUserMetricsServiceFactory() =
    default;

void SupervisedUserMetricsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  supervised_user::SupervisedUserMetricsService::RegisterProfilePrefs(registry);
}

KeyedService* SupervisedUserMetricsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new supervised_user::SupervisedUserMetricsService(
      profile->GetPrefs(),
      SupervisedUserServiceFactory::GetForProfile(profile)->GetURLFilter());
}

bool SupervisedUserMetricsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool SupervisedUserMetricsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
