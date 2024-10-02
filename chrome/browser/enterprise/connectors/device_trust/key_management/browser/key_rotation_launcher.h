// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_ROTATION_LAUNCHER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_ROTATION_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class BrowserDMTokenStorage;
class DeviceManagementService;
}  // namespace policy

namespace enterprise_connectors {

class SigningKeyPair;

class KeyRotationLauncher {
 public:
  using SynchronizationCallback = base::OnceCallback<void(absl::optional<int>)>;

  static std::unique_ptr<KeyRotationLauncher> Create(
      policy::BrowserDMTokenStorage* dm_token_storage,
      policy::DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~KeyRotationLauncher() = default;

  // Builds a key rotation payload using `nonce`, and then kicks off a key
  // rotation command.
  virtual void LaunchKeyRotation(const std::string& nonce,
                                 KeyRotationCommand::Callback callback) = 0;

  // Verifies if `key_pair`'s public key is known by the management server.
  // Invokes `callback` with the upload code if a request was made.
  virtual void SynchronizePublicKey(const SigningKeyPair& key_pair,
                                    SynchronizationCallback callback) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_ROTATION_LAUNCHER_H_
