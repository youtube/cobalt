// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_command_constructor.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "components/apdu/apdu_command.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

bool IsConvertibleToU2fRegisterCommand(
    const CtapMakeCredentialRequest& request) {
  if (request.user_verification == UserVerificationRequirement::kRequired ||
      request.resident_key_required)
    return false;

  return base::Contains(
      request.public_key_credential_params.public_key_credential_params(),
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256),
      &PublicKeyCredentialParams::CredentialInfo::algorithm);
}

bool ShouldPreferCTAP2EvenIfItNeedsAPIN(
    const CtapMakeCredentialRequest& request) {
  return request.hmac_secret ||
         // U2F devices can only support |kEnterpriseApprovedByBrowser| so
         // |kEnterpriseIfRPListedOnAuthenticator| should go over CTAP2.
         request.attestation_preference ==
             AttestationConveyancePreference::
                 kEnterpriseIfRPListedOnAuthenticator;
}

bool IsConvertibleToU2fSignCommand(const CtapGetAssertionRequest& request) {
  return request.user_verification != UserVerificationRequirement::kRequired &&
         !request.allow_list.empty();
}

absl::optional<std::vector<uint8_t>> ConvertToU2fRegisterCommand(
    const CtapMakeCredentialRequest& request) {
  if (!IsConvertibleToU2fRegisterCommand(request))
    return absl::nullopt;

  if (request.pin_auth && request.pin_auth->size() == 0) {
    // An empty pin_auth in CTAP2 indicates that the device should just wait
    // for a touch.
    return ConstructBogusU2fRegistrationCommand();
  }

  const bool is_individual_attestation =
      request.attestation_preference ==
      AttestationConveyancePreference::kEnterpriseApprovedByBrowser;
  return ConstructU2fRegisterCommand(
      fido_parsing_utils::CreateSHA256Hash(request.rp.id),
      request.client_data_hash, is_individual_attestation);
}

absl::optional<std::vector<uint8_t>> ConvertToU2fSignCommandWithBogusChallenge(
    const CtapMakeCredentialRequest& request,
    base::span<const uint8_t> key_handle) {
  return ConstructU2fSignCommand(
      fido_parsing_utils::CreateSHA256Hash(request.rp.id),
      kBogusChallenge, key_handle);
}

absl::optional<std::vector<uint8_t>> ConvertToU2fSignCommand(
    const CtapGetAssertionRequest& request,
    ApplicationParameterType application_parameter_type,
    base::span<const uint8_t> key_handle) {
  if (!IsConvertibleToU2fSignCommand(request))
    return absl::nullopt;

  const auto& application_parameter =
      application_parameter_type == ApplicationParameterType::kPrimary
          ? fido_parsing_utils::CreateSHA256Hash(request.rp_id)
          : request.alternative_application_parameter.value_or(
                std::array<uint8_t, kRpIdHashLength>());

  return ConstructU2fSignCommand(application_parameter,
                                 request.client_data_hash, key_handle);
}

std::vector<uint8_t> ConstructU2fRegisterCommand(
    base::span<const uint8_t, kU2fApplicationParamLength> application_parameter,
    base::span<const uint8_t, kU2fChallengeParamLength> challenge_parameter,
    bool is_individual_attestation) {
  std::vector<uint8_t> data;
  data.reserve(kU2fChallengeParamLength + kU2fApplicationParamLength);
  fido_parsing_utils::Append(&data, challenge_parameter);
  fido_parsing_utils::Append(&data, application_parameter);

  apdu::ApduCommand command;
  command.set_ins(base::strict_cast<uint8_t>(U2fApduInstruction::kRegister));
  command.set_p1(kP1TupRequiredConsumed |
                 (is_individual_attestation ? kP1IndividualAttestation : 0));
  command.set_data(std::move(data));
  command.set_response_length(apdu::ApduCommand::kApduMaxResponseLength);
  return command.GetEncodedCommand();
}

absl::optional<std::vector<uint8_t>> ConstructU2fSignCommand(
    base::span<const uint8_t, kU2fApplicationParamLength> application_parameter,
    base::span<const uint8_t, kU2fChallengeParamLength> challenge_parameter,
    base::span<const uint8_t> key_handle) {
  if (key_handle.size() > kMaxKeyHandleLength) {
    return absl::nullopt;
  }

  std::vector<uint8_t> data;
  data.reserve(kU2fChallengeParamLength + kU2fApplicationParamLength + 1 +
               key_handle.size());
  fido_parsing_utils::Append(&data, challenge_parameter);
  fido_parsing_utils::Append(&data, application_parameter);
  data.push_back(static_cast<uint8_t>(key_handle.size()));
  fido_parsing_utils::Append(&data, key_handle);

  apdu::ApduCommand command;
  command.set_ins(base::strict_cast<uint8_t>(U2fApduInstruction::kSign));
  command.set_p1(kP1TupRequiredConsumed);
  command.set_data(std::move(data));
  command.set_response_length(apdu::ApduCommand::kApduMaxResponseLength);
  return command.GetEncodedCommand();
}

std::vector<uint8_t> ConstructBogusU2fRegistrationCommand() {
  return ConstructU2fRegisterCommand(kBogusAppParam, kBogusChallenge);
}

}  // namespace device
