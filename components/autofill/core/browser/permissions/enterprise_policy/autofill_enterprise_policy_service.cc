// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/enterprise_policy/autofill_enterprise_policy_service.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace autofill {


AutofillEnterprisePolicyService::AutofillEnterprisePolicyService(
    PrefService* prefs)
    : prefs_(CHECK_DEREF(prefs)) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy)) {
    autofill_types_blocked_change_registrar_.Init(&prefs_.get());
    autofill_types_blocked_change_registrar_.Add(
        prefs::kAutofillTypesBlocked,
        base::BindRepeating(
            &AutofillEnterprisePolicyService::OnAutofillPolicyChanged,
            base::Unretained(this)));
    OnAutofillPolicyChanged();
  }
}

AutofillEnterprisePolicyService::~AutofillEnterprisePolicyService() = default;

bool AutofillEnterprisePolicyService::IsAutofillTypeBlockedByPolicy(
    const GURL& url,
    AutofillClient::AutofillPolicyDataCategory category) const {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy)) {
    return false;
  }

  return std::ranges::any_of(
      blocked_patterns_cache_, [&](const BlockedPatternEntry& entry) {
        return entry.pattern.Matches(url) &&
               std::ranges::find(entry.blocked_categories, category) !=
                   entry.blocked_categories.end();
      });
}

void AutofillEnterprisePolicyService::OnAutofillPolicyChanged() {
  blocked_patterns_cache_.clear();
  const base::ListValue& blocked_list =
      prefs_->GetList(prefs::kAutofillTypesBlocked);
  for (const base::Value& entry : blocked_list) {
    if (!entry.is_dict()) {
      continue;
    }

    const base::DictValue& entry_dict = entry.GetDict();
    const std::string* pattern_str =
        entry_dict.FindString(prefs::kAutofillBlockedTypesUrlPatternKey);
    const base::ListValue* blocked_types =
        entry_dict.FindList(prefs::kAutofillBlockedTypesBlockedTypesKey);

    if (!pattern_str || !blocked_types) {
      continue;
    }

    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(*pattern_str);
    if (!pattern.IsValid()) {
      continue;
    }

    std::vector<AutofillClient::AutofillPolicyDataCategory> categories;
    for (const base::Value& blocked_type : *blocked_types) {
      if (!blocked_type.is_string()) {
        continue;
      }
      const std::string& type_str = blocked_type.GetString();
      if (type_str == prefs::kAutofillBlockedTypesContactInfoValue) {
        categories.push_back(
            AutofillClient::AutofillPolicyDataCategory::kContactInfo);
      } else if (type_str == prefs::kAutofillBlockedTypesPaymentsValue) {
        categories.push_back(
            AutofillClient::AutofillPolicyDataCategory::kPayments);
      } else if (type_str == prefs::kAutofillBlockedTypesIdentityDocsValue) {
        categories.push_back(
            AutofillClient::AutofillPolicyDataCategory::kIdentityDocs);
      } else if (type_str == prefs::kAutofillBlockedTypesTravelValue) {
        categories.push_back(
            AutofillClient::AutofillPolicyDataCategory::kTravel);
      } else if (type_str == prefs::kAutofillBlockedTypesShoppingValue) {
        categories.push_back(
            AutofillClient::AutofillPolicyDataCategory::kShopping);
      }
    }
    blocked_patterns_cache_.push_back({pattern, std::move(categories)});
  }
}

}  // namespace autofill
