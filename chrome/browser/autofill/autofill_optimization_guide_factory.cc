// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_optimization_guide_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// static
AutofillOptimizationGuide* AutofillOptimizationGuideFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutofillOptimizationGuide*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

// static
AutofillOptimizationGuideFactory*
AutofillOptimizationGuideFactory::GetInstance() {
  return base::Singleton<AutofillOptimizationGuideFactory>::get();
}

AutofillOptimizationGuideFactory::AutofillOptimizationGuideFactory()
    : ProfileKeyedServiceFactory(
          "AutofillOptimizationGuide",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              // OptimizationGuideKeyedService is not available if it is a
              // sign-in or lockscreen profile, so we should not build
              // AutofillOptimizationGuide for these profiles either.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

AutofillOptimizationGuideFactory::~AutofillOptimizationGuideFactory() = default;

KeyedService* AutofillOptimizationGuideFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new AutofillOptimizationGuide(
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile));
}

}  // namespace autofill
