// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_policy_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/permissions/autofill_policy_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// static
AutofillPolicyService* AutofillPolicyServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutofillPolicyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutofillPolicyServiceFactory* AutofillPolicyServiceFactory::GetInstance() {
  static base::NoDestructor<AutofillPolicyServiceFactory> instance;
  return instance.get();
}

AutofillPolicyServiceFactory::AutofillPolicyServiceFactory()
    : ProfileKeyedServiceFactory(
          "AutofillPolicyService",
          ProfileSelections::BuildRedirectedInIncognito()) {}

AutofillPolicyServiceFactory::~AutofillPolicyServiceFactory() = default;

std::unique_ptr<KeyedService>
AutofillPolicyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AutofillPolicyService>(profile->GetPrefs());
}

}  // namespace autofill
