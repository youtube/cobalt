// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"

namespace ash::nearby::presence::proto {

::nearby::internal::Metadata BuildMetadata(
    ::nearby::internal::DeviceType device_type,
    const std::string& account_name,
    const std::string& device_name,
    const std::string& user_name,
    const std::string& profile_url,
    const std::string& mac_address) {
  ::nearby::internal::Metadata proto;
  proto.set_device_type(device_type);
  proto.set_account_name(account_name);
  proto.set_user_name(user_name);
  proto.set_device_name(device_name);
  proto.set_user_name(user_name);
  proto.set_device_profile_url(profile_url);
  proto.set_bluetooth_mac_address(mac_address);
  return proto;
}

mojom::PresenceDeviceType DeviceTypeToMojom(
    ::nearby::internal::DeviceType device_type) {
  switch (device_type) {
    case ::nearby::internal::DeviceType::DEVICE_TYPE_UNKNOWN:
      return mojom::PresenceDeviceType::kUnknown;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_PHONE:
      return mojom::PresenceDeviceType::kPhone;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_TABLET:
      return mojom::PresenceDeviceType::kTablet;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_DISPLAY:
      return mojom::PresenceDeviceType::kDisplay;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_TV:
      return mojom::PresenceDeviceType::kTv;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_WATCH:
      return mojom::PresenceDeviceType::kWatch;
    case ::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS:
      return mojom::PresenceDeviceType::kChromeos;
    default:
      return mojom::PresenceDeviceType::kUnknown;
  }
}

// TODO(b:300510287): Use EnumTrait<> to convert between enums rather than
// a method call.
mojom::PublicCredentialType PublicCredentialTypeToMojom(
    ::nearby::presence::PublicCredentialType public_credential_type) {
  switch (public_credential_type) {
    case ::nearby::presence::PublicCredentialType::kLocalPublicCredential:
      return mojom::PublicCredentialType::kLocalPublicCredential;
    case ::nearby::presence::PublicCredentialType::kRemotePublicCredential:
      return mojom::PublicCredentialType::kRemotePublicCredential;
  }

  NOTREACHED_NORETURN();
}

mojom::PrivateKeyPtr PrivateKeyToMojom(
    ::nearby::internal::LocalCredential::PrivateKey private_key) {
  return mojom::PrivateKey::New(
      private_key.certificate_alias(),
      std::vector<uint8_t>(private_key.key().begin(), private_key.key().end()));
}

::nearby::internal::IdentityType IdentityTypeFromMojom(
    mojom::IdentityType identity_type) {
  switch (identity_type) {
    case mojom::IdentityType::kIdentityTypeUnspecified:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
    case mojom::IdentityType::kIdentityTypePrivate:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE;
    case mojom::IdentityType::kIdentityTypeTrusted:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_TRUSTED;
    case mojom::IdentityType::kIdentityTypePublic:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC;
    case mojom::IdentityType::kIdentityTypeProvisioned:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PROVISIONED;
    default:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
  }
}

mojom::MetadataPtr MetadataToMojom(::nearby::internal::Metadata metadata) {
  return mojom::Metadata::New(
      DeviceTypeToMojom(metadata.device_type()), metadata.account_name(),
      metadata.device_name(), metadata.user_name(),
      metadata.device_profile_url(),
      std::vector<uint8_t>(metadata.bluetooth_mac_address().begin(),
                           metadata.bluetooth_mac_address().end()));
}

::nearby::internal::LocalCredential::PrivateKey PrivateKeyFromMojom(
    mojom::PrivateKey* private_key) {
  ::nearby::internal::LocalCredential::PrivateKey proto;
  proto.set_certificate_alias(
      std::string(private_key->certificate_alias.begin(),
                  private_key->certificate_alias.end()));
  proto.set_key(std::string(private_key->key.begin(), private_key->key.end()));
  return proto;
}

::nearby::internal::SharedCredential SharedCredentialFromMojom(
    mojom::SharedCredential* shared_credential) {
  ::nearby::internal::SharedCredential proto;
  proto.set_secret_id(std::string(shared_credential->secret_id.begin(),
                                  shared_credential->secret_id.end()));
  proto.set_key_seed(std::string(shared_credential->key_seed.begin(),
                                 shared_credential->key_seed.end()));
  proto.set_start_time_millis(shared_credential->start_time_millis);
  proto.set_end_time_millis(shared_credential->end_time_millis);
  proto.set_encrypted_metadata_bytes_v0(
      std::string(shared_credential->encrypted_metadata_bytes.begin(),
                  shared_credential->encrypted_metadata_bytes.end()));
  proto.set_metadata_encryption_key_tag_v0(
      std::string(shared_credential->metadata_encryption_key_tag.begin(),
                  shared_credential->metadata_encryption_key_tag.end()));
  proto.set_connection_signature_verification_key(std::string(
      shared_credential->connection_signature_verification_key.begin(),
      shared_credential->connection_signature_verification_key.end()));
  proto.set_advertisement_signature_verification_key(std::string(
      shared_credential->advertisement_signature_verification_key.begin(),
      shared_credential->advertisement_signature_verification_key.end()));
  proto.set_identity_type(
      IdentityTypeFromMojom(shared_credential->identity_type));
  proto.set_version(std::string(shared_credential->version.begin(),
                                shared_credential->version.end()));
  return proto;
}

::nearby::internal::LocalCredential LocalCredentialFromMojom(
    mojom::LocalCredential* local_credential) {
  ::nearby::internal::LocalCredential proto;
  proto.set_secret_id(std::string(local_credential->secret_id.begin(),
                                  local_credential->secret_id.end()));
  proto.set_key_seed(std::string(local_credential->key_seed.begin(),
                                 local_credential->key_seed.end()));
  proto.set_start_time_millis(local_credential->start_time_millis);
  proto.set_metadata_encryption_key_v0(
      std::string(local_credential->metadata_encryption_key_v0.begin(),
                  local_credential->metadata_encryption_key_v0.end()));

  auto* advertisement_signing_key =
      new ::nearby::internal::LocalCredential::PrivateKey(PrivateKeyFromMojom(
          local_credential->advertisement_signing_key.get()));
  proto.set_allocated_advertisement_signing_key(advertisement_signing_key);

  auto* connection_signing_key =
      new ::nearby::internal::LocalCredential::PrivateKey(
          PrivateKeyFromMojom(local_credential->connection_signing_key.get()));
  proto.set_allocated_connection_signing_key(connection_signing_key);

  proto.set_identity_type(
      IdentityTypeFromMojom(local_credential->identity_type));

  for (const auto& pair : local_credential->consumed_salts) {
    auto map_pair =
        ::google::protobuf::MapPair<uint32_t, bool>(pair.first, pair.second);
    proto.mutable_consumed_salts()->insert(map_pair);
  }

  proto.set_metadata_encryption_key_v1(
      std::string(local_credential->metadata_encryption_key_v1.begin(),
                  local_credential->metadata_encryption_key_v1.end()));

  return proto;
}

mojom::IdentityType IdentityTypeToMojom(
    ::nearby::internal::IdentityType identity_type) {
  switch (identity_type) {
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED:
      return mojom::IdentityType::kIdentityTypeUnspecified;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE:
      return mojom::IdentityType::kIdentityTypePrivate;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_TRUSTED:
      return mojom::IdentityType::kIdentityTypeTrusted;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC:
      return mojom::IdentityType::kIdentityTypePublic;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PROVISIONED:
      return mojom::IdentityType::kIdentityTypeProvisioned;
    default:
      return mojom::IdentityType::kIdentityTypeUnspecified;
  }
}

mojom::SharedCredentialPtr SharedCredentialToMojom(
    ::nearby::internal::SharedCredential shared_credential) {
  return mojom::SharedCredential::New(
      std::vector<uint8_t>(shared_credential.secret_id().begin(),
                           shared_credential.secret_id().end()),
      std::vector<uint8_t>(shared_credential.key_seed().begin(),
                           shared_credential.key_seed().end()),
      shared_credential.start_time_millis(),
      shared_credential.end_time_millis(),
      std::vector<uint8_t>(
          shared_credential.encrypted_metadata_bytes_v0().begin(),
          shared_credential.encrypted_metadata_bytes_v0().end()),
      std::vector<uint8_t>(
          shared_credential.metadata_encryption_key_tag_v0().begin(),
          shared_credential.metadata_encryption_key_tag_v0().end()),
      std::vector<uint8_t>(
          shared_credential.connection_signature_verification_key().begin(),
          shared_credential.connection_signature_verification_key().end()),
      std::vector<uint8_t>(
          shared_credential.advertisement_signature_verification_key().begin(),
          shared_credential.advertisement_signature_verification_key().end()),
      IdentityTypeToMojom(shared_credential.identity_type()),
      std::vector<uint8_t>(shared_credential.version().begin(),
                           shared_credential.version().end()));
}

mojom::LocalCredentialPtr LocalCredentialToMojom(
    ::nearby::internal::LocalCredential local_credential) {
  base::flat_map<uint32_t, bool> salt_flat_map(
      local_credential.consumed_salts().begin(),
      local_credential.consumed_salts().end());

  return mojom::LocalCredential::New(
      std::vector<uint8_t>(local_credential.secret_id().begin(),
                           local_credential.secret_id().end()),
      std::vector<uint8_t>(local_credential.key_seed().begin(),
                           local_credential.key_seed().end()),
      local_credential.start_time_millis(),
      std::vector<uint8_t>(
          local_credential.metadata_encryption_key_v0().begin(),
          local_credential.metadata_encryption_key_v0().end()),
      PrivateKeyToMojom(local_credential.advertisement_signing_key()),
      PrivateKeyToMojom(local_credential.connection_signing_key()),
      IdentityTypeToMojom(local_credential.identity_type()), salt_flat_map,
      std::vector<uint8_t>(
          local_credential.metadata_encryption_key_v1().begin(),
          local_credential.metadata_encryption_key_v1().end()));
}

ash::nearby::proto::PublicCertificate PublicCertificateFromSharedCredential(
    ::nearby::internal::SharedCredential shared_credential) {
  ash::nearby::proto::PublicCertificate certificate;
  certificate.set_secret_id(shared_credential.secret_id());
  certificate.set_secret_key(shared_credential.key_seed());
  certificate.set_public_key(
      shared_credential.connection_signature_verification_key());
  certificate.mutable_start_time()->set_seconds(
      MillisecondsToSeconds(shared_credential.start_time_millis()));
  certificate.mutable_end_time()->set_seconds(
      MillisecondsToSeconds(shared_credential.end_time_millis()));
  certificate.set_encrypted_metadata_bytes(
      shared_credential.encrypted_metadata_bytes_v0());
  certificate.set_metadata_encryption_key_tag(
      shared_credential.metadata_encryption_key_tag_v0());
  certificate.set_trust_type(
      TrustTypeFromIdentityType(shared_credential.identity_type()));
  return certificate;
}

ash::nearby::proto::TrustType TrustTypeFromIdentityType(
    ::nearby::internal::IdentityType identity_type) {
  switch (identity_type) {
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED:
      return ash::nearby::proto::TrustType::TRUST_TYPE_UNSPECIFIED;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE:
      return ash::nearby::proto::TrustType::TRUST_TYPE_PRIVATE;
    case ::nearby::internal::IdentityType::IDENTITY_TYPE_TRUSTED:
      return ash::nearby::proto::TrustType::TRUST_TYPE_TRUSTED;
    default:
      return ash::nearby::proto::TrustType::TRUST_TYPE_UNSPECIFIED;
  }
}

int64_t MillisecondsToSeconds(int64_t milliseconds) {
  return milliseconds / 1000.0;
}

::nearby::internal::SharedCredential PublicCertificateToSharedCredential(
    ash::nearby::proto::PublicCertificate certificate) {
  ::nearby::internal::SharedCredential shared_credential;
  shared_credential.set_secret_id(certificate.secret_id());
  shared_credential.set_key_seed(certificate.secret_key());
  shared_credential.set_connection_signature_verification_key(
      certificate.public_key());
  shared_credential.set_start_time_millis(
      SecondsToMilliseconds(certificate.start_time().seconds()));
  shared_credential.set_end_time_millis(
      SecondsToMilliseconds(certificate.end_time().seconds()));
  shared_credential.set_encrypted_metadata_bytes_v0(
      certificate.encrypted_metadata_bytes());
  shared_credential.set_metadata_encryption_key_tag_v0(
      certificate.metadata_encryption_key_tag());
  shared_credential.set_identity_type(
      TrustTypeToIdentityType(certificate.trust_type()));
  return shared_credential;
}

::nearby::internal::IdentityType TrustTypeToIdentityType(
    ash::nearby::proto::TrustType trust_type) {
  switch (trust_type) {
    case ash::nearby::proto::TrustType::TRUST_TYPE_UNSPECIFIED:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
    case ash::nearby::proto::TrustType::TRUST_TYPE_PRIVATE:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE;
    case ash::nearby::proto::TrustType::TRUST_TYPE_TRUSTED:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_TRUSTED;
    default:
      return ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;
  }
}

int64_t SecondsToMilliseconds(int64_t seconds) {
  return seconds * 1000.0;
}

}  // namespace ash::nearby::presence::proto
