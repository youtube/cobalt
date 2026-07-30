// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"

#include "base/no_destructor.h"
#include "base/test/gtest_util.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr int64_t kLocalCredentialId = 10;
constexpr char kDeviceName[] = "Test's Chromebook";
constexpr char kMacAddress[] = "1A:2B:3C:4D:5E:6F";
constexpr char kDeviceId[] = "DeviceId";
constexpr char kDusi[] = "11";
constexpr char kSignatureVersion[] = "3149642683";
constexpr char AdvertisementSigningKeyCertificateAlias[] =
    "NearbySharingYWJjZGVmZ2hpamtsbW5vcA";
constexpr char ConnectionSigningKeyCertificateAlias[] =
    "NearbySharingCJfjKGVmZ2hpJCtsbC5vDb";
constexpr int64_t kStartTimeMillis = 255486129307;
constexpr int64_t kEndTimeMillis = 64301728896;
constexpr int64_t kSharedCredentialId = 37;

const std::vector<uint8_t>& GetSecretId() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x11, 0x11, 0x11, 0x11, 0x11, 0x11});
  return *val;
}
const std::vector<uint8_t>& GetKeySeed() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x22, 0x22, 0x22, 0x22, 0x22, 0x22});
  return *val;
}
const std::vector<uint8_t>& GetEncryptedMetadataBytesV0() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x33, 0x33, 0x33, 0x33, 0x33, 0x33});
  return *val;
}
const std::vector<uint8_t>& GetMetadataEncryptionTag() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x44, 0x44, 0x44, 0x44, 0x44, 0x44});
  return *val;
}
const std::vector<uint8_t>& GetAdvertisementMetadataEncrpytionKeyV0() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x44, 0x45, 0x46, 0x47, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
       0x44, 0x44});
  return *val;
}
const std::vector<uint8_t>& GetIdentityTokenV1() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x44, 0x45, 0x46, 0x47, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
       0x44, 0x44, 0x44, 0x44});
  return *val;
}
const std::vector<uint8_t>& GetPrivateKey() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x44, 0x44, 0x46, 0x74, 0x44, 0x44});
  return *val;
}
const std::vector<uint8_t>& GetConnectionSignatureVerificationKey() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x55, 0x55, 0x55, 0x55, 0x55, 0x55});
  return *val;
}
const std::vector<uint8_t>& GetAdvertisementSignatureVerificationKey() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x66, 0x66, 0x66, 0x66, 0x66, 0x66});
  return *val;
}
const std::vector<uint8_t>& GetVersion() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x77, 0x77, 0x77, 0x77, 0x77, 0x77});
  return *val;
}
const std::map<uint32_t, bool>& GetConsumedSalts() {
  static const base::NoDestructor<std::map<uint32_t, bool>> val(
      {{0xb412, true}, {0x34b2, false}, {0x5171, false}});
  return *val;
}
const std::vector<uint8_t>& GetEncryptedMetadataBytesV1() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x88, 0x88, 0x88, 0x88, 0x88, 0x88});
  return *val;
}
const std::vector<uint8_t>& GetIdentityTokenShortSaltAdvHmacKeyV1() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA});
  return *val;
}
const std::vector<uint8_t>& GetIdentityTokenExtendedSaltAdvHmacKeyV1() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC});
  return *val;
}
const std::vector<uint8_t>& GetIdentityTokenSignedAdvHmacKeyV1() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD});
  return *val;
}

void SetProtoMap(::google::protobuf::Map<uint32_t, bool>* proto_map,
                 const std::map<uint32_t, bool>& map) {
  for (const auto& element : map) {
    proto_map->insert({element.first, element.second});
  }
}

::nearby::internal::LocalCredential::PrivateKey* CreatePrivateKeyProto(
    std::string certificate_alias,
    std::vector<uint8_t> key) {
  ::nearby::internal::LocalCredential::PrivateKey* private_key_proto =
      new ::nearby::internal::LocalCredential::PrivateKey;
  private_key_proto->set_certificate_alias(std::string(certificate_alias));
  private_key_proto->set_key(std::string(key.begin(), key.end()));
  return private_key_proto;
}

}  // namespace

namespace ash::nearby::presence::proto {

class ProtoConversionsTest : public testing::Test {};

TEST_F(ProtoConversionsTest, BuildMetadata) {
  ::nearby::internal::DeviceIdentityMetaData metadata = BuildMetadata(
      /*device_type=*/::nearby::internal::DeviceType::DEVICE_TYPE_LAPTOP,
      /*device_name=*/kDeviceName,
      /*mac_address=*/kMacAddress,
      /*device_id=*/kDeviceId);

  EXPECT_EQ(kDeviceName, metadata.device_name());
  EXPECT_EQ(kMacAddress, metadata.bluetooth_mac_address());
  EXPECT_EQ(kDeviceId, metadata.device_id());
}

TEST_F(ProtoConversionsTest, DeviceTypeToMojo) {
  EXPECT_EQ(
      mojom::PresenceDeviceType::kChromeos,
      DeviceTypeToMojom(::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS));
}

TEST_F(ProtoConversionsTest, PublicCredentialTypeToMojom) {
  EXPECT_EQ(
      mojom::PublicCredentialType::kLocalPublicCredential,
      PublicCredentialTypeToMojom(
          ::nearby::presence::PublicCredentialType::kLocalPublicCredential));

  EXPECT_EQ(
      mojom::PublicCredentialType::kRemotePublicCredential,
      PublicCredentialTypeToMojom(
          ::nearby::presence::PublicCredentialType::kRemotePublicCredential));
}

TEST_F(ProtoConversionsTest, MetadataToMojom) {
  ::nearby::internal::DeviceIdentityMetaData metadata = BuildMetadata(
      /*device_type=*/::nearby::internal::DeviceType::DEVICE_TYPE_LAPTOP,
      /*device_name=*/kDeviceName,
      /*mac_address=*/kMacAddress,
      /*device_id=*/kDeviceId);
  mojom::MetadataPtr mojo = MetadataToMojom(metadata);

  EXPECT_EQ(kDeviceName, mojo->device_name);
  EXPECT_EQ(kMacAddress, std::string(mojo->bluetooth_mac_address.begin(),
                                     mojo->bluetooth_mac_address.end()));
  EXPECT_EQ(kDeviceId,
            std::string(mojo->device_id.begin(), mojo->device_id.end()));
}

TEST_F(ProtoConversionsTest, IdentityTypeFromMojom) {
  EXPECT_EQ(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
      IdentityTypeFromMojom(mojom::IdentityType::kIdentityTypePrivateGroup));
}

TEST_F(ProtoConversionsTest, SharedCredentialFromMojom) {
  mojom::SharedCredentialPtr mojo_cred = mojom::SharedCredential::New(
      GetKeySeed(), kStartTimeMillis, kEndTimeMillis,
      GetEncryptedMetadataBytesV0(), GetMetadataEncryptionTag(),
      GetConnectionSignatureVerificationKey(),
      GetAdvertisementSignatureVerificationKey(),
      mojom::IdentityType::kIdentityTypePrivateGroup, GetVersion(),
      mojom::CredentialType::kCredentialTypeDevice,
      GetEncryptedMetadataBytesV1(), GetIdentityTokenShortSaltAdvHmacKeyV1(),
      kSharedCredentialId, kDusi, kSignatureVersion,
      GetIdentityTokenExtendedSaltAdvHmacKeyV1(),
      GetIdentityTokenSignedAdvHmacKeyV1());
  ::nearby::internal::SharedCredential proto_cred =
      SharedCredentialFromMojom(mojo_cred.get());
  EXPECT_EQ(std::string(GetKeySeed().begin(), GetKeySeed().end()),
            proto_cred.key_seed());
  EXPECT_EQ(kStartTimeMillis, proto_cred.start_time_millis());
  EXPECT_EQ(kEndTimeMillis, proto_cred.end_time_millis());
  EXPECT_EQ(std::string(GetEncryptedMetadataBytesV0().begin(),
                        GetEncryptedMetadataBytesV0().end()),
            proto_cred.encrypted_metadata_bytes_v0());
  EXPECT_EQ(std::string(GetMetadataEncryptionTag().begin(),
                        GetMetadataEncryptionTag().end()),
            proto_cred.metadata_encryption_key_tag_v0());
  EXPECT_EQ(std::string(GetConnectionSignatureVerificationKey().begin(),
                        GetConnectionSignatureVerificationKey().end()),
            proto_cred.connection_signature_verification_key());
  EXPECT_EQ(std::string(GetAdvertisementSignatureVerificationKey().begin(),
                        GetAdvertisementSignatureVerificationKey().end()),
            proto_cred.advertisement_signature_verification_key());
  EXPECT_EQ(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
            proto_cred.identity_type());
  EXPECT_EQ(std::string(GetVersion().begin(), GetVersion().end()),
            proto_cred.version());
  EXPECT_EQ(::nearby::internal::CredentialType::CREDENTIAL_TYPE_DEVICE,
            proto_cred.credential_type());
  EXPECT_EQ(std::string(GetEncryptedMetadataBytesV1().begin(),
                        GetEncryptedMetadataBytesV1().end()),
            proto_cred.encrypted_metadata_bytes_v1());
  EXPECT_EQ(std::string(GetIdentityTokenShortSaltAdvHmacKeyV1().begin(),
                        GetIdentityTokenShortSaltAdvHmacKeyV1().end()),
            proto_cred.identity_token_short_salt_adv_hmac_key_v1());
  EXPECT_EQ(kSharedCredentialId, proto_cred.id());
  EXPECT_EQ(kDusi, proto_cred.dusi());
  EXPECT_EQ(kSignatureVersion, proto_cred.signature_version());
  EXPECT_EQ(std::string(GetIdentityTokenExtendedSaltAdvHmacKeyV1().begin(),
                        GetIdentityTokenExtendedSaltAdvHmacKeyV1().end()),
            proto_cred.identity_token_extended_salt_adv_hmac_key_v1());
  EXPECT_EQ(std::string(GetIdentityTokenSignedAdvHmacKeyV1().begin(),
                        GetIdentityTokenSignedAdvHmacKeyV1().end()),
            proto_cred.identity_token_signed_adv_hmac_key_v1());
}

TEST_F(ProtoConversionsTest, SharedCredentialToMojom) {
  ::nearby::internal::SharedCredential proto_cred;
  proto_cred.set_secret_id(
      std::string(GetSecretId().begin(), GetSecretId().end()));
  proto_cred.set_key_seed(
      std::string(GetKeySeed().begin(), GetKeySeed().end()));
  proto_cred.set_start_time_millis(kStartTimeMillis);
  proto_cred.set_end_time_millis(kEndTimeMillis);
  proto_cred.set_encrypted_metadata_bytes_v0(
      std::string(GetEncryptedMetadataBytesV0().begin(),
                  GetEncryptedMetadataBytesV0().end()));
  proto_cred.set_metadata_encryption_key_tag_v0(std::string(
      GetMetadataEncryptionTag().begin(), GetMetadataEncryptionTag().end()));
  proto_cred.set_connection_signature_verification_key(
      std::string(GetConnectionSignatureVerificationKey().begin(),
                  GetConnectionSignatureVerificationKey().end()));
  proto_cred.set_advertisement_signature_verification_key(
      std::string(GetAdvertisementSignatureVerificationKey().begin(),
                  GetAdvertisementSignatureVerificationKey().end()));
  proto_cred.set_identity_type(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);
  proto_cred.set_version(std::string(GetVersion().begin(), GetVersion().end()));
  proto_cred.set_credential_type(
      ::nearby::internal::CredentialType::CREDENTIAL_TYPE_GAIA);
  proto_cred.set_encrypted_metadata_bytes_v1(
      std::string(GetEncryptedMetadataBytesV1().begin(),
                  GetEncryptedMetadataBytesV1().end()));
  proto_cred.set_identity_token_short_salt_adv_hmac_key_v1(
      std::string(GetIdentityTokenShortSaltAdvHmacKeyV1().begin(),
                  GetIdentityTokenShortSaltAdvHmacKeyV1().end()));
  proto_cred.set_id(kSharedCredentialId);
  proto_cred.set_dusi(kDusi);
  proto_cred.set_signature_version(kSignatureVersion);
  proto_cred.set_identity_token_extended_salt_adv_hmac_key_v1(
      std::string(GetIdentityTokenExtendedSaltAdvHmacKeyV1().begin(),
                  GetIdentityTokenExtendedSaltAdvHmacKeyV1().end()));
  proto_cred.set_identity_token_signed_adv_hmac_key_v1(
      std::string(GetIdentityTokenSignedAdvHmacKeyV1().begin(),
                  GetIdentityTokenSignedAdvHmacKeyV1().end()));

  mojom::SharedCredentialPtr mojo_cred = SharedCredentialToMojom(proto_cred);
  EXPECT_EQ(GetKeySeed(), mojo_cred->key_seed);
  EXPECT_EQ(kStartTimeMillis, mojo_cred->start_time_millis);
  EXPECT_EQ(kEndTimeMillis, mojo_cred->end_time_millis);
  EXPECT_EQ(GetEncryptedMetadataBytesV0(),
            mojo_cred->encrypted_metadata_bytes_v0);
  EXPECT_EQ(GetMetadataEncryptionTag(),
            mojo_cred->metadata_encryption_key_tag_v0);
  EXPECT_EQ(GetConnectionSignatureVerificationKey(),
            mojo_cred->connection_signature_verification_key);
  EXPECT_EQ(GetAdvertisementSignatureVerificationKey(),
            mojo_cred->advertisement_signature_verification_key);
  EXPECT_EQ(mojom::IdentityType::kIdentityTypePrivateGroup,
            mojo_cred->identity_type);
  EXPECT_EQ(GetVersion(), mojo_cred->version);
  EXPECT_EQ(mojom::CredentialType::kCredentialTypeGaia,
            mojo_cred->credential_type);
  EXPECT_EQ(GetEncryptedMetadataBytesV1(),
            mojo_cred->encrypted_metadata_bytes_v1);
  EXPECT_EQ(GetIdentityTokenShortSaltAdvHmacKeyV1(),
            mojo_cred->identity_token_short_salt_adv_hmac_key_v1);
  EXPECT_EQ(kSharedCredentialId, mojo_cred->id);
  EXPECT_EQ(kDusi, mojo_cred->dusi);
  EXPECT_EQ(kSignatureVersion, mojo_cred->signature_version);
  EXPECT_EQ(GetIdentityTokenExtendedSaltAdvHmacKeyV1(),
            mojo_cred->identity_token_extended_salt_adv_hmac_key_v1);
  EXPECT_EQ(GetIdentityTokenSignedAdvHmacKeyV1(),
            mojo_cred->identity_token_signed_adv_hmac_key_v1);
}

TEST_F(ProtoConversionsTest,
       RemoteSharedCredentialToThirdPartySharedCredential) {
  ash::nearby::proto::SharedCredential remote_shared_credential;
  remote_shared_credential.set_id(kSharedCredentialId);
  remote_shared_credential.set_key_seed(
      std::string(GetKeySeed().begin(), GetKeySeed().end()));
  remote_shared_credential.set_start_time_millis(kStartTimeMillis);
  remote_shared_credential.set_end_time_millis(kEndTimeMillis);
  remote_shared_credential.set_encrypted_metadata_bytes_v0(
      std::string(GetEncryptedMetadataBytesV0().begin(),
                  GetEncryptedMetadataBytesV0().end()));
  remote_shared_credential.set_metadata_encryption_key_tag_v0(std::string(
      GetMetadataEncryptionTag().begin(), GetMetadataEncryptionTag().end()));
  remote_shared_credential.set_connection_signature_verification_key(
      std::string(GetConnectionSignatureVerificationKey().begin(),
                  GetConnectionSignatureVerificationKey().end()));
  remote_shared_credential.set_advertisement_signature_verification_key(
      std::string(GetAdvertisementSignatureVerificationKey().begin(),
                  GetAdvertisementSignatureVerificationKey().end()));
  remote_shared_credential.set_identity_type(
      ash::nearby::proto::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);
  remote_shared_credential.set_version(
      std::string(GetVersion().begin(), GetVersion().end()));
  remote_shared_credential.set_credential_type(
      ash::nearby::proto::CredentialType::CREDENTIAL_TYPE_GAIA);
  remote_shared_credential.set_encrypted_metadata_bytes_v1(
      std::string(GetEncryptedMetadataBytesV1().begin(),
                  GetEncryptedMetadataBytesV1().end()));
  remote_shared_credential.set_identity_token_short_salt_adv_hmac_key_v1(
      std::string(GetIdentityTokenShortSaltAdvHmacKeyV1().begin(),
                  GetIdentityTokenShortSaltAdvHmacKeyV1().end()));
  remote_shared_credential.set_dusi(kDusi);
  remote_shared_credential.set_signature_version(kSignatureVersion);
  remote_shared_credential.set_identity_token_extended_salt_adv_hmac_key_v1(
      std::string(GetIdentityTokenExtendedSaltAdvHmacKeyV1().begin(),
                  GetIdentityTokenExtendedSaltAdvHmacKeyV1().end()));
  remote_shared_credential.set_identity_token_signed_adv_hmac_key_v1(
      std::string(GetIdentityTokenSignedAdvHmacKeyV1().begin(),
                  GetIdentityTokenSignedAdvHmacKeyV1().end()));

  ::nearby::internal::SharedCredential proto_cred =
      RemoteSharedCredentialToThirdPartySharedCredential(
          remote_shared_credential);
  EXPECT_EQ(kSharedCredentialId, proto_cred.id());
  EXPECT_EQ(std::string(GetKeySeed().begin(), GetKeySeed().end()),
            proto_cred.key_seed());
  EXPECT_EQ(kStartTimeMillis, proto_cred.start_time_millis());
  EXPECT_EQ(kEndTimeMillis, proto_cred.end_time_millis());
  EXPECT_EQ(std::string(GetEncryptedMetadataBytesV0().begin(),
                        GetEncryptedMetadataBytesV0().end()),
            proto_cred.encrypted_metadata_bytes_v0());
  EXPECT_EQ(std::string(GetMetadataEncryptionTag().begin(),
                        GetMetadataEncryptionTag().end()),
            proto_cred.metadata_encryption_key_tag_v0());
  EXPECT_EQ(std::string(GetConnectionSignatureVerificationKey().begin(),
                        GetConnectionSignatureVerificationKey().end()),
            proto_cred.connection_signature_verification_key());
  EXPECT_EQ(std::string(GetAdvertisementSignatureVerificationKey().begin(),
                        GetAdvertisementSignatureVerificationKey().end()),
            proto_cred.advertisement_signature_verification_key());
  EXPECT_EQ(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
            proto_cred.identity_type());
  EXPECT_EQ(std::string(GetVersion().begin(), GetVersion().end()),
            proto_cred.version());
  EXPECT_EQ(::nearby::internal::CredentialType::CREDENTIAL_TYPE_GAIA,
            proto_cred.credential_type());
  EXPECT_EQ(std::string(GetEncryptedMetadataBytesV1().begin(),
                        GetEncryptedMetadataBytesV1().end()),
            proto_cred.encrypted_metadata_bytes_v1());
  EXPECT_EQ(std::string(GetIdentityTokenShortSaltAdvHmacKeyV1().begin(),
                        GetIdentityTokenShortSaltAdvHmacKeyV1().end()),
            proto_cred.identity_token_short_salt_adv_hmac_key_v1());
  EXPECT_EQ(kDusi, proto_cred.dusi());
  EXPECT_EQ(kSignatureVersion, proto_cred.signature_version());
  EXPECT_EQ(std::string(GetIdentityTokenExtendedSaltAdvHmacKeyV1().begin(),
                        GetIdentityTokenExtendedSaltAdvHmacKeyV1().end()),
            proto_cred.identity_token_extended_salt_adv_hmac_key_v1());
  EXPECT_EQ(std::string(GetIdentityTokenSignedAdvHmacKeyV1().begin(),
                        GetIdentityTokenSignedAdvHmacKeyV1().end()),
            proto_cred.identity_token_signed_adv_hmac_key_v1());
}

TEST_F(ProtoConversionsTest, PrivateKeyToMojomTest) {
  ::nearby::internal::LocalCredential::PrivateKey private_key;
  private_key.set_certificate_alias(
      std::string(AdvertisementSigningKeyCertificateAlias));
  private_key.set_key(
      std::string(GetPrivateKey().begin(), GetPrivateKey().end()));

  mojom::PrivateKeyPtr mojo_private_key = PrivateKeyToMojom(private_key);

  EXPECT_EQ(AdvertisementSigningKeyCertificateAlias,
            mojo_private_key->certificate_alias);
  EXPECT_EQ(GetPrivateKey(), mojo_private_key->key);
}

TEST_F(ProtoConversionsTest, PrivateKeyFromMojomTest) {
  mojom::PrivateKeyPtr mojo_private_key = mojom::PrivateKey::New(
      AdvertisementSigningKeyCertificateAlias, GetPrivateKey());

  ::nearby::internal::LocalCredential::PrivateKey proto_private_key =
      PrivateKeyFromMojom(mojo_private_key.get());

  EXPECT_EQ(std::string(AdvertisementSigningKeyCertificateAlias),
            proto_private_key.certificate_alias());
  EXPECT_EQ(std::string(GetPrivateKey().begin(), GetPrivateKey().end()),
            proto_private_key.key());
}

TEST_F(ProtoConversionsTest, LocalCredentialToMojomTest) {
  ::nearby::internal::LocalCredential local_credential;
  local_credential.set_secret_id(
      std::string(GetSecretId().begin(), GetSecretId().end()));
  local_credential.set_key_seed(
      std::string(GetKeySeed().begin(), GetKeySeed().end()));
  local_credential.set_start_time_millis(kStartTimeMillis);
  local_credential.set_end_time_millis(kEndTimeMillis);
  local_credential.set_metadata_encryption_key_v0(
      std::string(GetAdvertisementMetadataEncrpytionKeyV0().begin(),
                  GetAdvertisementMetadataEncrpytionKeyV0().end()));
  local_credential.set_allocated_advertisement_signing_key(
      CreatePrivateKeyProto(AdvertisementSigningKeyCertificateAlias,
                            GetPrivateKey()));
  local_credential.set_allocated_connection_signing_key(CreatePrivateKeyProto(
      ConnectionSigningKeyCertificateAlias, GetPrivateKey()));
  local_credential.set_identity_type(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);
  SetProtoMap(local_credential.mutable_consumed_salts(), GetConsumedSalts());
  local_credential.set_identity_token_v1(
      std::string(GetIdentityTokenV1().begin(), GetIdentityTokenV1().end()));
  local_credential.set_id(kLocalCredentialId);
  local_credential.set_signature_version(kSignatureVersion);

  mojom::LocalCredentialPtr mojo_local_credential =
      LocalCredentialToMojom(local_credential);

  EXPECT_EQ(GetSecretId(), mojo_local_credential->secret_id);
  EXPECT_EQ(GetKeySeed(), mojo_local_credential->key_seed);
  EXPECT_EQ(kStartTimeMillis, mojo_local_credential->start_time_millis);
  EXPECT_EQ(kEndTimeMillis, mojo_local_credential->end_time_millis);
  EXPECT_EQ(GetAdvertisementMetadataEncrpytionKeyV0(),
            mojo_local_credential->metadata_encryption_key_v0);
  EXPECT_EQ(mojom::IdentityType::kIdentityTypePrivateGroup,
            mojo_local_credential->identity_type);
  EXPECT_EQ(GetConsumedSalts().size(),
            mojo_local_credential->consumed_salts.size());
  EXPECT_EQ(GetIdentityTokenV1(), mojo_local_credential->identity_token_v1);
  EXPECT_EQ(kLocalCredentialId, mojo_local_credential->id);
  EXPECT_EQ(kSignatureVersion, mojo_local_credential->signature_version);
  // advertisement_signing_key
  EXPECT_EQ(
      AdvertisementSigningKeyCertificateAlias,
      mojo_local_credential->advertisement_signing_key->certificate_alias);
  EXPECT_EQ(GetPrivateKey(),
            mojo_local_credential->advertisement_signing_key->key);
  // connection_singing_key
  EXPECT_EQ(ConnectionSigningKeyCertificateAlias,
            mojo_local_credential->connection_signing_key->certificate_alias);
  EXPECT_EQ(GetPrivateKey(),
            mojo_local_credential->connection_signing_key->key);
}

TEST_F(ProtoConversionsTest, LocalCredentialFromMojomTest) {
  base::flat_map<uint32_t, bool> kConsumedSalts_flat(GetConsumedSalts().begin(),
                                                     GetConsumedSalts().end());
  mojom::LocalCredentialPtr mojo_local_credential = mojom::LocalCredential::New(
      GetSecretId(), GetKeySeed(), kStartTimeMillis, kEndTimeMillis,
      GetAdvertisementMetadataEncrpytionKeyV0(),
      mojom::PrivateKey::New(AdvertisementSigningKeyCertificateAlias,
                             GetPrivateKey()),
      mojom::PrivateKey::New(ConnectionSigningKeyCertificateAlias,
                             GetPrivateKey()),
      mojom::IdentityType::kIdentityTypePrivateGroup, kConsumedSalts_flat,
      GetIdentityTokenV1(), kLocalCredentialId, kSignatureVersion);

  ::nearby::internal::LocalCredential local_credential_proto =
      LocalCredentialFromMojom(mojo_local_credential.get());

  EXPECT_EQ(std::string(GetSecretId().begin(), GetSecretId().end()),
            local_credential_proto.secret_id());
  EXPECT_EQ(std::string(GetKeySeed().begin(), GetKeySeed().end()),
            local_credential_proto.key_seed());
  EXPECT_EQ(kStartTimeMillis, local_credential_proto.start_time_millis());
  EXPECT_EQ(kEndTimeMillis, local_credential_proto.end_time_millis());
  EXPECT_EQ(std::string(GetAdvertisementMetadataEncrpytionKeyV0().begin(),
                        GetAdvertisementMetadataEncrpytionKeyV0().end()),
            local_credential_proto.metadata_encryption_key_v0());
  EXPECT_EQ(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
            local_credential_proto.identity_type());
  EXPECT_EQ(GetConsumedSalts().size(),
            local_credential_proto.consumed_salts().size());
  EXPECT_EQ(
      std::string(GetIdentityTokenV1().begin(), GetIdentityTokenV1().end()),
      local_credential_proto.identity_token_v1());
  EXPECT_EQ(kLocalCredentialId, local_credential_proto.id());
  EXPECT_EQ(kSignatureVersion, local_credential_proto.signature_version());
  // advertisement_signing_key
  EXPECT_EQ(
      AdvertisementSigningKeyCertificateAlias,
      local_credential_proto.advertisement_signing_key().certificate_alias());
  EXPECT_EQ(std::string(GetPrivateKey().begin(), GetPrivateKey().end()),
            local_credential_proto.advertisement_signing_key().key());
  // connection_signing_key
  EXPECT_EQ(
      ConnectionSigningKeyCertificateAlias,
      local_credential_proto.connection_signing_key().certificate_alias());
  EXPECT_EQ(std::string(GetPrivateKey().begin(), GetPrivateKey().end()),
            local_credential_proto.connection_signing_key().key());
}

}  // namespace ash::nearby::presence::proto
