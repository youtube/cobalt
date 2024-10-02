// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_policies_utils.h"

#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/features.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/sync/base/sync_prefs.h"

namespace browsing_data {

namespace policy_data_types {
// Data retention policy types that require sync to be disabled.
extern const char kBrowsingHistory[] = "browsing_history";
extern const char kPasswordSignin[] = "password_signin";
extern const char kAutofill[] = "autofill";
extern const char kSiteSettings[] = "site_settings";
// Data retention policy types that do not require sync to be disabled.
extern const char kHostedAppData[] = "hosted_app_data";
extern const char kDownloadHistory[] = "download_history";
extern const char kCookiesAndOtherSiteData[] = "cookies_and_other_site_data";
extern const char kCachedImagesAndFiles[] = "cached_images_and_files";
}  // namespace policy_data_types

namespace {

// The format of the log message shown in chrome://policy/logs when sync types
// are automatically disabled.
constexpr char kDisabledSyncTypesLogFormat[] =
    "The policy %s automatically disabled the following sync types: %s";

// Adds the sync type for the `browsing_data_type` to  `sync_types` if it
// should be disabled.
void AppendSyncTypesIfRequired(const base::Value& browsing_data_type,
                               syncer::UserSelectableTypeSet* sync_types) {
  // Map of browsing data types to sync types that need to be disabled for
  // them.
  static const auto kDataToSyncTypesMap =
      base::MakeFixedFlatMap<std::string, syncer::UserSelectableTypeSet>(
          {{browsing_data::policy_data_types::kBrowsingHistory,
            {syncer::UserSelectableType::kHistory,
             syncer::UserSelectableType::kTabs,
             syncer::UserSelectableType::kSavedTabGroups}},
           {browsing_data::policy_data_types::kPasswordSignin,
            {syncer::UserSelectableType::kPasswords}},
           {browsing_data::policy_data_types::kSiteSettings,
            {syncer::UserSelectableType::kPreferences}},
           {browsing_data::policy_data_types::kAutofill,
            {syncer::UserSelectableType::kAutofill}},
           {browsing_data::policy_data_types::kDownloadHistory, {}},
           {browsing_data::policy_data_types::kCookiesAndOtherSiteData, {}},
           {browsing_data::policy_data_types::kCachedImagesAndFiles, {}},
           {browsing_data::policy_data_types::kHostedAppData, {}}});

  // When a new sync type or browsing data type is introduced in the code,
  // kDataToSyncTypesMap should be updated if needed to ensure that browsing
  // data that can be cleared by policy is not already synced across devices.
  static_assert(static_cast<int>(syncer::UserSelectableType::kLastType) == 11,
                "It looks like a sync type was added or removed. Please update "
                "`kDataToSyncTypesMap` value maps above if it affects any of "
                "the browsing data types.");

  static_assert(
      static_cast<int>(browsing_data::BrowsingDataType::NUM_TYPES) ==
          static_cast<int>(kDataToSyncTypesMap.size()) + 1,
      "It looks like a browsing data type was added or removed. Please "
      "update `kDataToSyncTypesMap` above to include the new type and the sync "
      "types it maps to if this data is synced.");

  auto* it = kDataToSyncTypesMap.find(browsing_data_type.GetString());
  if (it == kDataToSyncTypesMap.end()) {
    return;
  }
  for (const syncer::UserSelectableType sync_type_needed : it->second) {
    sync_types->Put(sync_type_needed);
  }
}

}  // namespace

syncer::UserSelectableTypeSet GetSyncTypesForClearBrowsingData(
    const base::Value& policy_value) {
  // The use of GetList() without type checking is safe because this
  // function is only called if the policy schema is valid.
  const auto& items = policy_value.GetList();
  syncer::UserSelectableTypeSet sync_types;
  for (const auto& item : items) {
    AppendSyncTypesIfRequired(item, &sync_types);
  }
  return sync_types;
}

syncer::UserSelectableTypeSet GetSyncTypesForBrowsingDataLifetime(
    const base::Value& policy_value) {
  // The use of GetList() and GetDict() without type checking are safe because
  // this function is only called if the policy schema is valid.
  const auto& items = policy_value.GetList();
  syncer::UserSelectableTypeSet sync_types;
  for (const auto& item : items) {
    const base::Value* data_types = item.GetDict().Find("data_types");
    for (const auto& type : data_types->GetList()) {
      AppendSyncTypesIfRequired(type, &sync_types);
    }
  }
  return sync_types;
}

void DisableSyncTypes(const syncer::UserSelectableTypeSet& types_set,
                      PrefValueMap* prefs,
                      const std::string& policy_name,
                      std::string& log_message) {
  for (const syncer::UserSelectableType type_name : types_set) {
    const char* pref = syncer::SyncPrefs::GetPrefNameForType(type_name);
    if (pref) {
      prefs->SetValue(pref, base::Value(false));
    }
  }
  log_message =
      types_set.Size() > 0
          ? base::StringPrintf(kDisabledSyncTypesLogFormat, policy_name.c_str(),
                               UserSelectableTypeSetToString(types_set).c_str())
          : std::string();
}

bool IsPolicyDependencyEnabled() {
  // Check that FeatureList is available as a protection against early startup
  // crashes. Some policy providers are initialized very early even before
  // base::FeatureList is available, but when policies are finally applied, the
  // feature stack is fully initialized. The instance check ensures that the
  // final decision is delayed until all features are initalized, without any
  // other downstream effect.
  return base::FeatureList::GetInstance() &&
         base::FeatureList::IsEnabled(
             features::kDataRetentionPoliciesDisableSyncTypesNeeded);
}

}  // namespace browsing_data
