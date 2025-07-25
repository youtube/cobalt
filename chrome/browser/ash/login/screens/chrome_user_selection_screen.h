// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CHROME_USER_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CHROME_USER_SELECTION_SCREEN_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"

class AccountId;

namespace ash {

class ChromeUserSelectionScreen
    : public UserSelectionScreen,
      public policy::DeviceLocalAccountPolicyService::Observer {
 public:
  explicit ChromeUserSelectionScreen(DisplayedScreen display_type);

  ChromeUserSelectionScreen(const ChromeUserSelectionScreen&) = delete;
  ChromeUserSelectionScreen& operator=(const ChromeUserSelectionScreen&) =
      delete;

  ~ChromeUserSelectionScreen() override;

  // UserSelectionScreen:
  void Init(const user_manager::UserList& users) override;

  // policy::DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

 private:
  // Check whether the display name set by policy for a public session has
  // changed and if so, notify the UI.
  void CheckForPublicSessionDisplayNameChange(
      policy::DeviceLocalAccountPolicyBroker* broker);

  // Check whether the list of recommended locales set by policy for a public
  // session has changed and if so, notify the UI.
  void CheckForPublicSessionLocalePolicyChange(
      policy::DeviceLocalAccountPolicyBroker* broker);

  // Check whether full management disclosure is needed for the public/managed
  // session login screen and if so, notify the UI.
  void CheckIfFullManagementDisclosureNeeded(
      policy::DeviceLocalAccountPolicyBroker* broker);

  // Notify the UI that the display name for a public session has changed,
  // taking the display name from the `User` owned by `UserManager`.
  void SetPublicSessionDisplayName(const AccountId& account_id);

  // Send an updated list of locales for a public session to the UI, consisting
  // of the `recommended_locales` followed by all other available locales.
  void SetPublicSessionLocales(
      const AccountId& account_id,
      const std::vector<std::string>& recommended_locales);

  // Send an updated flag for enabling/disable full management disclosure to the
  // UI.
  void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure);

  raw_ptr<policy::DeviceLocalAccountPolicyService, ExperimentalAsh>
      device_local_account_policy_service_;

  // Map from public session account IDs to their display names set by policy.
  typedef std::map<AccountId, std::string> DisplayNamesMap;
  DisplayNamesMap public_session_display_names_;

  base::WeakPtrFactory<ChromeUserSelectionScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CHROME_USER_SELECTION_SCREEN_H_
