// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// static
EntityDataManager* AutofillEntityDataManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<EntityDataManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutofillEntityDataManagerFactory*
AutofillEntityDataManagerFactory::GetInstance() {
  static base::NoDestructor<AutofillEntityDataManagerFactory> instance;
  return instance.get();
}

AutofillEntityDataManagerFactory::AutofillEntityDataManagerFactory()
    : ProfileKeyedServiceFactory(
          "AutofillEntityDataManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(WebDataServiceFactory::GetInstance());
}

AutofillEntityDataManagerFactory::~AutofillEntityDataManagerFactory() = default;

std::unique_ptr<KeyedService>
AutofillEntityDataManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAiWithDataSchema)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<autofill::AutofillWebDataService> local_storage =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!local_storage) {
    // This happens in tests because
    // WebDataServiceFactory::ServiceIsNULLWhileTesting() is true.
    return nullptr;
  }
  return std::make_unique<EntityDataManager>(std::move(local_storage));
}

bool AutofillEntityDataManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace autofill
