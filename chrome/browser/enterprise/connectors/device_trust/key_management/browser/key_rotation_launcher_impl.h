// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_ROTATION_LAUNCHER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_ROTATION_LAUNCHER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_upload_request.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class BrowserDMTokenStorage;
class DeviceManagementService;
}  // namespace policy

namespace enterprise_connectors {

class KeyRotationLauncherImpl : public KeyRotationLauncher {
 public:
  KeyRotationLauncherImpl(
      policy::BrowserDMTokenStorage* dm_token_storage,
      policy::DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~KeyRotationLauncherImpl() override;

  // KeyRotationLauncher:
  void LaunchKeyRotation(const std::string& nonce,
                         KeyRotationCommand::Callback callback) override;
  void SynchronizePublicKey(const SigningKeyPair& key_pair,
                            SynchronizationCallback callback) override;

 private:
  absl::optional<std::string> GetDmServerUrl();

  void OnUploadRequestCreated(
      SynchronizationCallback callback,
      absl::optional<const KeyUploadRequest> upload_request);
  void OnUploadCompleted(SynchronizationCallback callback, int status_code);

  raw_ptr<policy::BrowserDMTokenStorage> dm_token_storage_;
  raw_ptr<policy::DeviceManagementService> device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  MojoKeyNetworkDelegate network_delegate_;

  std::unique_ptr<KeyRotationCommand> command_;

  // Checker used to validate that non-background tasks should be
  // running on the original sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<KeyRotationLauncherImpl> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_ROTATION_LAUNCHER_IMPL_H_
