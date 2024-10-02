// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation_certificate_generator_impl.h"
#include <iterator>
#include <memory>
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/fake_soft_bind_attestation_flow.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_device_sync_test_devices.h"
#include "chromeos/ash/services/device_sync/fake_ecies_encryption.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::phonehub {

using testing::_;

class MockCryptAuthKeyRegistry : public device_sync::CryptAuthKeyRegistry {
 public:
  MockCryptAuthKeyRegistry()
      : key(device_sync::CryptAuthKey(
            device_sync::GetLocalDeviceForTest()
                .better_together_device_metadata->public_key(),
            device_sync::GetPrivateKeyFromPublicKeyForTest(
                device_sync::GetLocalDeviceForTest()
                    .better_together_device_metadata->public_key()),
            device_sync::CryptAuthKey::Status::kActive,
            cryptauthv2::KeyType::P256,
            device_sync::kCryptAuthFixedUserKeyPairHandle)) {}
  ~MockCryptAuthKeyRegistry() override = default;

  const device_sync::CryptAuthKey* GetActiveKey(
      device_sync::CryptAuthKeyBundle::Name name) const override {
    return &key;
  }

  void OnKeyRegistryUpdated() override {}

 private:
  device_sync::CryptAuthKey key;
};

class FakeCryptAuthKeyRegistryFactory
    : public device_sync::CryptAuthKeyRegistryImpl::Factory {
 public:
  FakeCryptAuthKeyRegistryFactory() = default;

  ~FakeCryptAuthKeyRegistryFactory() override = default;

  MockCryptAuthKeyRegistry* instance() { return instance_; }

 private:
  // CryptAuthKeyRegistryImpl::Factory:
  std::unique_ptr<device_sync::CryptAuthKeyRegistry> CreateInstance(
      PrefService* pref_service) override {
    // Only one instance is expected to be created per test.
    EXPECT_FALSE(instance_);
    auto instance = std::make_unique<MockCryptAuthKeyRegistry>();
    instance_ = instance.get();
    return instance;
  }

  MockCryptAuthKeyRegistry* instance_ = nullptr;
};

class AttestationCertificateGeneratorImplTest : public testing::Test {
 public:
  AttestationCertificateGeneratorImplTest()
      : profile_manager_(CreateTestingProfileManager()),
        profile_(profile_manager_->CreateTestingProfile("test")) {
    device_sync::CryptAuthKeyRegistryImpl::Factory::SetFactoryForTesting(
        &mock_cryptauth_key_factory_);
    user_manager_ = new FakeChromeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager_.get()));
    user_manager_->AddUser(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    auto soft_bind_attestation_flow =
        std::make_unique<attestation::FakeSoftBindAttestationFlow>();
    mock_attestation_flow_ = soft_bind_attestation_flow.get();
    attestation_certificate_generator_impl_ =
        std::make_unique<AttestationCertificateGeneratorImpl>(
            profile_, std::move(soft_bind_attestation_flow));
  }

  AttestationCertificateGeneratorImplTest(
      const AttestationCertificateGeneratorImplTest&) = delete;
  AttestationCertificateGeneratorImplTest& operator=(
      const AttestationCertificateGeneratorImplTest&) = delete;
  ~AttestationCertificateGeneratorImplTest() override = default;

  void OnCertificateGenerated(base::RunLoop* wait_loop,
                              const std::vector<std::string>& certs,
                              bool valid) {
    is_valid_ = valid;
    certs_ = &certs;
    wait_loop->Quit();
  }

 protected:
  const std::vector<std::string> false_certs_ = {"cert"};
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AttestationCertificateGeneratorImpl>
      attestation_certificate_generator_impl_;
  FakeCryptAuthKeyRegistryFactory mock_cryptauth_key_factory_;
  bool is_valid_ = false;
  raw_ptr<const std::vector<std::string>, ExperimentalAsh> certs_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  const raw_ptr<TestingProfile, ExperimentalAsh> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  raw_ptr<FakeChromeUserManager, ExperimentalAsh> user_manager_;
  raw_ptr<attestation::FakeSoftBindAttestationFlow, ExperimentalAsh>
      mock_attestation_flow_;

 private:
  std::unique_ptr<TestingProfileManager> CreateTestingProfileManager() {
    auto profile_manager = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profile_manager->SetUp());
    return profile_manager;
  }
};

TEST_F(AttestationCertificateGeneratorImplTest,
       RetrieveCertificateWithoutCache) {
  mock_attestation_flow_->SetCertificates(false_certs_);
  base::RunLoop wait_loop;
  // Reset the timestamp to force regenerate certificate.
  attestation_certificate_generator_impl_
      ->last_attestation_certificate_generated_time_ = base::Time();
  attestation_certificate_generator_impl_->RetrieveCertificate(base::BindOnce(
      &AttestationCertificateGeneratorImplTest::OnCertificateGenerated,
      base::Unretained(this), &wait_loop));
  wait_loop.Run();
  EXPECT_TRUE(is_valid_);
  EXPECT_EQ(*certs_, false_certs_);
}

TEST_F(AttestationCertificateGeneratorImplTest, RetrieveCertificateWithCache) {
  mock_attestation_flow_->SetCertificates(false_certs_);
  base::RunLoop wait_loop;
  attestation_certificate_generator_impl_->RetrieveCertificate(base::BindOnce(
      &AttestationCertificateGeneratorImplTest::OnCertificateGenerated,
      base::Unretained(this), &wait_loop));
  wait_loop.Run();
  // Used cached result generated in constructor.
  EXPECT_FALSE(is_valid_);
  EXPECT_EQ(*certs_, std::vector<std::string>());
}

}  // namespace ash::phonehub
