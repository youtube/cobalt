// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_DESKTOP_H_

#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/policy/cbcm_invalidations_initializer.h"

class DeviceIdentityProvider;

namespace instance_id {
class InstanceIDDriver;
}

namespace invalidation {
class FCMInvalidationService;
}

namespace policy {
class ChromeBrowserCloudManagementRegisterWatcher;
class CloudPolicyInvalidator;
class RemoteCommandsInvalidator;

// Desktop implementation of the platform-specific operations of CBCMController.
class ChromeBrowserCloudManagementControllerDesktop
    : public ChromeBrowserCloudManagementController::Delegate,
      public CBCMInvalidationsInitializer::Delegate {
 public:
  ChromeBrowserCloudManagementControllerDesktop();
  ChromeBrowserCloudManagementControllerDesktop(
      const ChromeBrowserCloudManagementControllerDesktop&) = delete;
  ChromeBrowserCloudManagementControllerDesktop& operator=(
      const ChromeBrowserCloudManagementControllerDesktop&) = delete;

  ~ChromeBrowserCloudManagementControllerDesktop() override;

  // ChromeBrowserCloudManagementController::Delegate implementation.
  void SetDMTokenStorageDelegate() override;
  int GetUserDataDirKey() override;
  base::FilePath GetExternalPolicyDir() override;
  NetworkConnectionTrackerGetter CreateNetworkConnectionTrackerGetter()
      override;
  void InitializeOAuthTokenFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_state) override;
  void StartWatchingRegistration(
      ChromeBrowserCloudManagementController* controller) override;
  bool WaitUntilPolicyEnrollmentFinished() override;
  bool IsEnterpriseStartupDialogShowing() override;
  void OnServiceAccountSet(CloudPolicyClient* client,
                           const std::string& account_email) override;
  void ShutDown() override;
  MachineLevelUserCloudPolicyManager* GetMachineLevelUserCloudPolicyManager()
      override;
  DeviceManagementService* GetDeviceManagementService() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetBestEffortTaskRunner()
      override;
  std::unique_ptr<enterprise_reporting::ReportingDelegateFactory>
  GetReportingDelegateFactory() override;
  void SetGaiaURLLoaderFactory(scoped_refptr<network::SharedURLLoaderFactory>
                                   url_loader_factory) override;
  bool ReadyToCreatePolicyManager() override;
  bool ReadyToInit() override;
  std::unique_ptr<ClientDataDelegate> CreateClientDataDelegate() override;
  std::unique_ptr<enterprise_connectors::DeviceTrustKeyManager>
  CreateDeviceTrustKeyManager() override;

  // CBCMInvalidationsInitializer::Delegate:
  // Starts the services required for Policy Invalidations over FCM to be
  // enabled.
  void StartInvalidations() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool IsInvalidationsServiceStarted() const override;

 private:
  std::unique_ptr<ChromeBrowserCloudManagementRegisterWatcher>
      cloud_management_register_watcher_;

  // These objects are all involved in Policy Invalidations.
  CBCMInvalidationsInitializer invalidations_initializer_;
  scoped_refptr<network::SharedURLLoaderFactory> gaia_url_loader_factory_;
  std::unique_ptr<DeviceIdentityProvider> identity_provider_;
  std::unique_ptr<instance_id::InstanceIDDriver> device_instance_id_driver_;
  std::unique_ptr<invalidation::FCMInvalidationService> invalidation_service_;
  std::unique_ptr<CloudPolicyInvalidator> policy_invalidator_;

  // This invalidator is responsible for receiving remote commands invalidations
  std::unique_ptr<RemoteCommandsInvalidator> commands_invalidator_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_DESKTOP_H_
