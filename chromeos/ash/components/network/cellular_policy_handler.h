// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_POLICY_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_POLICY_HANDLER_H_

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace ash {

class CellularESimInstaller;
class CellularInhibitor;
class NetworkProfileHandler;
class NetworkStateHandler;
class ManagedCellularPrefHandler;
class ManagedNetworkConfigurationHandler;
enum class HermesResponseStatus;

// TODO(b/281904820): Remove this old description.
// Handles provisioning eSIM profiles via policy.
//
// When installing policy eSIM profiles, the activation code is constructed from
// the SM-DP+ address in the policy configuration. Install requests are queued
// and installation is performed one by one. Install attempts are retried for
// fixed number of tries and the request queue doesn't get blocked by the
// requests that are waiting for retry attempt.

// This class encapsulates the logic for installing eSIM profiles configured by
// policy. Installation requests are added to a queue, and each request will be
// retried a fixed number of times with a retry delay between each attempt.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularPolicyHandler
    : public HermesManagerClient::Observer,
      public CellularESimProfileHandler::Observer,
      public NetworkStateHandlerObserver {
 public:
  CellularPolicyHandler();
  CellularPolicyHandler(const CellularPolicyHandler&) = delete;
  CellularPolicyHandler& operator=(const CellularPolicyHandler&) = delete;
  ~CellularPolicyHandler() override;

  void Init(CellularESimProfileHandler* cellular_esim_profile_handler,
            CellularESimInstaller* cellular_esim_installer,
            CellularInhibitor* cellular_inhibitor,
            NetworkProfileHandler* network_profile_handler,
            NetworkStateHandler* network_state_handler,
            ManagedCellularPrefHandler* managed_cellular_pref_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler);

  // TODO(b/281904820): Remove this function once SM-DS Support is launched.
  // Installs an eSIM profile and connects to its network from policy with
  // given |smdp_address|. The Shill service configuration will also be updated
  // to the policy guid and the new ICCID after installation completes. If
  // another eSIM profile is already under installation process, the current
  // request will wait until the previous one is completed. Each installation
  // will be retried for a fixed number of tries.
  void InstallESim(const std::string& smdp_address,
                   const base::Value::Dict& onc_config);

  // Installs the policy eSIM profile defined in |onc_config|. The Shill service
  // configuration will be updated to match the GUID provided by |onc_config|
  // and to include the ICCID of the installed profile. Installations are
  // performed using a queue, and each installation will be retried a fix number
  // of times.
  void InstallESim(const base::Value::Dict& onc_config);

 private:
  // This enum allows us to treat a retry differently depending on what the
  // reason for retrying is.
  enum class InstallRetryReason {
    kMissingNonCellularConnectivity = 0,
    kInternalError = 1,
    kUserError = 2,
    kOther = 3,
  };

  // HermesUserErrorCodes indicate errors made by the user. These can be due
  // to bad input or a valid input that has already been successfully processed.
  // In such errors, we do not attempt to retry.
  const std::array<HermesResponseStatus, 4> kHermesUserErrorCodes = {
      HermesResponseStatus::kErrorAlreadyDisabled,
      HermesResponseStatus::kErrorAlreadyEnabled,
      HermesResponseStatus::kErrorInvalidActivationCode,
      HermesResponseStatus::kErrorInvalidIccid};

  // HermesInternalErrorCodes indicate system failure during the installation
  // process. These error can happen due to code bugs or reasons unrelated to
  // user input. In these cases, we retry using an exponental backoff policy to
  // attempt the installation again.
  const std::array<HermesResponseStatus, 7> kHermesInternalErrorCodes = {
      HermesResponseStatus::kErrorUnknown,
      HermesResponseStatus::kErrorInternalLpaFailure,
      HermesResponseStatus::kErrorWrongState,
      HermesResponseStatus::kErrorSendApduFailure,
      HermesResponseStatus::kErrorUnexpectedModemManagerState,
      HermesResponseStatus::kErrorModemMessageProcessing,
      HermesResponseStatus::kErrorPendingProfile};

  friend class CellularPolicyHandlerTest;

  // Represents policy eSIM install request parameters. Requests are queued and
  // processed one at a time. |activation_code| represents the SM-DP+ activation
  // code that will be used to install the eSIM profile, and |onc_config| is the
  // ONC configuration of the cellular policy.
  struct InstallPolicyESimRequest {
    InstallPolicyESimRequest(policy_util::SmdxActivationCode activation_code,
                             const base::Value::Dict& onc_config);
    InstallPolicyESimRequest(const InstallPolicyESimRequest&) = delete;
    InstallPolicyESimRequest& operator=(const InstallPolicyESimRequest&) =
        delete;
    ~InstallPolicyESimRequest();

    const policy_util::SmdxActivationCode activation_code;
    base::Value::Dict onc_config;
    net::BackoffEntry retry_backoff;
  };

  // HermesManagerClient::Observer:
  void OnAvailableEuiccListChanged() override;

  // CellularESimProfileHandler::Observer:
  void OnESimProfileListUpdated() override;

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;
  void OnShuttingDown() override;

  // These functions implement the functionality necessary to interact with the
  // queue of policy eSIM installation requests.
  void ResumeInstallIfNeeded();
  void ProcessRequests();
  void ScheduleRetryAndProcessRequests(
      std::unique_ptr<InstallPolicyESimRequest> request,
      InstallRetryReason reason);
  void PushRequestAndProcess(std::unique_ptr<InstallPolicyESimRequest> request);
  void PopRequest();
  void PopAndProcessRequests();

  // Attempts to install the first request in the queue. This function is
  // responsible for ensuring that both a cellular device and Hermes are
  // available. Further, this function will also refresh the list of installed
  // eSIM profiles so that we can properly determine whether an eSIM profile has
  // already been installed for the request.
  void AttemptInstallESim();

  // Actually responsible for kicking off the installation process. This
  // function will configure the Shill service that corresponds to the profile
  // that will be installed, and will ensure that we have non-cellular internet
  // connectivity.
  void PerformInstallESim(const dbus::ObjectPath& euicc_path);

  void OnRefreshProfileList(
      const dbus::ObjectPath& euicc_path,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnConfigureESimService(absl::optional<dbus::ObjectPath> service_path);
  void OnInhibitedForRefreshSmdxProfiles(
      const dbus::ObjectPath& euicc_path,
      base::Value::Dict new_shill_properties,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnRefreshSmdxProfiles(
      const dbus::ObjectPath& euicc_path,
      base::Value::Dict new_shill_properties,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      base::TimeTicks start_time,
      HermesResponseStatus status,
      const std::vector<dbus::ObjectPath>& profile_paths);
  void CompleteRefreshSmdxProfiles(
      const dbus::ObjectPath& euicc_path,
      base::Value::Dict new_shill_properties,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      HermesResponseStatus status,
      const std::vector<dbus::ObjectPath>& profile_paths);
  void OnESimProfileInstallAttemptComplete(
      HermesResponseStatus hermes_status,
      absl::optional<dbus::ObjectPath> profile_path,
      absl::optional<std::string> service_path);
  void OnWaitTimeout();

  base::Value::Dict GetNewShillProperties();
  const policy_util::SmdxActivationCode& GetCurrentActivationCode() const;
  absl::optional<dbus::ObjectPath> FindExistingMatchingESimProfile();
  bool HasNonCellularInternetConnectivity();
  InstallRetryReason HermesResponseStatusToRetryReason(
      HermesResponseStatus status) const;

  raw_ptr<CellularESimProfileHandler, ExperimentalAsh>
      cellular_esim_profile_handler_ = nullptr;
  raw_ptr<CellularESimInstaller, ExperimentalAsh> cellular_esim_installer_ =
      nullptr;
  raw_ptr<CellularInhibitor, ExperimentalAsh> cellular_inhibitor_ = nullptr;
  raw_ptr<NetworkProfileHandler, ExperimentalAsh> network_profile_handler_ =
      nullptr;
  raw_ptr<NetworkStateHandler, ExperimentalAsh> network_state_handler_ =
      nullptr;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  raw_ptr<ManagedCellularPrefHandler, DanglingUntriaged | ExperimentalAsh>
      managed_cellular_pref_handler_ = nullptr;
  raw_ptr<ManagedNetworkConfigurationHandler,
          DanglingUntriaged | ExperimentalAsh>
      managed_network_configuration_handler_ = nullptr;

  bool is_installing_ = false;

  // While Hermes is the source of truth for the EUICC state, Chrome maintains a
  // cache of the installed eSIM profiles. To ensure we properly detect when a
  // profile has already been installed for a particular request we force a
  // refresh of the profile cache before each installation.
  bool need_refresh_profile_list_ = true;
  base::circular_deque<std::unique_ptr<InstallPolicyESimRequest>>
      remaining_install_requests_;

  base::OneShotTimer wait_timer_;

  base::ScopedObservation<HermesManagerClient, HermesManagerClient::Observer>
      hermes_observation_{this};
  base::ScopedObservation<CellularESimProfileHandler,
                          CellularESimProfileHandler::Observer>
      cellular_esim_profile_handler_observation_{this};

  base::WeakPtrFactory<CellularPolicyHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_POLICY_HANDLER_H_
