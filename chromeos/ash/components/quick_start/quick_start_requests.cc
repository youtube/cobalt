// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_requests.h"
#include "base/base64.h"
#include "base/json/json_writer.h"
#include "chromeos/ash/components/quick_start/proto/aes_gcm_authentication_message.pb.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_message_type.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/aead.h"
#include "crypto/sha2.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::quick_start::requests {

namespace {

namespace proto = ::quick_start::proto;

// bootstrapOptions key telling the phone how to handle
// challenge UI in case of fallback.
constexpr char kFlowTypeKey[] = "flowType";
// bootstrapOptions key telling the phone the number of
// accounts are expected to transfer account to the target device.
constexpr char kAccountRequirementKey[] = "accountRequirement";

// Base64 encoded CBOR bytes containing the Fido command. This will be used
// for GetInfo and GetAssertion.
static constexpr char FIDO_MESSAGE_KEY[] = "fidoMessage";

// Maps to AccountRequirementSingle enum value for Account Requirement field
// meaning that at least one account is required on the phone. The user will
// select the specified account to transfer.
// Enum Source: go/bootstrap-options-account-requirement-single.
constexpr int kAccountRequirementSingle = 2;

// Maps to FlowTypeTargetChallenge enum value for Flow Type field meaning that
// the fallback challenge will happen on the target device.
// Enum Source: go/bootstrap-options-flow-type-target-challenge.
constexpr int kFlowTypeTargetChallenge = 2;

const char kRelyingPartyId[] = "google.com";
const char kCtapRequestType[] = "webauthn.get";
const char kOrigin[] = "https://accounts.google.com";

// Maps to CBOR byte labelling FIDO request as GetAssertion.
const uint8_t kAuthenticatorGetAssertionCommand = 0x02;
const char kUserPresenceMapKey[] = "up";
const char kUserVerificationMapKey[] = "uv";

// Maps to CBOR byte labelling FIDO request as GetInfo.
const uint8_t kAuthenticatorGetInfoCommand = 0x04;

// Boolean in WifiCredentialsRequest indicating we should request WiFi
// Credentials
constexpr char kRequestWifiKey[] = "request_wifi";

// Key in WifiCredentialsRequest and NotifySourceOfUpdateMessage including the
// shared secret to resume the connection if a reboot occurs.
constexpr char kSharedSecretKey[] = "shared_secret";

// Key in WifiCredentialsRequest and NotifySourceOfUpdateMessage for the session
// ID
constexpr char kSessionIdKey[] = "SESSION_ID";

// The role that should be used for the target device. See this enum:
// http://google3/java/com/google/android/gms/smartdevice/d2d/proto/aes_gcm_authentication_message.proto;l=26;rcl=489093041
constexpr int32_t kAuthPayloadTargetDeviceRole = 1;

// Boolean in NotifySourceOfUpdateMessage indicating target device requires an
// update.
constexpr char kNotifySourceOfUpdateMessageKey[] = "forced_update_required";

}  // namespace

std::unique_ptr<QuickStartMessage> BuildBootstrapOptionsRequest() {
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kBootstrapConfigurations);
  message->GetPayload()->Set(kAccountRequirementKey, kAccountRequirementSingle);
  message->GetPayload()->Set(kFlowTypeKey, kFlowTypeTargetChallenge);
  return message;
}

std::unique_ptr<QuickStartMessage> BuildAssertionRequestMessage(
    const std::string& challenge_b64url) {
  cbor::Value request = GenerateGetAssertionRequest(challenge_b64url);
  std::vector<uint8_t> ctap_request_command =
      CBOREncodeGetAssertionRequest(std::move(request));

  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kSecondDeviceAuthPayload);

  message->GetPayload()->Set(FIDO_MESSAGE_KEY,
                             base::Base64Encode(ctap_request_command));
  return message;
}

std::unique_ptr<QuickStartMessage> BuildGetInfoRequestMessage() {
  std::vector<uint8_t> ctap_request_command({kAuthenticatorGetInfoCommand});
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kSecondDeviceAuthPayload);
  message->GetPayload()->Set(FIDO_MESSAGE_KEY,
                             base::Base64Encode(ctap_request_command));
  return message;
}

std::unique_ptr<QuickStartMessage> BuildRequestWifiCredentialsMessage(
    int32_t session_id,
    std::string& shared_secret) {
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kQuickStartPayload);
  message->GetPayload()->Set(kRequestWifiKey, true);
  std::string shared_secret_str(shared_secret.begin(), shared_secret.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);
  message->GetPayload()->Set(kSharedSecretKey, shared_secret_base64);
  message->GetPayload()->Set(kSessionIdKey, session_id);

  return message;
}

std::string CreateFidoClientDataJson(const url::Origin& origin,
                                     const std::string& challenge_b64url) {
  base::Value::Dict fido_collected_client_data;
  fido_collected_client_data.Set("type", kCtapRequestType);
  fido_collected_client_data.Set("challenge", challenge_b64url);
  fido_collected_client_data.Set("origin", origin.Serialize());
  fido_collected_client_data.Set("crossOrigin", false);
  std::string fido_client_data_json;
  base::JSONWriter::Write(fido_collected_client_data, &fido_client_data_json);
  return fido_client_data_json;
}

cbor::Value GenerateGetAssertionRequest(const std::string& challenge_b64url) {
  url::Origin origin = url::Origin::Create(GURL(kOrigin));
  std::string client_data_json =
      CreateFidoClientDataJson(origin, challenge_b64url);
  cbor::Value::MapValue cbor_map;
  cbor_map.insert_or_assign(cbor::Value(0x01), cbor::Value(kRelyingPartyId));
  std::array<uint8_t, crypto::kSHA256Length> client_data_hash;
  crypto::SHA256HashString(client_data_json, client_data_hash.data(),
                           client_data_hash.size());
  cbor_map.insert_or_assign(cbor::Value(0x02), cbor::Value(client_data_hash));
  cbor::Value::MapValue option_map;
  option_map.insert_or_assign(cbor::Value(kUserPresenceMapKey),
                              cbor::Value(true));
  option_map.insert_or_assign(cbor::Value(kUserVerificationMapKey),
                              cbor::Value(true));
  cbor_map.insert_or_assign(cbor::Value(0x05),
                            cbor::Value(std::move(option_map)));
  return cbor::Value(std::move(cbor_map));
}

std::vector<uint8_t> CBOREncodeGetAssertionRequest(const cbor::Value& request) {
  // Encode the CtapGetAssertionRequest into cbor bytes vector.
  absl::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(request);
  DCHECK(cbor_bytes);
  std::vector<uint8_t> request_bytes = std::move(*cbor_bytes);
  // Add the command byte to the beginning of this now fully encoded cbor bytes
  // vector.
  request_bytes.insert(request_bytes.begin(),
                       kAuthenticatorGetAssertionCommand);
  return request_bytes;
}

std::vector<uint8_t> BuildTargetDeviceHandshakeMessage(
    const std::string& authentication_token,
    std::array<uint8_t, 32> secret,
    std::array<uint8_t, 12> nonce) {
  proto::V1Message::AuthenticationPayload auth_payload;
  auth_payload.set_role(kAuthPayloadTargetDeviceRole);
  auth_payload.set_auth_string(authentication_token);

  std::string unencrypted_payload;
  auth_payload.SerializeToString(&unencrypted_payload);

  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(secret);
  std::vector<uint8_t> encrypted_payload =
      aead.Seal(std::vector<uint8_t>(unencrypted_payload.begin(),
                                     unencrypted_payload.end()),
                nonce, /*additional_data=*/std::vector<uint8_t>());

  proto::AesGcmAuthenticationMessage auth_message;
  auth_message.set_version(proto::AesGcmAuthenticationMessage::V1);
  proto::V1Message* v1_message = auth_message.mutable_v1();
  v1_message->set_nonce(std::string(nonce.begin(), nonce.end()));
  v1_message->set_payload(
      std::string(encrypted_payload.begin(), encrypted_payload.end()));

  std::string auth_message_serialized;
  auth_message.SerializeToString(&auth_message_serialized);

  return std::vector<uint8_t>(auth_message_serialized.begin(),
                              auth_message_serialized.end());
}

std::unique_ptr<QuickStartMessage> BuildNotifySourceOfUpdateMessage(
    int32_t session_id,
    std::string& shared_secret) {
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kQuickStartPayload);
  message->GetPayload()->Set(kNotifySourceOfUpdateMessageKey, true);

  std::string shared_secret_str(shared_secret.begin(), shared_secret.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);
  message->GetPayload()->Set(kSharedSecretKey, shared_secret_base64);
  message->GetPayload()->Set(kSessionIdKey, session_id);

  return message;
}
}  // namespace ash::quick_start::requests
