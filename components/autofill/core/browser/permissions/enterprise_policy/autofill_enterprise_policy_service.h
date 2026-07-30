// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_ENTERPRISE_POLICY_AUTOFILL_ENTERPRISE_POLICY_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_ENTERPRISE_POLICY_AUTOFILL_ENTERPRISE_POLICY_SERVICE_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class GURL;
class PrefService;

namespace autofill {

// Evaluates the `autofill.types_blocked` enterprise policy.
class AutofillEnterprisePolicyService : public KeyedService {
 public:
  explicit AutofillEnterprisePolicyService(PrefService* prefs);
  AutofillEnterprisePolicyService(const AutofillEnterprisePolicyService&) =
      delete;
  AutofillEnterprisePolicyService& operator=(
      const AutofillEnterprisePolicyService&) = delete;
  ~AutofillEnterprisePolicyService() override;

  // Returns true if the specified Autofill data category is blocked by the
  // Enterprise policy for the given `url`.
  [[nodiscard]] bool IsAutofillTypeBlockedByPolicy(
      const GURL& url,
      AutofillClient::AutofillPolicyDataCategory category) const;

 private:
  void OnAutofillPolicyChanged();

  const base::raw_ref<PrefService> prefs_;
  PrefChangeRegistrar autofill_types_blocked_change_registrar_;

  struct BlockedPatternEntry {
    ContentSettingsPattern pattern;
    std::vector<AutofillClient::AutofillPolicyDataCategory> blocked_categories;
  };
  std::vector<BlockedPatternEntry> blocked_patterns_cache_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_ENTERPRISE_POLICY_AUTOFILL_ENTERPRISE_POLICY_SERVICE_H_
