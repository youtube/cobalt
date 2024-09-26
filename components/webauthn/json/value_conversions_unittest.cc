// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/json/value_conversions.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/json/json_string_value_serializer.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-shared.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace webauthn {
namespace {

using blink::mojom::CommonCredentialInfo;
using blink::mojom::CommonCredentialInfoPtr;
using blink::mojom::GetAssertionAuthenticatorResponse;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::MakeCredentialAuthenticatorResponse;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;
using blink::mojom::PublicKeyCredentialCreationOptions;
using blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::PublicKeyCredentialRequestOptions;
using blink::mojom::PublicKeyCredentialRequestOptionsPtr;
using blink::mojom::RemoteDesktopClientOverride;
using blink::mojom::RemoteDesktopClientOverridePtr;

std::vector<uint8_t> ToByteVector(base::StringPiece in) {
  const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(in.data());
  return std::vector<uint8_t>(in_ptr, in_ptr + in.size());
}

constexpr char kAppId[] = "https://example.test/appid.json";
static const std::vector<uint8_t> kChallenge = ToByteVector("test challenge");
constexpr char kOrigin[] = "https://login.example.test/";
constexpr char kRpId[] = "example.test";
constexpr char kRpName[] = "Example LLC";
static const base::TimeDelta kTimeout = base::Seconds(30);
constexpr char kUserDisplayName[] = "Example User";
static const std::vector<uint8_t> kUserId = ToByteVector("test user id");
constexpr char kUserName[] = "user@example.test";

std::vector<device::PublicKeyCredentialParams::CredentialInfo>
GetPublicKeyCredentialParameters() {
  return {
      device::PublicKeyCredentialParams::CredentialInfo{
          device ::CredentialType::kPublicKey,
          base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256)},
      device::PublicKeyCredentialParams::CredentialInfo{
          device ::CredentialType::kPublicKey,
          base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kRs256)}};
}

std::vector<device::PublicKeyCredentialDescriptor> GetCredentialList() {
  return {device::PublicKeyCredentialDescriptor(
              device::CredentialType::kPublicKey, {20, 21, 22},
              {device::FidoTransportProtocol::kUsbHumanInterfaceDevice}),
          device::PublicKeyCredentialDescriptor(
              device::CredentialType::kPublicKey, {30, 31, 32}, {})};
}

TEST(WebAuthenticationJSONConversionTest,
     PublicKeyCredentialCreationOptionsToValue) {
  // Exercise all supported fields.
  auto options = PublicKeyCredentialCreationOptions::New(
      device::PublicKeyCredentialRpEntity(kRpId, kRpName),
      device::PublicKeyCredentialUserEntity(kUserId, kUserName,
                                            kUserDisplayName),
      kChallenge, GetPublicKeyCredentialParameters(), kTimeout,
      GetCredentialList(),
      device::AuthenticatorSelectionCriteria(
          device::AuthenticatorAttachment::kPlatform,
          device::ResidentKeyRequirement::kRequired,
          device::UserVerificationRequirement::kRequired),
      device::AttestationConveyancePreference::kDirect,
      /*hmac_create_secret=*/true,
      /*prf_enable=*/false, blink::mojom::ProtectionPolicy::UV_REQUIRED,
      /*enforce_protection_policy=*/true,
      /*appid_exclude=*/kAppId,
      /*cred_props=*/true, device::LargeBlobSupport::kRequired,
      /*is_payment_credential_creation=*/false,
      /*cred_blob=*/ToByteVector("test cred blob"),
      /*min_pin_length_requested=*/true,
      blink::mojom::RemoteDesktopClientOverride::New(
          url::Origin::Create(GURL(kOrigin)),
          /*same_origin_with_ancestors=*/true),
      // TODO(crbug.com/1356340): support devicePubKey in JSON when it's stable.
      /*device_public_key=*/nullptr);

  base::Value value = ToValue(options);
  std::string json;
  JSONStringValueSerializer serializer(&json);
  ASSERT_TRUE(serializer.Serialize(value));
  EXPECT_EQ(
      json,
      R"({"attestation":"direct","authenticatorSelection":{"authenticatorAttachment":"platform","residentKey":"required","userVerification":"required"},"challenge":"dGVzdCBjaGFsbGVuZ2U","excludeCredentials":[{"id":"FBUW","transports":["usb"],"type":"public-key"},{"id":"Hh8g","type":"public-key"}],"extensions":{"appIdExclude":"https://example.test/appid.json","credBlob":"dGVzdCBjcmVkIGJsb2I","credProps":true,"credentialProtectionPolicy":"userVerificationRequired","enforceCredentialProtectionPolicy":true,"hmacCreateSecret":true,"largeBlob":{"support":"required"},"minPinLength":true,"remoteDesktopClientOverride":{"origin":"https://login.example.test","sameOriginWithAncestors":true}},"pubKeyCredParams":[{"alg":-7,"type":"public-key"},{"alg":-257,"type":"public-key"}],"rp":{"id":"example.test","name":"Example LLC"},"user":{"displayName":"Example User","id":"dGVzdCB1c2VyIGlk","name":"user@example.test"}})");
}

TEST(WebAuthenticationJSONConversionTest,
     PublicKeyCredentialRequestOptionsToValue) {
  // Exercise all supported fields.
  auto options = PublicKeyCredentialRequestOptions::New(
      /*is_conditional=*/false, kChallenge, kTimeout, kRpId,
      GetCredentialList(), device::UserVerificationRequirement::kRequired,
      kAppId,
      std::vector<device::CableDiscoveryData>{
          {device::CableDiscoveryData::Version::V1, device::CableEidArray{},
           device::CableEidArray{}, device::CableSessionPreKeyArray{}}},
#if BUILDFLAG(IS_ANDROID)
      /*user_verification_methods=*/false,
#endif
      /*prf=*/false, std::vector<blink::mojom::PRFValuesPtr>(),
      /*large_blob_read=*/true,
      /*large_blob_write=*/std::vector<uint8_t>{8, 9, 10},
      /*get_cred_blob=*/true,
      blink::mojom::RemoteDesktopClientOverride::New(
          url::Origin::Create(GURL(kOrigin)),
          /*same_origin_with_ancestors=*/true),
      // TODO: support devicePubKey in JSON when it's stable.
      /*device_public_key=*/nullptr);

  base::Value value = ToValue(options);
  std::string json;
  JSONStringValueSerializer serializer(&json);
  ASSERT_TRUE(serializer.Serialize(value));
  EXPECT_EQ(
      json,
      R"({"allowCredentials":[{"id":"FBUW","transports":["usb"],"type":"public-key"},{"id":"Hh8g","type":"public-key"}],"challenge":"dGVzdCBjaGFsbGVuZ2U","extensions":{"appid":"https://example.test/appid.json","cableAuthentication":[{"authenticatorEid":"AAAAAAAAAAAAAAAAAAAAAA","clientEid":"AAAAAAAAAAAAAAAAAAAAAA","sessionPreKey":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA","version":1}],"getCredBlob":true,"largeBlob":{"read":true,"write":"CAkK"},"remoteDesktopClientOverride":{"origin":"https://login.example.test","sameOriginWithAncestors":true}},"rpId":"example.test","userVerification":"required"})");
}

TEST(WebAuthenticationJSONConversionTest,
     AuthenticatorAttestationResponseFromValue) {
  // The following values appear Base64URL-encoded in `json`.
  static const std::vector<uint8_t> kAttestationObject = {
      0xA3, 0x63, 0x66, 0x6D, 0x74, 0x64, 0x6E, 0x6F, 0x6E, 0x65, 0x67, 0x61,
      0x74, 0x74, 0x53, 0x74, 0x6D, 0x74, 0xA0, 0x68, 0x61, 0x75, 0x74, 0x68,
      0x44, 0x61, 0x74, 0x61, 0x58, 0xA4, 0x26, 0xBD, 0x72, 0x78, 0xBE, 0x46,
      0x37, 0x61, 0xF1, 0xFA, 0xA1, 0xB1, 0x0A, 0xB4, 0xC4, 0xF8, 0x26, 0x70,
      0x26, 0x9C, 0x41, 0x0C, 0x72, 0x6A, 0x1F, 0xD6, 0xE0, 0x58, 0x55, 0xE1,
      0x9B, 0x46, 0x5D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x20, 0x04, 0x27, 0xA9, 0x01, 0x28, 0xEE, 0x83, 0xD0, 0x8F, 0x2F, 0xA9,
      0xBA, 0x93, 0xB3, 0x2F, 0x7F, 0x9B, 0xA8, 0x21, 0x63, 0xB1, 0x09, 0x25,
      0xC4, 0x6A, 0x54, 0x2D, 0xF3, 0xAB, 0x9C, 0x6E, 0x96, 0xA5, 0x01, 0x02,
      0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20, 0x96, 0x45, 0xF0, 0x5D, 0xE1,
      0x37, 0x98, 0xD8, 0x63, 0x4E, 0x52, 0x96, 0xBD, 0x17, 0xF7, 0xAF, 0xB3,
      0x5E, 0xC4, 0xF4, 0x65, 0xAD, 0x7E, 0x65, 0x78, 0xE8, 0x44, 0xEE, 0xBD,
      0xB9, 0x12, 0xF5, 0x22, 0x58, 0x20, 0xF1, 0x93, 0x8C, 0x25, 0x36, 0xA0,
      0x3C, 0x27, 0xE5, 0xF3, 0x36, 0x75, 0x9F, 0x7E, 0xAA, 0xC4, 0x0F, 0x25,
      0x20, 0xE3, 0x86, 0xBD, 0x9A, 0xE7, 0xD4, 0x26, 0xA1, 0x07, 0xD1, 0xBE,
      0x0C, 0x02};
  static const std::vector<uint8_t> kAuthenticatorData = {
      0x26, 0xBD, 0x72, 0x78, 0xBE, 0x46, 0x37, 0x61, 0xF1, 0xFA, 0xA1, 0xB1,
      0x0A, 0xB4, 0xC4, 0xF8, 0x26, 0x70, 0x26, 0x9C, 0x41, 0x0C, 0x72, 0x6A,
      0x1F, 0xD6, 0xE0, 0x58, 0x55, 0xE1, 0x9B, 0x46, 0x5D, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x04, 0x27, 0xA9, 0x01, 0x28,
      0xEE, 0x83, 0xD0, 0x8F, 0x2F, 0xA9, 0xBA, 0x93, 0xB3, 0x2F, 0x7F, 0x9B,
      0xA8, 0x21, 0x63, 0xB1, 0x09, 0x25, 0xC4, 0x6A, 0x54, 0x2D, 0xF3, 0xAB,
      0x9C, 0x6E, 0x96, 0xA5, 0x01, 0x02, 0x03, 0x26, 0x20, 0x01, 0x21, 0x58,
      0x20, 0x96, 0x45, 0xF0, 0x5D, 0xE1, 0x37, 0x98, 0xD8, 0x63, 0x4E, 0x52,
      0x96, 0xBD, 0x17, 0xF7, 0xAF, 0xB3, 0x5E, 0xC4, 0xF4, 0x65, 0xAD, 0x7E,
      0x65, 0x78, 0xE8, 0x44, 0xEE, 0xBD, 0xB9, 0x12, 0xF5, 0x22, 0x58, 0x20,
      0xF1, 0x93, 0x8C, 0x25, 0x36, 0xA0, 0x3C, 0x27, 0xE5, 0xF3, 0x36, 0x75,
      0x9F, 0x7E, 0xAA, 0xC4, 0x0F, 0x25, 0x20, 0xE3, 0x86, 0xBD, 0x9A, 0xE7,
      0xD4, 0x26, 0xA1, 0x07, 0xD1, 0xBE, 0x0C, 0x02,
  };
  static const std::vector<uint8_t> kId = ToByteVector("test id");
  constexpr char kIdB64Url[] = "dGVzdCBpZA";  // base64url("test")
  static const std::vector<uint8_t> kClientDataJson =
      ToByteVector("test client data json");
  static const std::vector<uint8_t> kPublicKey = {
      0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02,
      0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03,
      0x42, 0x00, 0x04, 0x96, 0x45, 0xF0, 0x5D, 0xE1, 0x37, 0x98, 0xD8, 0x63,
      0x4E, 0x52, 0x96, 0xBD, 0x17, 0xF7, 0xAF, 0xB3, 0x5E, 0xC4, 0xF4, 0x65,
      0xAD, 0x7E, 0x65, 0x78, 0xE8, 0x44, 0xEE, 0xBD, 0xB9, 0x12, 0xF5, 0xF1,
      0x93, 0x8C, 0x25, 0x36, 0xA0, 0x3C, 0x27, 0xE5, 0xF3, 0x36, 0x75, 0x9F,
      0x7E, 0xAA, 0xC4, 0x0F, 0x25, 0x20, 0xE3, 0x86, 0xBD, 0x9A, 0xE7, 0xD4,
      0x26, 0xA1, 0x07, 0xD1, 0xBE, 0x0C, 0x02,
  };

  // Exercise every possible field in the result struct.
  constexpr char kJson[] = R"({
  "authenticatorAttachment": "platform",
  "clientExtensionResults": {
    "credBlob": true,
    "credProps": { "rk": true },
    "hmacCreateSecret": true,
    "largeBlob": { "supported": true }
  },
  "id": "dGVzdCBpZA",
  "rawId": "dGVzdCBpZA",
  "response": {
    "attestationObject": "o2NmbXRkbm9uZWdhdHRTdG10oGhhdXRoRGF0YVikJr1yeL5GN2Hx-qGxCrTE-CZwJpxBDHJqH9bgWFXhm0ZdAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAQnqQEo7oPQjy-pupOzL3-bqCFjsQklxGpULfOrnG6WpQECAyYgASFYIJZF8F3hN5jYY05Slr0X96-zXsT0Za1-ZXjoRO69uRL1Ilgg8ZOMJTagPCfl8zZ1n36qxA8lIOOGvZrn1CahB9G-DAI",
    "clientDataJSON": "dGVzdCBjbGllbnQgZGF0YSBqc29u",
    "transports": [ "usb" ]
  },
  "type": "public-key"
})";

  JSONStringValueDeserializer deserializer(kJson);
  std::string deserialize_error;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  ASSERT_TRUE(value) << deserialize_error;

  auto [response, error] = MakeCredentialResponseFromValue(*value);
  ASSERT_TRUE(response) << error;

  auto expected = MakeCredentialAuthenticatorResponse::New(
      CommonCredentialInfo::New(kIdB64Url, kId, kClientDataJson,
                                kAuthenticatorData),
      device::AuthenticatorAttachment::kPlatform, kAttestationObject,
      std::vector<device::FidoTransportProtocol>{
          device::FidoTransportProtocol::kUsbHumanInterfaceDevice},
      /*echo_hmac_create_secret=*/true, /*hmac_create_secret=*/true,
      /*echo_prf=*/false, /*prf=*/false, /*echo_cred_blob=*/true,
      /*cred_blob=*/true, /*public_key_der=*/kPublicKey,
      /*public_key_algo=*/-7,
      /*echo_cred_props=*/true, /*has_cred_props_rk=*/true,
      /*cred_props_rk=*/true, /*echo_large_blob=*/true,
      /*supports_large_blob=*/true,
      // TODO: support devicePubKey in JSON when it's stable.
      /*device_public_key=*/nullptr);

  EXPECT_EQ(response->info, expected->info);
  EXPECT_EQ(response->authenticator_attachment,
            expected->authenticator_attachment);
  EXPECT_EQ(response->attestation_object, expected->attestation_object);
  EXPECT_EQ(response->transports, expected->transports);
  EXPECT_EQ(response->echo_hmac_create_secret,
            expected->echo_hmac_create_secret);
  EXPECT_EQ(response->hmac_create_secret, expected->hmac_create_secret);
  EXPECT_EQ(response->echo_prf, expected->prf);
  EXPECT_EQ(response->echo_cred_blob, expected->echo_cred_blob);
  EXPECT_EQ(response->cred_blob, expected->cred_blob);
  EXPECT_EQ(response->public_key_der, expected->public_key_der);
  EXPECT_EQ(response->public_key_algo, expected->public_key_algo);
  EXPECT_EQ(response->echo_cred_props, expected->echo_cred_props);
  EXPECT_EQ(response->has_cred_props_rk, expected->has_cred_props_rk);
  EXPECT_EQ(response->cred_props_rk, expected->cred_props_rk);
  EXPECT_EQ(response->echo_large_blob, expected->echo_large_blob);
  EXPECT_EQ(response->supports_large_blob, expected->supports_large_blob);
  // Produce a failure even if the list above is missing any fields. But this
  // will not print any meaningful error.
  EXPECT_EQ(response, expected);
}

TEST(WebAuthenticationJSONConversionTest,
     AuthenticatorAssertionResponseFromValue) {
  // The following values appear Base64URL-encoded in `json`.
  static const std::vector<uint8_t> kAuthenticatorData =
      ToByteVector("test authenticator data");
  static const std::vector<uint8_t> kId = ToByteVector("test id");
  constexpr char kIdB64Url[] = "dGVzdCBpZA";  // base64url(kId)
  static const std::vector<uint8_t> kClientDataJson =
      ToByteVector("test client data json");
  static const std::vector<uint8_t> kCredBlob = ToByteVector("test cred blob");
  static const std::vector<uint8_t> kLargeBlob =
      ToByteVector("test large blob");
  static const std::vector<uint8_t> kSignature = ToByteVector("test signature");
  static const std::vector<uint8_t> kUserHandle =
      ToByteVector("test user handle");

  // Exercise every possible field in the result struct.
  constexpr char kJson[] = R"({
  "authenticatorAttachment": "cross-platform",
  "clientExtensionResults": {
    "appid": true,
    "getCredBlob": "dGVzdCBjcmVkIGJsb2I",
    "largeBlob": {
      "blob": "dGVzdCBsYXJnZSBibG9i",
      "written": true
    }
  },
  "id": "dGVzdCBpZA",
  "rawId": "dGVzdCBpZA",
  "response": {
    "authenticatorData": "dGVzdCBhdXRoZW50aWNhdG9yIGRhdGE",
    "clientDataJSON": "dGVzdCBjbGllbnQgZGF0YSBqc29u",
    "signature": "dGVzdCBzaWduYXR1cmU",
    "userHandle": "dGVzdCB1c2VyIGhhbmRsZQ"
  },
  "type": "public-key"
})";

  JSONStringValueDeserializer deserializer(kJson);
  std::string deserialize_error;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  ASSERT_TRUE(value) << deserialize_error;

  auto [response, error] = GetAssertionResponseFromValue(*value);
  ASSERT_TRUE(response) << error;

  auto expected = GetAssertionAuthenticatorResponse::New(
      CommonCredentialInfo::New(kIdB64Url, kId, kClientDataJson,
                                kAuthenticatorData),
      device::AuthenticatorAttachment::kCrossPlatform, kSignature, kUserHandle,
      /*echo_appid_extension=*/true, /*appid_extension=*/true,
#if BUILDFLAG(IS_ANDROID)
      /*echo_user_verification_methods=*/false,
      /*user_verification_methods=*/absl::nullopt,
#endif
      /*echo_prf=*/false, /*prf_results=*/nullptr, /*prf_not_evaluated=*/false,
      /*echo_large_blob=*/true,
      /*large_blob=*/kLargeBlob, /*echo_large_blob_written=*/true,
      /*large_blob_written=*/true,
      /*get_cred_blob=*/kCredBlob,
      // TODO: support devicePubKey in JSON when it's stable.
      /*device_public_key=*/nullptr);

  EXPECT_EQ(response->info, expected->info);
  EXPECT_EQ(response->authenticator_attachment,
            expected->authenticator_attachment);
  EXPECT_EQ(response->signature, expected->signature);
  EXPECT_EQ(response->user_handle, expected->user_handle);
  EXPECT_EQ(response->echo_appid_extension, expected->echo_appid_extension);
  EXPECT_EQ(response->appid_extension, expected->appid_extension);
  EXPECT_EQ(response->echo_prf, expected->echo_prf);
  EXPECT_EQ(response->prf_results, expected->prf_results);
  EXPECT_EQ(response->prf_not_evaluated, expected->prf_not_evaluated);
  EXPECT_EQ(response->echo_large_blob, expected->echo_large_blob);
  EXPECT_EQ(response->large_blob, expected->large_blob);
  EXPECT_EQ(response->echo_large_blob_written,
            expected->echo_large_blob_written);
  EXPECT_EQ(response->large_blob_written, expected->large_blob_written);
  EXPECT_EQ(response->get_cred_blob, expected->get_cred_blob);
  // Produce a failure even if the list above is missing any fields. But this
  // will not print any meaningful error.
  EXPECT_EQ(response, expected);
}

}  // namespace
}  // namespace webauthn
