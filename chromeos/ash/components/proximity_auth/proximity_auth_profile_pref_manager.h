// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/account_id/account_id.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace proximity_auth {

// Implementation of ProximityAuthPrefManager for a logged in session with a
// user profile.
class ProximityAuthProfilePrefManager
    : public ProximityAuthPrefManager,
      public ash::multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  // Creates a pref manager backed by preferences registered in
  // |pref_service| (persistent across browser restarts). |pref_service| should
  // have been registered using RegisterPrefs(). Not owned, must out live this
  // instance.
  ProximityAuthProfilePrefManager(
      PrefService* pref_service,
      ash::multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);

  ProximityAuthProfilePrefManager(const ProximityAuthProfilePrefManager&) =
      delete;
  ProximityAuthProfilePrefManager& operator=(
      const ProximityAuthProfilePrefManager&) = delete;

  ~ProximityAuthProfilePrefManager() override;

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // ProximityAuthPrefManager:
  bool IsEasyUnlockAllowed() const override;
  void SetIsEasyUnlockEnabled(bool is_easy_unlock_enabled) const override;
  bool IsEasyUnlockEnabled() const override;
  void SetEasyUnlockEnabledStateSet() const override;
  bool IsEasyUnlockEnabledStateSet() const override;
  void SetLastPromotionCheckTimestampMs(int64_t timestamp_ms) override;
  int64_t GetLastPromotionCheckTimestampMs() const override;
  void SetPromotionShownCount(int count) override;
  int GetPromotionShownCount() const override;

  // ash::multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const ash::multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

 private:
  // Contains perferences that outlive the lifetime of this object and across
  // process restarts. Not owned and must outlive this instance.
  raw_ptr<PrefService, ExperimentalAsh> pref_service_ = nullptr;

  // The account id of the current profile.
  AccountId account_id_;

  // Used to determine the FeatureState of Smart Lock.
  raw_ptr<ash::multidevice_setup::MultiDeviceSetupClient, ExperimentalAsh>
      multidevice_setup_client_ = nullptr;

  base::WeakPtrFactory<ProximityAuthProfilePrefManager> weak_ptr_factory_{this};
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_
