// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/at_memory_promo_tracker_factory.h"

#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/at_memory_promo_tracker.h"

namespace autofill {

// static
AtMemoryPromoTracker* AtMemoryPromoTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AtMemoryPromoTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
AtMemoryPromoTrackerFactory* AtMemoryPromoTrackerFactory::GetInstance() {
  static base::NoDestructor<AtMemoryPromoTrackerFactory> instance;
  return instance.get();
}

AtMemoryPromoTrackerFactory::AtMemoryPromoTrackerFactory()
    : ProfileKeyedServiceFactory(
          "AtMemoryPromoTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(GoogleGroupsManagerFactory::GetInstance());
}

AtMemoryPromoTrackerFactory::~AtMemoryPromoTrackerFactory() = default;

std::unique_ptr<KeyedService>
AtMemoryPromoTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!autofill::IsAtMemoryFeatureEnabled(
          GoogleGroupsManagerFactory::GetForBrowserContext(context))) {
    return nullptr;
  }
  return std::make_unique<AtMemoryPromoTracker>();
}

}  // namespace autofill
