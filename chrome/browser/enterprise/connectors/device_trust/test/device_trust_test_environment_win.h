// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_WIN_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_WIN_H_

#include "base/test/test_reg_util_win.h"
#include "base/threading/thread.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/scoped_key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_test_environment.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"

namespace enterprise_connectors {

class DeviceTrustTestEnvironmentWin : public DeviceTrustTestEnvironment,
                                      public KeyRotationCommandFactory {
 public:
  DeviceTrustTestEnvironmentWin();
  ~DeviceTrustTestEnvironmentWin() override;

  // KeyRotationCommandFactory:
  std::unique_ptr<KeyRotationCommand> CreateCommand(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  // DeviceTrustTestEnvironment:
  void SetUpExistingKey() override;

  // DeviceTrustTestEnvironment:
  std::vector<uint8_t> GetWrappedKey() override;

 private:
  // RegistryOverrideManager for testing with registry
  registry_util::RegistryOverrideManager registry_override_manager_;

  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;

  // Used to fake that the browser was a system-level installation.
  install_static::ScopedInstallDetails install_details_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_TEST_DEVICE_TRUST_TEST_ENVIRONMENT_WIN_H_
