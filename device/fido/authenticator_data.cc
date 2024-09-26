// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_data.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

constexpr size_t kAttestedCredentialDataOffset =
    kRpIdHashLength + kFlagsLength + kSignCounterLength;

uint8_t AuthenticatorDataFlags(bool user_present,
                               bool user_verified,
                               bool backup_eligible,
                               bool has_attested_credential_data,
                               bool has_extension_data) {
  return (user_present ? base::strict_cast<uint8_t>(
                             AuthenticatorData::Flag::kTestOfUserPresence)
                       : 0) |
         (user_verified ? base::strict_cast<uint8_t>(
                              AuthenticatorData::Flag::kTestOfUserVerification)
                        : 0) |
         (backup_eligible ? base::strict_cast<uint8_t>(
                                AuthenticatorData::Flag::kBackupEligible)
                          : 0) |
         (has_attested_credential_data
              ? base::strict_cast<uint8_t>(
                    AuthenticatorData::Flag::kAttestation)
              : 0) |
         (has_extension_data
              ? base::strict_cast<uint8_t>(
                    AuthenticatorData::Flag::kExtensionDataIncluded)
              : 0);
}

}  // namespace

// static
absl::optional<AuthenticatorData> AuthenticatorData::DecodeAuthenticatorData(
    base::span<const uint8_t> auth_data) {
  if (auth_data.size() < kAttestedCredentialDataOffset)
    return absl::nullopt;
  auto application_parameter = auth_data.first<kRpIdHashLength>();
  uint8_t flag_byte = auth_data[kRpIdHashLength];
  auto counter =
      auth_data.subspan<kRpIdHashLength + kFlagsLength, kSignCounterLength>();

  auth_data = auth_data.subspan(kAttestedCredentialDataOffset);
  absl::optional<AttestedCredentialData> attested_credential_data;
  if (flag_byte & static_cast<uint8_t>(Flag::kAttestation)) {
    auto maybe_result =
        AttestedCredentialData::ConsumeFromCtapResponse(auth_data);
    if (!maybe_result) {
      return absl::nullopt;
    }
    std::tie(attested_credential_data, auth_data) = std::move(*maybe_result);
  }

  absl::optional<cbor::Value> extensions;
  if (flag_byte & static_cast<uint8_t>(Flag::kExtensionDataIncluded)) {
    cbor::Reader::DecoderError error;
    extensions = cbor::Reader::Read(auth_data, &error);
    if (!extensions) {
      FIDO_LOG(ERROR)
          << "CBOR decoding of authenticator data extensions failed ("
          << cbor::Reader::ErrorCodeToString(error) << ") from "
          << base::HexEncode(auth_data.data(), auth_data.size());
      return absl::nullopt;
    }
    if (!extensions->is_map()) {
      FIDO_LOG(ERROR)
          << "Incorrect CBOR structure of authenticator data extensions: "
          << cbor::DiagnosticWriter::Write(*extensions);
      return absl::nullopt;
    }
  } else if (!auth_data.empty()) {
    return absl::nullopt;
  }

  return AuthenticatorData(application_parameter, flag_byte, counter,
                           std::move(attested_credential_data),
                           std::move(extensions));
}

AuthenticatorData::AuthenticatorData(
    base::span<const uint8_t, kRpIdHashLength> rp_id_hash,
    uint8_t flags,
    base::span<const uint8_t, kSignCounterLength> counter,
    absl::optional<AttestedCredentialData> data,
    absl::optional<cbor::Value> extensions)
    : application_parameter_(fido_parsing_utils::Materialize(rp_id_hash)),
      flags_(flags),
      counter_(fido_parsing_utils::Materialize(counter)),
      attested_data_(std::move(data)),
      extensions_(std::move(extensions)) {
  DCHECK(!extensions_ || extensions_->is_map());
  DCHECK_EQ((flags_ & static_cast<uint8_t>(Flag::kExtensionDataIncluded)) != 0,
            !!extensions_);
  DCHECK_EQ(((flags_ & static_cast<uint8_t>(Flag::kAttestation)) != 0),
            !!attested_data_);
}

AuthenticatorData::AuthenticatorData(
    base::span<const uint8_t, kRpIdHashLength> rp_id_hash,
    bool user_present,
    bool user_verified,
    bool backup_eligible,
    uint32_t sign_counter,
    absl::optional<AttestedCredentialData> attested_credential_data,
    absl::optional<cbor::Value> extensions)
    : AuthenticatorData(
          rp_id_hash,
          AuthenticatorDataFlags(user_present,
                                 user_verified,
                                 backup_eligible,
                                 attested_credential_data.has_value(),
                                 extensions.has_value()),
          std::array<uint8_t, kSignCounterLength>{
              static_cast<uint8_t>(sign_counter >> 24),
              static_cast<uint8_t>(sign_counter >> 16),
              static_cast<uint8_t>(sign_counter >> 8),
              static_cast<uint8_t>(sign_counter)},
          std::move(attested_credential_data),
          std::move(extensions)) {}

AuthenticatorData::AuthenticatorData(AuthenticatorData&& other) = default;
AuthenticatorData& AuthenticatorData::operator=(AuthenticatorData&& other) =
    default;

AuthenticatorData::~AuthenticatorData() = default;

bool AuthenticatorData::DeleteDeviceAaguid() {
  if (!attested_data_)
    return false;

  return attested_data_->DeleteAaguid();
}

bool AuthenticatorData::EraseExtension(base::StringPiece name) {
  if (!extensions_) {
    return false;
  }

  DCHECK(extensions_->is_map());
  const cbor::Value::MapValue& orig_map = extensions_->GetMap();
  const auto it = orig_map.find(cbor::Value(name));
  if (it == orig_map.end()) {
    return false;
  }

  cbor::Value::MapValue new_map;
  for (const auto& [key, value] : orig_map) {
    if (key.is_string() && name == key.GetString()) {
      continue;
    }

    new_map.emplace(key.Clone(), value.Clone());
  }

  extensions_ = cbor::Value(std::move(new_map));
  return true;
}

std::vector<uint8_t> AuthenticatorData::SerializeToByteArray() const {
  std::vector<uint8_t> authenticator_data;
  fido_parsing_utils::Append(&authenticator_data, application_parameter_);
  authenticator_data.insert(authenticator_data.end(), flags_);
  fido_parsing_utils::Append(&authenticator_data, counter_);

  if (attested_data_) {
    // Attestations are returned in registration responses but not in assertion
    // responses.
    fido_parsing_utils::Append(&authenticator_data,
                               attested_data_->SerializeAsBytes());
  }

  if (extensions_) {
    const auto maybe_extensions = cbor::Writer::Write(*extensions_);
    if (maybe_extensions) {
      fido_parsing_utils::Append(&authenticator_data, *maybe_extensions);
    }
  }

  return authenticator_data;
}

std::vector<uint8_t> AuthenticatorData::GetCredentialId() const {
  if (!attested_data_)
    return std::vector<uint8_t>();

  return attested_data_->credential_id();
}

}  // namespace device
