// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "content/public/browser/storage_partition.h"

BulkLeakCheckServiceFactory::BulkLeakCheckServiceFactory()
    : ProfileKeyedServiceFactory(
          "PasswordBulkLeakCheck",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

BulkLeakCheckServiceFactory::~BulkLeakCheckServiceFactory() = default;

// static
BulkLeakCheckServiceFactory* BulkLeakCheckServiceFactory::GetInstance() {
  static base::NoDestructor<BulkLeakCheckServiceFactory> instance;
  return instance.get();
}

// static
password_manager::BulkLeakCheckServiceInterface*
BulkLeakCheckServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<password_manager::BulkLeakCheckServiceInterface*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* BulkLeakCheckServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new password_manager::BulkLeakCheckService(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
