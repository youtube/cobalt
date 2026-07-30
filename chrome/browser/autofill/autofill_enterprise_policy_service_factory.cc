// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_enterprise_policy_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/permissions/enterprise_policy/autofill_enterprise_policy_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// static
AutofillEnterprisePolicyService*
AutofillEnterprisePolicyServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AutofillEnterprisePolicyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutofillEnterprisePolicyServiceFactory*
AutofillEnterprisePolicyServiceFactory::GetInstance() {
  static base::NoDestructor<AutofillEnterprisePolicyServiceFactory> instance;
  return instance.get();
}

AutofillEnterprisePolicyServiceFactory::AutofillEnterprisePolicyServiceFactory()
    : ProfileKeyedServiceFactory(
          "AutofillEnterprisePolicyService",
          ProfileSelections::BuildRedirectedInIncognito()) {}

AutofillEnterprisePolicyServiceFactory::
    ~AutofillEnterprisePolicyServiceFactory() = default;

std::unique_ptr<KeyedService>
AutofillEnterprisePolicyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AutofillEnterprisePolicyService>(profile->GetPrefs());
}

}  // namespace autofill
