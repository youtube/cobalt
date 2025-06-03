// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_CELLULAR_PREF_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_CELLULAR_PREF_HANDLER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "components/prefs/pref_service.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

class NetworkStateHandler;

// This class provides the ability to store and query prefs for managed cellular
// networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ManagedCellularPrefHandler {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when a ICCID - SMDP address is added or removed.
    virtual void OnManagedCellularPrefChanged() = 0;
  };

  ManagedCellularPrefHandler();
  ManagedCellularPrefHandler(const ManagedCellularPrefHandler&) = delete;
  ManagedCellularPrefHandler& operator=(const ManagedCellularPrefHandler&) =
      delete;
  virtual ~ManagedCellularPrefHandler();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void Init(NetworkStateHandler* network_state_handler);
  void SetDevicePrefs(PrefService* device_prefs);

  // Add a new ICCID and SMDP+ address pair to device pref for a managed
  // cellular network. If |sync_stub_networks| is set true,
  // NetworkStateHandler::SyncStubCellularNetworks() will be called.
  void AddIccidSmdpPair(const std::string& iccid,
                        const std::string& smdp_address,
                        bool sync_stub_networks = true);

  // Remove the ICCID and SMDP+ address pair from the device pref with given
  // |iccid|.
  void RemovePairWithIccid(const std::string& iccid);

  // Persistes the eSIM metadata for a managed cellular network to device prefs.
  // If |sync_stub_networks| is set true,
  // NetworkStateHandler::SyncStubCellularNetworks() will be called.
  void AddESimMetadata(const std::string& iccid,
                       const std::string& name,
                       const policy_util::SmdxActivationCode& activation_code,
                       bool sync_stub_networks = true);

  // Returns the persisted eSIM metadata that corresponds to ICCID |iccid|, if
  // it exists, otherwise returns |nullptr|.
  const base::Value::Dict* GetESimMetadata(const std::string& iccid);

  // Removes the persisted eSIM metadata that corresponds to ICCID |iccid|.
  void RemoveESimMetadata(const std::string& iccid);

  // Marks cellular network with iccid |iccid| as migrated to the APN revamp
  // feature. See (b/162365553).
  virtual void AddApnMigratedIccid(const std::string& iccid);

  // Return true if the |iccid| has been migrated to the APN Revamp feature.
  virtual bool ContainsApnMigratedIccid(const std::string& iccid) const;

  // Returns the corresponding SMDP+ address for the given |iccid|. Returns
  // nullptr if no such |iccid| is found.
  const std::string* GetSmdpAddressFromIccid(const std::string& iccid) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  // This change migrates the existing prefs, the ICCID and SM-DP+ pairs, to the
  // new eSIM metadata format that includes the ICCID, SM-DX activation code,
  // and name of the network as provided by policy. Since the previous format
  // did not contain a name the migrated entries will not contain a name until
  // subsequent policy application. This will overwrite existing entries and
  // should only be called once.
  void MigrateExistingPrefs();

  void NotifyManagedCellularPrefChanged();

  raw_ptr<NetworkStateHandler, ExperimentalAsh> network_state_handler_ =
      nullptr;

  // Initialized to null and set once SetDevicePrefs() is called.
  raw_ptr<PrefService, ExperimentalAsh> device_prefs_ = nullptr;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_CELLULAR_PREF_HANDLER_H_
