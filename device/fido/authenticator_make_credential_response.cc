// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_make_credential_response.h"

#include <utility>

#include "components/cbor/writer.h"
#include "device/fido/attestation_object.h"
#include "device/fido/attestation_statement_formats.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/device_public_key_extension.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/large_blob.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"

namespace device {

// static
absl::optional<AuthenticatorMakeCredentialResponse>
AuthenticatorMakeCredentialResponse::CreateFromU2fRegisterResponse(
    absl::optional<FidoTransportProtocol> transport_used,
    base::span<const uint8_t, kRpIdHashLength> relying_party_id_hash,
    base::span<const uint8_t> u2f_data) {
  auto public_key = P256PublicKey::ExtractFromU2fRegistrationResponse(
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256), u2f_data);
  if (!public_key)
    return absl::nullopt;

  auto attested_credential_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          u2f_data, std::move(public_key));

  if (!attested_credential_data)
    return absl::nullopt;

  // Extract the credential_id for packing into the response data.
  std::vector<uint8_t> credential_id =
      attested_credential_data->credential_id();

  // The counter is zeroed out for Register requests.
  std::array<uint8_t, 4> counter = {};
  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);

  AuthenticatorData authenticator_data(relying_party_id_hash, flags, counter,
                                       std::move(attested_credential_data));

  auto fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(u2f_data);

  if (!fido_attestation_statement)
    return absl::nullopt;

  AuthenticatorMakeCredentialResponse response(
      transport_used, AttestationObject(std::move(authenticator_data),
                                        std::move(fido_attestation_statement)));
  response.is_resident_key = false;
  return response;
}

AuthenticatorMakeCredentialResponse::AuthenticatorMakeCredentialResponse(
    absl::optional<FidoTransportProtocol> in_transport_used,
    AttestationObject in_attestation_object)
    : attestation_object(std::move(in_attestation_object)),
      transport_used(in_transport_used) {}

AuthenticatorMakeCredentialResponse::AuthenticatorMakeCredentialResponse(
    AuthenticatorMakeCredentialResponse&& that) = default;

AuthenticatorMakeCredentialResponse&
AuthenticatorMakeCredentialResponse::operator=(
    AuthenticatorMakeCredentialResponse&& other) = default;

AuthenticatorMakeCredentialResponse::~AuthenticatorMakeCredentialResponse() =
    default;

std::vector<uint8_t>
AuthenticatorMakeCredentialResponse::GetCBOREncodedAttestationObject() const {
  return cbor::Writer::Write(AsCBOR(attestation_object))
      .value_or(std::vector<uint8_t>());
}

absl::optional<device::DevicePublicKeyOutput>
AuthenticatorMakeCredentialResponse::GetDevicePublicKeyResponse() const {
  const absl::optional<cbor::Value>& maybe_extensions =
      attestation_object.authenticator_data().extensions();
  if (!maybe_extensions) {
    return absl::nullopt;
  }

  DCHECK(maybe_extensions->is_map());
  const cbor::Value::MapValue& extensions = maybe_extensions->GetMap();
  const auto device_public_key_it =
      extensions.find(cbor::Value(device::kExtensionDevicePublicKey));
  if (device_public_key_it == extensions.end()) {
    return absl::nullopt;
  }

  return device::DevicePublicKeyOutput::FromExtension(
      device_public_key_it->second);
}

const std::array<uint8_t, kRpIdHashLength>&
AuthenticatorMakeCredentialResponse::GetRpIdHash() const {
  return attestation_object.rp_id_hash();
}

std::vector<uint8_t> AsCTAPStyleCBORBytes(
    const AuthenticatorMakeCredentialResponse& response) {
  const AttestationObject& object = response.attestation_object;
  cbor::Value::MapValue map;
  map.emplace(1, object.attestation_statement().format_name());
  map.emplace(2, object.authenticator_data().SerializeToByteArray());
  map.emplace(3, AsCBOR(object.attestation_statement()));
  if (response.enterprise_attestation_returned) {
    map.emplace(4, true);
  }
  if (response.large_blob_type == LargeBlobSupportType::kKey) {
    // Chrome ignores the value of the large blob key on make credential
    // requests.
    map.emplace(5, cbor::Value(std::array<uint8_t, kLargeBlobKeyLength>()));
  }
  cbor::Value::MapValue unsigned_extension_outputs;
  if (response.device_public_key_signature.has_value()) {
    unsigned_extension_outputs.emplace(kExtensionDevicePublicKey,
                                       *response.device_public_key_signature);
  }
  if (response.prf_enabled) {
    cbor::Value::MapValue prf;
    prf.emplace(kExtensionPRFEnabled, true);
    unsigned_extension_outputs.emplace(kExtensionPRF, std::move(prf));
  }
  if (response.large_blob_type == LargeBlobSupportType::kExtension) {
    cbor::Value::MapValue large_blob_ext;
    large_blob_ext.emplace(kExtensionLargeBlobSupported, true);
    unsigned_extension_outputs.emplace(kExtensionLargeBlob,
                                       std::move(large_blob_ext));
  }
  if (!unsigned_extension_outputs.empty()) {
    map.emplace(6, std::move(unsigned_extension_outputs));
  }
  auto encoded_bytes = cbor::Writer::Write(cbor::Value(std::move(map)));
  DCHECK(encoded_bytes);
  return std::move(*encoded_bytes);
}

}  // namespace device
