// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/at_memory_query_service_factory.h"

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/accessibility_annotator/at_memory_query_service_delegate_impl.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/annotation_reducer/personal_context_resolver_impl.h"
#include "components/accessibility_annotator/core/at_memory_query_service.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/at_memory/autofill_data_provider_impl.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
AtMemoryQueryServiceFactory* AtMemoryQueryServiceFactory::GetInstance() {
  static base::NoDestructor<AtMemoryQueryServiceFactory> instance;
  return instance.get();
}

// static
accessibility_annotator::AtMemoryQueryService*
AtMemoryQueryServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<accessibility_annotator::AtMemoryQueryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

AtMemoryQueryServiceFactory::AtMemoryQueryServiceFactory()
    : ProfileKeyedServiceFactory("AtMemoryQueryService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(autofill::AutofillEntityDataManagerFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(GoogleGroupsManagerFactory::GetInstance());
  DependsOn(PersonalContextServiceFactory::GetInstance());
}

AtMemoryQueryServiceFactory::~AtMemoryQueryServiceFactory() = default;

std::unique_ptr<KeyedService>
AtMemoryQueryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!autofill::IsAtMemoryFeatureEnabled(
          GoogleGroupsManagerFactory::GetForBrowserContext(context))) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<autofill::AutofillDataProviderImpl> data_provider =
      std::make_unique<autofill::AutofillDataProviderImpl>(
          autofill::PersonalDataManagerFactory::GetForBrowserContext(context),
          autofill::AutofillEntityDataManagerFactory::GetForProfile(profile));

  auto* optimization_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

  personal_context::PersonalContextService* personal_context_service =
      PersonalContextServiceFactory::GetForProfile(profile);

  std::unique_ptr<accessibility_annotator::PersonalContextResolver>
      personal_context_resolver;
  if (personal_context_service) {
    personal_context_resolver =
        std::make_unique<accessibility_annotator::PersonalContextResolverImpl>(
            personal_context_service,
            g_browser_process->GetApplicationLocale());
  }

  return std::make_unique<accessibility_annotator::AtMemoryQueryService>(
      std::make_unique<
          accessibility_annotator::AtMemoryQueryServiceDelegateImpl>(profile),
      std::move(data_provider), std::move(personal_context_resolver),
      optimization_guide_service);
}

bool AtMemoryQueryServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}
