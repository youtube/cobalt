// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace ash {

class KioskCrashRestoreTest : public MixinBasedInProcessBrowserTest,
                              public LocalStateMixin::Delegate {
 public:
  KioskCrashRestoreTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()) {}
  KioskCrashRestoreTest(const KioskCrashRestoreTest&) = delete;
  KioskCrashRestoreTest& operator=(const KioskCrashRestoreTest&) = delete;

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override { SetUpExistingKioskApp(); }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    // Override device policy.
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        owner_key_util_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());

    // SessionManagerClient will be destroyed in ChromeBrowserMain.
    SessionManagerClient::InitializeFakeInMemory();
    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_.GetBlob());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    const AccountId account_id = AccountId::FromUserEmail(GetTestAppUserId());
    const cryptohome::AccountIdentifier cryptohome_id =
        cryptohome::CreateAccountIdentifierFromAccountId(account_id);

    command_line->AppendSwitchASCII(switches::kLoginUser,
                                    cryptohome_id.account_id());
    command_line->AppendSwitchASCII(
        switches::kLoginProfile,
        UserDataAuthClient::GetStubSanitizedUsername(cryptohome_id));
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  virtual const std::string GetTestAppUserId() const = 0;

 private:
  void SetUpExistingKioskApp() {
    // Create policy data that contains the test app as an existing kiosk app.
    KioskAppsMixin::AppendKioskAccount(&device_policy_.payload());
    KioskAppsMixin::AppendWebKioskAccount(&device_policy_.payload());
    device_policy_.Build();

    // Prepare the policy data to store in device policy cache.
    enterprise_management::PolicyData policy_data;
    CHECK(device_policy_.payload().SerializeToString(
        policy_data.mutable_policy_value()));
    const std::string policy_data_string = policy_data.SerializeAsString();
    std::string encoded;
    base::Base64Encode(policy_data_string, &encoded);

    // Store policy data and existing device local accounts in local state.
    g_browser_process->local_state()->SetString(prefs::kDeviceSettingsCache,
                                                encoded);

    base::Value::List accounts;
    accounts.Append(GetTestAppUserId());
    g_browser_process->local_state()->SetList("PublicAccounts",
                                              std::move(accounts));
  }

  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;

  EmbeddedTestServerSetupMixin embedded_test_server_{&mixin_host_,
                                                     embedded_test_server()};
  KioskAppsMixin kiosk_apps_{&mixin_host_, embedded_test_server()};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

class ChromeKioskCrashRestoreTest : public KioskCrashRestoreTest {
  const std::string GetTestAppUserId() const override {
    return policy::GenerateDeviceLocalAccountUserId(
        KioskAppsMixin::kEnterpriseKioskAccountId,
        policy::DeviceLocalAccount::TYPE_KIOSK_APP);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeKioskCrashRestoreTest, ChromeAppNotInstalled) {
  // If app is not installed when restoring from crash, the kiosk launch is
  // expected to fail, as in that case the crash occured during the app
  // initialization - before the app was actually launched.
  EXPECT_EQ(KioskAppLaunchError::Error::kUnableToLaunch,
            KioskAppLaunchError::Get());
}

class WebKioskCrashRestoreTest : public KioskCrashRestoreTest {
  const std::string GetTestAppUserId() const override {
    return policy::GenerateDeviceLocalAccountUserId(
        KioskAppsMixin::kEnterpriseWebKioskAccountId,
        policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP);
  }
};

IN_PROC_BROWSER_TEST_F(WebKioskCrashRestoreTest, WebKioskLaunches) {
  // If app is not installed when restoring from crash, the kiosk launch is
  // expected to fail, as in that case the crash occured during the app
  // initialization - before the app was actually launched.
  EXPECT_EQ(KioskAppLaunchError::Error::kNone, KioskAppLaunchError::Get());
  KioskSessionInitializedWaiter().Wait();
}

}  // namespace ash
