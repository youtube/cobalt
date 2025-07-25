// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/credential_storage.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/src/internal/platform/implementation/credential_callbacks.h"

namespace {

const char kManagerAppName[] = "test_manager_app_id";
const char kAccountName[] = "test_account_name";

const unsigned int kPublicCredentialInStorageCount = 1u;
const unsigned int kPrivateCredentialInStorageCount = 1u;

const std::vector<uint8_t> kSecretId_Local = {0x11, 0x12, 0x13,
                                              0x14, 0x15, 0x16};
const std::vector<uint8_t> kSecretId_Shared = {0x12, 0x13, 0x14,
                                               0x15, 0x16, 0x17};

const std::vector<uint8_t> kKeySeed = {
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40};
const std::vector<uint8_t> kEncryptedMetadataBytes = {0x33, 0x33, 0x33,
                                                      0x33, 0x33, 0x33};
const std::vector<uint8_t> kMetadataEncryptionTag = {0x44, 0x44, 0x44,
                                                     0x44, 0x44, 0x44};
const std::vector<uint8_t> kMetadataEncryptionKeyV0 = {
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e};
const std::vector<uint8_t> kConnectionSignatureVerificationKey = {
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
const std::vector<uint8_t> kAdvertisementSignatureVerificationKey = {
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
const std::vector<uint8_t> kVersion = {0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
const char AdvertisementSigningKeyCertificateAlias[] =
    "NearbySharingABCDEF123456";
const std::vector<uint8_t> kAdvertisementPrivateKey = {0x41, 0x42, 0x43,
                                                       0x44, 0x45, 0x46};
const char ConnectionSigningKeyCertificateAlias[] = "NearbySharingXYZ789";
const std::vector<uint8_t> kConnectionPrivateKey = {0x51, 0x52, 0x53,
                                                    0x54, 0x55, 0x56};
const std::vector<uint8_t> kMetadataEncryptionKeyV1 = {
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70};
const base::flat_map<uint32_t, bool> kConsumedSalts = {{0xb412, true},
                                                       {0x34b2, false},
                                                       {0x5171, false}};

constexpr int64_t kStartTimeMillis = 255486129307;
constexpr int64_t kEndtimeMillis = 64301728896;

::nearby::internal::LocalCredential CreateLocalCredentialProto(
    const std::vector<uint8_t>& secret_id,
    const std::vector<uint8_t>& key_seed,
    int64_t start_time_millis,
    const std::vector<uint8_t>& metadata_encryption_key_v0,
    const std::string& advertisement_signing_key_certificate_alias,
    const std::vector<uint8_t>& advertisement_private_key,
    const std::string& connection_signing_key_certificate_alias,
    const std::vector<uint8_t>& connection_private_key,
    const base::flat_map<uint32_t, bool>& consumed_salts,
    const std::vector<uint8_t>& metadata_encryption_key_v1) {
  ::nearby::internal::LocalCredential proto;

  proto.set_secret_id(std::string(secret_id.begin(), secret_id.end()));
  proto.set_key_seed(std::string(key_seed.begin(), key_seed.end()));
  proto.set_start_time_millis(start_time_millis);
  proto.set_metadata_encryption_key_v0(std::string(
      metadata_encryption_key_v0.begin(), metadata_encryption_key_v0.end()));

  proto.mutable_advertisement_signing_key()->set_certificate_alias(
      advertisement_signing_key_certificate_alias);
  proto.mutable_advertisement_signing_key()->set_key(std::string(
      advertisement_private_key.begin(), advertisement_private_key.end()));

  proto.mutable_connection_signing_key()->set_certificate_alias(
      connection_signing_key_certificate_alias);
  proto.mutable_connection_signing_key()->set_key(std::string(
      connection_private_key.begin(), connection_private_key.end()));

  // All local credentials have IdentityType of kIdentityTypePrivate.
  proto.set_identity_type(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE);

  for (const auto& pair : consumed_salts) {
    auto map_pair =
        ::google::protobuf::MapPair<uint32_t, bool>(pair.first, pair.second);
    proto.mutable_consumed_salts()->insert(map_pair);
  }

  proto.set_metadata_encryption_key_v1(std::string(
      metadata_encryption_key_v1.begin(), metadata_encryption_key_v1.end()));

  return proto;
}

::nearby::internal::SharedCredential CreateSharedCredentialProto(
    const std::vector<uint8_t>& secret_id,
    const std::vector<uint8_t>& key_seed,
    int64_t start_time_millis,
    int64_t end_time_millis,
    const std::vector<uint8_t>& encrypted_metadata_btyes,
    const std::vector<uint8_t>& metadata_encryption_key_tag,
    const std::vector<uint8_t>& connection_signature_verification_key,
    const std::vector<uint8_t>& advertisement_signature_verification_key,
    const std::vector<uint8_t>& version) {
  ::nearby::internal::SharedCredential proto;

  proto.set_secret_id(std::string(secret_id.begin(), secret_id.end()));
  proto.set_key_seed(std::string(key_seed.begin(), key_seed.end()));
  proto.set_start_time_millis(start_time_millis);
  proto.set_end_time_millis(end_time_millis);
  proto.set_encrypted_metadata_bytes_v0(std::string(
      encrypted_metadata_btyes.begin(), encrypted_metadata_btyes.end()));
  proto.set_metadata_encryption_key_tag_v0(std::string(
      metadata_encryption_key_tag.begin(), metadata_encryption_key_tag.end()));
  proto.set_connection_signature_verification_key(
      std::string(connection_signature_verification_key.begin(),
                  connection_signature_verification_key.end()));
  proto.set_advertisement_signature_verification_key(
      std::string(advertisement_signature_verification_key.begin(),
                  advertisement_signature_verification_key.end()));
  proto.set_identity_type(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE);
  proto.set_version(std::string(version.begin(), version.end()));

  return proto;
}

ash::nearby::presence::mojom::SharedCredentialPtr CreateSharedCredentialMojo(
    const std::vector<uint8_t>& secret_id,
    const std::vector<uint8_t>& key_seed,
    const int64_t start_time_millis,
    const int64_t end_time_millis,
    const std::vector<uint8_t>& encrypted_metadata_bytes,
    const std::vector<uint8_t>& metadata_encryption_key_tag,
    const std::vector<uint8_t>& connection_signature_verification_key,
    const std::vector<uint8_t>& advertisement_signature_verification_key,
    const ash::nearby::presence::mojom::IdentityType identity_type,
    const std::vector<uint8_t>& version) {
  return ash::nearby::presence::mojom::SharedCredential::New(
      secret_id, key_seed, start_time_millis, end_time_millis,
      encrypted_metadata_bytes, metadata_encryption_key_tag,
      connection_signature_verification_key,
      advertisement_signature_verification_key, identity_type, version);
}

ash::nearby::presence::mojom::LocalCredentialPtr CreateLocalCredential(
    const std::vector<uint8_t>& secret_id,
    const std::vector<uint8_t>& key_seed,
    const int64_t start_time_millis,
    const std::vector<uint8_t>& metadata_encryption_key_v0,
    const std::string& advertisement_signing_key_certificate_alias,
    const std::vector<uint8_t>& advertisement_signing_key_data,
    const std::string& connection_signing_key_certificate_alias,
    const std::vector<uint8_t>& connection_signing_key_data,
    const ash::nearby::presence::mojom::IdentityType identity_type,
    const base::flat_map<uint32_t, bool>& consumed_salts,
    const std::vector<uint8_t>& metadata_encryption_key_v1) {
  auto local_credential = ash::nearby::presence::mojom::LocalCredential::New();

  local_credential->secret_id = secret_id;
  local_credential->key_seed = key_seed;
  local_credential->start_time_millis = start_time_millis;
  local_credential->metadata_encryption_key_v0 = metadata_encryption_key_v0;
  local_credential->identity_type = identity_type;
  local_credential->consumed_salts = consumed_salts;
  local_credential->metadata_encryption_key_v1 = metadata_encryption_key_v1;

  auto advertisement_key = ash::nearby::presence::mojom::PrivateKey::New();
  advertisement_key->certificate_alias =
      advertisement_signing_key_certificate_alias;
  advertisement_key->key = advertisement_signing_key_data;
  local_credential->advertisement_signing_key = std::move(advertisement_key);

  auto connection_key = ash::nearby::presence::mojom::PrivateKey::New();
  connection_key->certificate_alias = connection_signing_key_certificate_alias;
  connection_key->key = connection_signing_key_data;
  local_credential->connection_signing_key = std::move(connection_key);

  return local_credential;
}

void PopulateTestData(
    std::vector<::nearby::internal::LocalCredential>& local_credentials,
    std::vector<::nearby::internal::SharedCredential>& shared_credentials) {
  local_credentials.emplace_back(CreateLocalCredentialProto(
      kSecretId_Local, kKeySeed, kStartTimeMillis, kMetadataEncryptionKeyV0,
      AdvertisementSigningKeyCertificateAlias, kAdvertisementPrivateKey,
      ConnectionSigningKeyCertificateAlias, kConnectionPrivateKey,
      kConsumedSalts, kMetadataEncryptionKeyV1));
  shared_credentials.emplace_back(CreateSharedCredentialProto(
      kSecretId_Shared, kKeySeed, kStartTimeMillis, kEndtimeMillis,
      kEncryptedMetadataBytes, kMetadataEncryptionTag,
      kConnectionSignatureVerificationKey,
      kAdvertisementSignatureVerificationKey, kVersion));
}

const nearby::presence::CredentialSelector& CreateCredentialSelector() {
  static nearby::presence::CredentialSelector credential_selector = {
      kManagerAppName,
      kAccountName,
      nearby::internal::IDENTITY_TYPE_PRIVATE,
  };
  return credential_selector;
}

class FakeNearbyPresenceCredentialStorage
    : public ash::nearby::presence::mojom::NearbyPresenceCredentialStorage {
 public:
  FakeNearbyPresenceCredentialStorage() = default;
  ~FakeNearbyPresenceCredentialStorage() override = default;

  // mojom::NearbyPresenceCredentialStorage:
  void SaveCredentials(
      std::vector<ash::nearby::presence::mojom::LocalCredentialPtr>
          local_credentials,
      std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
          shared_credentials,
      ash::nearby::presence::mojom::PublicCredentialType public_credential_type,
      ash::nearby::presence::mojom::NearbyPresenceCredentialStorage::
          SaveCredentialsCallback callback) override {
    if (should_credentials_successfully_save_) {
      std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kOk);
    } else {
      std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kUnknown);
    }
  }

  // Note: Returns 1 credential in the vector.
  void GetPublicCredentials(
      ash::nearby::presence::mojom::PublicCredentialType public_credential_type,
      GetPublicCredentialsCallback callback) override {
    if (!should_public_credentials_successfully_retrieve_) {
      std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kUnknown,
                              absl::nullopt);
      return;
    }

    std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
        shared_credentials;
    shared_credentials.emplace_back(CreateSharedCredentialMojo(
        kSecretId_Shared, kKeySeed, kStartTimeMillis, kEndtimeMillis,
        kEncryptedMetadataBytes, kMetadataEncryptionTag,
        kConnectionSignatureVerificationKey,
        kAdvertisementSignatureVerificationKey,
        ash::nearby::presence::mojom::IdentityType::kIdentityTypePrivate,
        kVersion));
    // The constant must be changed if more shared credentials are added to
    // the vector.
    ASSERT_EQ(kPublicCredentialInStorageCount, shared_credentials.size());

    std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kOk,
                            std::move(shared_credentials));
  }

  void GetPrivateCredentials(GetPrivateCredentialsCallback callback) override {
    if (!should_private_credentials_successfully_retrieve_) {
      std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kUnknown,
                              absl::nullopt);
      return;
    }

    std::vector<ash::nearby::presence::mojom::LocalCredentialPtr>
        local_credentials;
    local_credentials.emplace_back(CreateLocalCredential(
        kSecretId_Local, kKeySeed, kStartTimeMillis, kMetadataEncryptionKeyV0,
        AdvertisementSigningKeyCertificateAlias, kAdvertisementPrivateKey,
        ConnectionSigningKeyCertificateAlias, kConnectionPrivateKey,
        ash::nearby::presence::mojom::IdentityType::kIdentityTypePrivate,
        kConsumedSalts, kMetadataEncryptionKeyV1));
    // The constant must be changed if more local credentials are added to
    // the vector.
    ASSERT_EQ(kPrivateCredentialInStorageCount, local_credentials.size());

    std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kOk,
                            std::move(local_credentials));
  }

  void SetShouldCredentialsSuccessfullySave(bool should_succeed) {
    should_credentials_successfully_save_ = should_succeed;
  }

  void SetShouldPublicCredentialsSuccessfullyRetrieve(bool should_succed) {
    should_public_credentials_successfully_retrieve_ = should_succed;
  }

  void SetShouldPrivateCredentialsSuccessfullyRetrieve(bool should_succed) {
    should_private_credentials_successfully_retrieve_ = should_succed;
  }

 private:
  bool should_credentials_successfully_save_ = true;
  bool should_public_credentials_successfully_retrieve_ = true;
  bool should_private_credentials_successfully_retrieve_ = true;
};

}  // namespace

namespace nearby::chrome {

class CredentialStorageTest : public testing::Test {
 public:
  CredentialStorageTest() = default;

  ~CredentialStorageTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto fake_credential_storage =
        std::make_unique<FakeNearbyPresenceCredentialStorage>();
    fake_credential_storage_ = fake_credential_storage.get();

    mojo::PendingRemote<
        ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
        pending_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_credential_storage),
        pending_remote.InitWithNewPipeAndPassReceiver());

    remote_credential_storage_.Bind(std::move(pending_remote),
                                    /*bind_task_runner=*/nullptr);

    credential_storage_ =
        std::make_unique<CredentialStorage>(remote_credential_storage_);
  }

  void TearDown() override {
    // Prevent dangling raw pointer once 'remote_credential_storage_' is
    // deconstructed.
    fake_credential_storage_ = nullptr;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::SharedRemote<
      ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
      remote_credential_storage_;
  std::unique_ptr<CredentialStorage> credential_storage_;
  raw_ptr<FakeNearbyPresenceCredentialStorage> fake_credential_storage_;
};

TEST_F(CredentialStorageTest, Initialize) {
  EXPECT_TRUE(credential_storage_);
}

TEST_F(CredentialStorageTest, SaveCredentials_LocalShared_Succeed) {
  std::vector<::nearby::internal::LocalCredential> local_credentials;
  std::vector<::nearby::internal::SharedCredential> shared_credentials;
  PopulateTestData(local_credentials, shared_credentials);

  base::RunLoop run_loop;

  nearby::presence::SaveCredentialsResultCallback callback;
  callback.credentials_saved_cb =
      absl::AnyInvocable<void(absl::Status)>([&](const absl::Status& status) {
        EXPECT_TRUE(status.ok());
        run_loop.Quit();
      });

  fake_credential_storage_->SetShouldCredentialsSuccessfullySave(
      /*should_succeed=*/true);
  credential_storage_->SaveCredentials(
      kManagerAppName, kAccountName, local_credentials, shared_credentials,
      ::nearby::presence::PublicCredentialType::kLocalPublicCredential,
      std::move(callback));

  run_loop.Run();
}

TEST_F(CredentialStorageTest, SaveCredentials_RemoteShared_Succeed) {
  std::vector<::nearby::internal::LocalCredential> local_credentials;
  std::vector<::nearby::internal::SharedCredential> shared_credentials;
  PopulateTestData(local_credentials, shared_credentials);

  base::RunLoop run_loop;

  nearby::presence::SaveCredentialsResultCallback callback;
  callback.credentials_saved_cb =
      absl::AnyInvocable<void(absl::Status)>([&](const absl::Status& status) {
        EXPECT_TRUE(status.ok());
        run_loop.Quit();
      });

  fake_credential_storage_->SetShouldCredentialsSuccessfullySave(
      /*should_succeed=*/true);
  credential_storage_->SaveCredentials(
      kManagerAppName, kAccountName, local_credentials, shared_credentials,
      ::nearby::presence::PublicCredentialType::kRemotePublicCredential,
      std::move(callback));

  run_loop.Run();
}

TEST_F(CredentialStorageTest, SaveCredentials_LocalShared_Fail) {
  std::vector<::nearby::internal::LocalCredential> local_credentials;
  std::vector<::nearby::internal::SharedCredential> shared_credentials;
  PopulateTestData(local_credentials, shared_credentials);

  base::RunLoop run_loop;

  nearby::presence::SaveCredentialsResultCallback callback;
  callback.credentials_saved_cb =
      absl::AnyInvocable<void(absl::Status)>([&](const absl::Status& status) {
        EXPECT_FALSE(status.ok());
        run_loop.Quit();
      });

  fake_credential_storage_->SetShouldCredentialsSuccessfullySave(
      /*should_succeed=*/false);
  credential_storage_->SaveCredentials(
      kManagerAppName, kAccountName, local_credentials, shared_credentials,
      ::nearby::presence::PublicCredentialType::kLocalPublicCredential,
      std::move(callback));

  run_loop.Run();
}

TEST_F(CredentialStorageTest, SaveCredentials_RemoteShared_Fail) {
  std::vector<::nearby::internal::LocalCredential> local_credentials;
  std::vector<::nearby::internal::SharedCredential> shared_credentials;
  PopulateTestData(local_credentials, shared_credentials);

  base::RunLoop run_loop;

  nearby::presence::SaveCredentialsResultCallback callback;
  callback.credentials_saved_cb =
      absl::AnyInvocable<void(absl::Status)>([&](const absl::Status& status) {
        EXPECT_FALSE(status.ok());
        run_loop.Quit();
      });

  fake_credential_storage_->SetShouldCredentialsSuccessfullySave(
      /*should_succeed=*/false);
  credential_storage_->SaveCredentials(
      kManagerAppName, kAccountName, local_credentials, shared_credentials,
      ::nearby::presence::PublicCredentialType::kRemotePublicCredential,
      std::move(callback));

  run_loop.Run();
}

TEST_F(CredentialStorageTest, GetPublicCredentials_Local_Success) {
  base::RunLoop run_loop;

  credential_storage_->GetPublicCredentials(
      CreateCredentialSelector(),
      ::nearby::presence::PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb = [&](auto status_or_creds) {
        ASSERT_TRUE(status_or_creds.ok());

        auto& creds = *status_or_creds;
        EXPECT_EQ(creds.size(), kPublicCredentialInStorageCount);

        run_loop.Quit();
      }});

  run_loop.Run();
}

TEST_F(CredentialStorageTest, GetPublicCredentials_Local_Fail) {
  base::RunLoop run_loop;

  fake_credential_storage_->SetShouldPublicCredentialsSuccessfullyRetrieve(
      /*should_succeed=*/false);
  credential_storage_->GetPublicCredentials(
      CreateCredentialSelector(),
      ::nearby::presence::PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb = [&](auto status_or_creds) {
        EXPECT_FALSE(status_or_creds.ok());
        run_loop.Quit();
      }});

  run_loop.Run();
}

TEST_F(CredentialStorageTest, GetPublicCredentials_Remote_Success) {
  base::RunLoop run_loop;

  credential_storage_->GetPublicCredentials(
      CreateCredentialSelector(),
      ::nearby::presence::PublicCredentialType::kRemotePublicCredential,
      {.credentials_fetched_cb = [&](auto status_or_creds) {
        ASSERT_TRUE(status_or_creds.ok());

        auto& creds = *status_or_creds;
        EXPECT_EQ(creds.size(), kPublicCredentialInStorageCount);

        run_loop.Quit();
      }});

  run_loop.Run();
}

TEST_F(CredentialStorageTest, GetPublicCredentials_Remote_Fail) {
  base::RunLoop run_loop;

  fake_credential_storage_->SetShouldPublicCredentialsSuccessfullyRetrieve(
      /*should_succeed=*/false);
  credential_storage_->GetPublicCredentials(
      CreateCredentialSelector(),
      ::nearby::presence::PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb = [&](auto status_or_creds) {
        EXPECT_FALSE(status_or_creds.ok());
        run_loop.Quit();
      }});

  run_loop.Run();
}

TEST_F(CredentialStorageTest, GetLocalCredentials_Success) {
  base::RunLoop run_loop;

  credential_storage_->GetLocalCredentials(
      CreateCredentialSelector(),
      {.credentials_fetched_cb = [&](auto status_or_creds) {
        ASSERT_TRUE(status_or_creds.ok());

        auto& creds = *status_or_creds;
        EXPECT_EQ(creds.size(), kPrivateCredentialInStorageCount);

        run_loop.Quit();
      }});

  run_loop.Run();
}

TEST_F(CredentialStorageTest, GetLocalCredentials_Fail) {
  base::RunLoop run_loop;

  fake_credential_storage_->SetShouldPrivateCredentialsSuccessfullyRetrieve(
      /*should_succeed=*/false);
  credential_storage_->GetLocalCredentials(
      CreateCredentialSelector(),
      {.credentials_fetched_cb = [&](auto status_or_creds) {
        EXPECT_FALSE(status_or_creds.ok());
        run_loop.Quit();
      }});

  run_loop.Run();
}

}  // namespace nearby::chrome
