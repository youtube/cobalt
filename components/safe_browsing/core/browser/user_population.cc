// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/user_population.h"

#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/unified_consent/pref_names.h"
#include "components/version_info/version_info.h"

namespace safe_browsing {

ChromeUserPopulation::UserPopulation GetUserPopulationPref(PrefService* prefs) {
  if (prefs) {
    if (IsEnhancedProtectionEnabled(*prefs)) {
      return ChromeUserPopulation::ENHANCED_PROTECTION;
    } else if (IsExtendedReportingEnabled(*prefs)) {
      return ChromeUserPopulation::EXTENDED_REPORTING;
    } else if (IsSafeBrowsingEnabled(*prefs)) {
      return ChromeUserPopulation::SAFE_BROWSING;
    }
  }

  return ChromeUserPopulation::UNKNOWN_USER_POPULATION;
}

ChromeUserPopulation GetUserPopulation(
    PrefService* prefs,
    bool is_incognito,
    bool is_history_sync_enabled,
    bool is_signed_in,
    bool is_under_advanced_protection,
    const policy::BrowserPolicyConnector* browser_policy_connector,
    absl::optional<size_t> num_profiles,
    absl::optional<size_t> num_loaded_profiles,
    absl::optional<size_t> num_open_profiles) {
  ChromeUserPopulation population;

  population.set_user_population(GetUserPopulationPref(prefs));

  if (prefs) {
    population.set_is_mbb_enabled(prefs->GetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  }

  population.set_is_incognito(is_incognito);

  population.set_is_history_sync_enabled(is_history_sync_enabled);

  population.set_is_under_advanced_protection(is_under_advanced_protection);

  population.set_profile_management_status(
      GetProfileManagementStatus(browser_policy_connector));

  std::string user_agent =
      version_info::GetProductNameAndVersionForUserAgent() + "/" +
      version_info::GetOSType();
  population.set_user_agent(user_agent);

  if (num_profiles)
    population.set_number_of_profiles(*num_profiles);

  if (num_loaded_profiles)
    population.set_number_of_loaded_profiles(*num_loaded_profiles);

  if (num_open_profiles)
    population.set_number_of_open_profiles(*num_open_profiles);

  population.set_is_signed_in(is_signed_in);

  return population;
}

}  // namespace safe_browsing
