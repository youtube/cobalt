// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_ENTERPRISE_POLICY_TEST_HELPER_H_
#define IOS_CHROME_BROWSER_POLICY_ENTERPRISE_POLICY_TEST_HELPER_H_

#include <memory>

#include "base/files/file_path.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"

class BrowserPolicyConnectorIOS;
class BrowserStatePolicyConnector;
class PrefService;
class TestChromeBrowserState;

// Builds the boilerplate enterprise policy configuration and creates a browser
// state configured with that policy.
class EnterprisePolicyTestHelper {
 public:
  // Creates a new instance using `state_directory_path` as storage for the
  // backing state.
  explicit EnterprisePolicyTestHelper(
      const base::FilePath& state_directory_path);
  ~EnterprisePolicyTestHelper();

  // Returns the browser state configured with `policy_provider_`.
  TestChromeBrowserState* GetBrowserState() const;
  // Returns the backing local state.
  PrefService* GetLocalState();
  // Returns the policy provider attached to `browser_state_`.
  policy::MockConfigurationPolicyProvider* GetPolicyProvider();
  // Returns the browser policy connector.
  BrowserPolicyConnectorIOS* GetBrowserPolicyConnector();

 private:
  // The enterprise configuration policy provider.
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  // The application-level policy connector. Must outlive `local_state_`.
  std::unique_ptr<BrowserPolicyConnectorIOS> browser_policy_connector_;
  // The local state PrefService managed by policy.
  std::unique_ptr<PrefService> local_state_;
  // The BrowserState-level policy connector. Must outlive `pref_service_`.
  std::unique_ptr<BrowserStatePolicyConnector> browser_state_policy_connector_;
  // The browser state configured with the `policy_provider_`.
  std::unique_ptr<TestChromeBrowserState> browser_state_;

  EnterprisePolicyTestHelper& operator=(const EnterprisePolicyTestHelper&) =
      delete;
  EnterprisePolicyTestHelper(const EnterprisePolicyTestHelper&) = delete;
};

#endif  // IOS_CHROME_BROWSER_POLICY_ENTERPRISE_POLICY_TEST_HELPER_H_
