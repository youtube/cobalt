// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_CLEANER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_CLEANER_H_

#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/sync/base/model_type.h"

class PrefService;

namespace autofill {

class PersonalDataManager;

// PersonalDataManagerCleaner is responsible for applying address and credit
// card cleanups once on browser startup provided that the sync is enabled or
// when the sync starts.
class PersonalDataManagerCleaner {
 public:
  PersonalDataManagerCleaner(
      PersonalDataManager* personal_data_manager,
      AlternativeStateNameMapUpdater* alternative_state_name_map_updater,
      PrefService* pref_service);
  ~PersonalDataManagerCleaner();
  PersonalDataManagerCleaner(const PersonalDataManagerCleaner&) = delete;
  PersonalDataManagerCleaner& operator=(const PersonalDataManagerCleaner&) =
      delete;

  // Applies address and credit card fixes and cleanups if the sync is enabled.
  // Also, logs address, credit card and offer startup metrics.
  void CleanupDataAndNotifyPersonalDataObservers();

  // Applies address/credit card fixes and cleanups depending on the
  // |model_type|.
  void ApplyAddressAndCardFixesAndCleanups(syncer::ModelType model_type);

  // TODO(crbug.com/1477292): Remove.
  void SyncStarted(syncer::ModelType model_type);

#if defined(UNIT_TEST)
  // Getter for |alternative_state_name_map_updater_| used for testing purposes.
  AlternativeStateNameMapUpdater*
  alternative_state_name_map_updater_for_testing() {
    return alternative_state_name_map_updater_;
  }
#endif  // defined(UNIT_TEST)

 protected:
  friend class PersonalDataManagerCleanerTest;

 private:
  // Applies various fixes and cleanups on autofill addresses.
  void ApplyAddressFixesAndCleanups();

  // Applies various fixes and cleanups on autofill credit cards.
  void ApplyCardFixesAndCleanups();

  // Removes settings-inaccessible profiles values from all profiles stored in
  // the |personal_data_manager_|.
  void RemoveInaccessibleProfileValues();

  // Applies the deduping routine once per major version if the feature is
  // enabled. Calls DedupeProfiles with the content of
  // |PersonalDataManager::GetProfiles()| as a parameter. Removes the profiles
  // to delete from the database and updates the others.
  // Returns true if the routine was run.
  bool ApplyDedupingRoutine();

  // Goes through all the |existing_profiles| and merges all similar unverified
  // profiles together. Also discards unverified profiles that are similar to a
  // verified profile. All the profiles except the results of the merges will be
  // added to |profile_guids_to_delete|. This routine should be run once per
  // major version.
  //
  // This method should only be called by ApplyDedupingRoutine. It is split for
  // testing purposes.
  void DedupeProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
      std::unordered_set<std::string>* profile_guids_to_delete) const;

  // Tries to delete disused addresses once per major version if the
  // feature is enabled.
  bool DeleteDisusedAddresses();

  // Tries to delete disused credit cards once per major version if the
  // feature is enabled.
  bool DeleteDisusedCreditCards();

  // Clears the value of the origin field of cards that were not created from
  // the settings page.
  void ClearCreditCardNonSettingsOrigins();

  // True if autofill profile dedupe needs to be performed.
  bool is_autofill_profile_dedupe_pending_ = true;

  // True if the profile or credit card cleanups need to be performed.
  bool is_profile_cleanup_pending_ = true;
  bool is_credit_card_cleanup_pending_ = true;

  // The personal data manager, used to load and update the personal data
  // from/to the web database.
  const raw_ptr<PersonalDataManager> personal_data_manager_ = nullptr;

  // The PrefService used by this instance.
  const raw_ptr<PrefService> pref_service_ = nullptr;

  // The AlternativeStateNameMapUpdater, used to populate
  // AlternativeStateNameMap with the geographical state data.
  const raw_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_ = nullptr;

  // Profiles are loaded through two sync bridges, the AUTOFILL_PROFILE and the
  // CONTACT_INFO sync bridge. Both of them are behind the same settings toggle,
  // so it is guaranteed that either both of them or none of them are activated
  // together. In order to initialize the alternative state map only after
  // profiles from both sources were loaded, we track whether each model type's
  // `SyncStarted()` event has triggered yet.
  // TODO(crbug.com/1348294): Remove once the AUTOFILL_PROFILE sync bridge is
  // deprecated.
  bool autofill_profile_sync_started_ = false;
  bool contact_info_sync_started_ = false;

  // base::WeakPtr ensures that the callback bound to the object is canceled
  // when that object is destroyed.
  base::WeakPtrFactory<PersonalDataManagerCleaner> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_CLEANER_H_
