// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

const std::string kUserEmail = "test.tester@gmail.com";
const std::string kCanocalizedUserEmail = "testtester@gmail.com";
const std::string kGivenName = "Test";
const std::string kUserName = "Test Tester";
const std::string kProfileUrl = "https://example.com";
const std::vector<uint8_t> kSecretId1 = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
const std::vector<uint8_t> kSecretId2 = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22};
const std::vector<uint8_t> kSecretId3 = {0x33, 0x33, 0x33, 0x33, 0x33, 0x33};
const std::vector<uint8_t> kSecretId4 = {0x44, 0x44, 0x44, 0x44, 0x44, 0x44};
const std::vector<uint8_t> kSecretId5 = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
const std::vector<uint8_t> kSecretId6 = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66};

}  // namespace

namespace ash::nearby::presence {

class LocalDeviceDataProviderImplTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterNearbyPresenceCredentialPrefs(pref_service_.registry());
    account_info_ = identity_test_env_.MakePrimaryAccountAvailable(
        kUserEmail, signin::ConsentLevel::kSignin);
  }

  void CreateDataProvider() {
    local_device_data_provider_ = std::make_unique<LocalDeviceDataProviderImpl>(
        &pref_service_, identity_test_env_.identity_manager());
  }

  void DestroyDataProvider() { local_device_data_provider_.reset(); }

 protected:
  AccountInfo account_info_;
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider_;
};

TEST_F(LocalDeviceDataProviderImplTest, DeviceId) {
  CreateDataProvider();

  // A 10-character alphanumeric ID is automatically generated if one doesn't
  // already exist.
  std::string id = local_device_data_provider_->GetDeviceId();
  EXPECT_EQ(10u, id.size());
  for (const char c : id) {
    EXPECT_TRUE(absl::ascii_isalnum(static_cast<unsigned char>(c)));
  }

  // The ID is persisted.
  DestroyDataProvider();
  CreateDataProvider();
  EXPECT_EQ(id, local_device_data_provider_->GetDeviceId());
}

TEST_F(LocalDeviceDataProviderImplTest, AccountName) {
  CreateDataProvider();
  EXPECT_EQ(kCanocalizedUserEmail,
            local_device_data_provider_->GetAccountName());
}

TEST_F(LocalDeviceDataProviderImplTest, SaveUserRegistrationInfo) {
  CreateDataProvider();

  // Simulate first time registration has occurred.
  local_device_data_provider_->SaveUserRegistrationInfo(kUserName, kProfileUrl);
  local_device_data_provider_->SetRegistrationComplete(/*complete=*/true);

  EXPECT_TRUE(
      local_device_data_provider_->IsRegistrationCompleteAndUserInfoSaved());
}

TEST_F(LocalDeviceDataProviderImplTest, Metadata) {
  CreateDataProvider();

  // Simulate first time registration has occurred.
  local_device_data_provider_->SaveUserRegistrationInfo(kUserName, kProfileUrl);

  // Assume that without the given name, the device name is just the
  // device type.
  ::nearby::internal::Metadata metadata =
      local_device_data_provider_->GetDeviceMetadata();
  EXPECT_EQ(base::UTF16ToUTF8(ui::GetChromeOSDeviceName()),
            metadata.device_name());

  // Populate the account information and expect the device name to include the
  // user's given name.
  account_info_.given_name = kGivenName;
  identity_test_env_.UpdateAccountInfoForAccount(account_info_);
  metadata = local_device_data_provider_->GetDeviceMetadata();
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_NEARBY_PRESENCE_DEVICE_NAME,
                                      base::UTF8ToUTF16(kGivenName),
                                      ui::GetChromeOSDeviceName()),
            metadata.device_name());

  // Confirm the other fields are expected.
  EXPECT_EQ(kCanocalizedUserEmail, metadata.account_name());
  EXPECT_EQ(kUserName, metadata.user_name());
  EXPECT_EQ(kProfileUrl, metadata.device_profile_url());
  EXPECT_EQ(::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS,
            metadata.device_type());
  EXPECT_EQ(std::string(), metadata.bluetooth_mac_address());
}

TEST_F(LocalDeviceDataProviderImplTest, PersistCredentialIds) {
  CreateDataProvider();

  // Mock a list of shared credentials. These credentials can be empty except
  // for the secret id field for unit test purposes since only the secret id is
  // persisted and checked for changes.
  ::nearby::internal::SharedCredential shared_credential1;
  shared_credential1.set_secret_id(
      std::string(kSecretId1.begin(), kSecretId1.end()));
  ::nearby::internal::SharedCredential shared_credential2;
  shared_credential2.set_secret_id(
      std::string(kSecretId2.begin(), kSecretId2.end()));
  ::nearby::internal::SharedCredential shared_credential3;
  shared_credential3.set_secret_id(
      std::string(kSecretId3.begin(), kSecretId3.end()));

  // Persist the list of shared credentials ids, and expect that the same list
  // passed to `HavePublicCredentialsChanged` returns false.
  local_device_data_provider_->UpdatePersistedSharedCredentials(
      {shared_credential1, shared_credential2, shared_credential3});
  EXPECT_FALSE(local_device_data_provider_->HaveSharedCredentialsChanged(
      {shared_credential1, shared_credential2, shared_credential3}));

  // Send in a changed list of shared credential ids to
  // `HavePublicCredentialsChanged` and expect it returns true.
  ::nearby::internal::SharedCredential shared_credential4;
  shared_credential4.set_secret_id(
      std::string(kSecretId4.begin(), kSecretId4.end()));
  ::nearby::internal::SharedCredential shared_credential5;
  shared_credential5.set_secret_id(
      std::string(kSecretId5.begin(), kSecretId5.end()));
  ::nearby::internal::SharedCredential shared_credential6;
  shared_credential6.set_secret_id(
      std::string(kSecretId6.begin(), kSecretId6.end()));
  EXPECT_TRUE(local_device_data_provider_->HaveSharedCredentialsChanged(
      {shared_credential4, shared_credential5, shared_credential6}));

  // Send in a changed list of shared credential ids with one removed, and
  // expect it to return true.
  EXPECT_TRUE(local_device_data_provider_->HaveSharedCredentialsChanged(
      {shared_credential1, shared_credential2}));

  // Send in a changed list of shared credential ids with one added, and
  // expect it to return true.
  EXPECT_TRUE(local_device_data_provider_->HaveSharedCredentialsChanged(
      {shared_credential1, shared_credential2, shared_credential3,
       shared_credential4}));
}

}  // namespace ash::nearby::presence
