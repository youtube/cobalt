// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/device_authentication.h"

#include <algorithm>
#include <map>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "crypto/hmac.h"
#include "net/base/escape.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {

namespace {

constexpr size_t kSHA256DigestSize = 32;

#if SB_API_VERSION < 13
bool ComputeSignatureWithSystemPropertySecret(const std::string& message,
                                              uint8_t* signature) {
  const size_t kBase64EncodedCertificationSecretLength = 1023;
  char base_64_secret_property[kBase64EncodedCertificationSecretLength + 1] = {
      0};
  bool result = SbSystemGetProperty(
      kSbSystemPropertyBase64EncodedCertificationSecret,
      base_64_secret_property, kBase64EncodedCertificationSecretLength);
  if (!result) {
    return false;
  }

  ComputeHMACSHA256SignatureWithProvidedKey(message, base_64_secret_property,
                                            signature, kSHA256DigestSize);
  return true;
}
#endif  // SB_API_VERSION < 13

bool ComputeSignatureFromSignAPI(const std::string& message,
                                 uint8_t* signature) {
  return SbSystemSignWithCertificationSecretKey(
      reinterpret_cast<const uint8_t*>(message.data()), message.size(),
      signature, kSHA256DigestSize);
}

// Check to see if we can query the platform for the secret key.  If so,
// go ahead and use it to sign the message, otherwise try to use the
// SbSystemSignWithCertificationSecretKey() method to sign the message.  If
// both methods fail, return an empty string.
std::string ComputeBase64Signature(const std::string& message) {
  uint8_t signature[kSHA256DigestSize];

  if (ComputeSignatureFromSignAPI(message, signature)) {
    DLOG(INFO) << "Using certification signature provided by "
               << "SbSystemSignWithCertificationSecretKey().";
#if SB_API_VERSION < 13
  } else if (ComputeSignatureWithSystemPropertySecret(message, signature)) {
    DLOG(INFO) << "Using certification key from SbSystemGetProperty().";
#endif  // SB_API_VERSION < 13
  } else {
    return std::string();
  }

  std::string base_64_url_signature;
  base::Base64UrlEncode(std::string(signature, signature + kSHA256DigestSize),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base_64_url_signature);
  return base_64_url_signature;
}

std::string NumberToFourByteString(size_t n) {
  std::string str;
  str += static_cast<char>(((n & 0xff000000) >> 24));
  str += static_cast<char>(((n & 0x00ff0000) >> 16));
  str += static_cast<char>(((n & 0x0000ff00) >> 8));
  str += static_cast<char>((n & 0x000000ff));
  return str;
}

// Used by ComputeMessage() to create a message component as a string.
std::string BuildMessageFragment(const std::string& key,
                                 const std::string& value) {
  std::string msg_fragment = NumberToFourByteString(key.length()) + key +
                             NumberToFourByteString(value.length()) + value;
  return msg_fragment;
}

// Returns the certification scope provided by the platform to use with device
// authentication.
std::string GetCertScopeFromPlatform() {
  // Get cert_scope and base_64_secret
  const size_t kCertificationScopeLength = 1023;
  char cert_scope_property[kCertificationScopeLength + 1] = {0};
  bool result =
      SbSystemGetProperty(kSbSystemPropertyCertificationScope,
                          cert_scope_property, kCertificationScopeLength);
  if (!result) {
    DLOG(ERROR) << "Unable to get kSbSystemPropertyCertificationScope";
    return std::string();
  }

  return cert_scope_property;
}

// Returns the start time provided by the platform for use with device
// authentication.
std::string GetStartTime() {
  return std::to_string(static_cast<int64_t>(base::Time::Now().ToDoubleT()));
}

}  // namespace

std::string GetDeviceAuthenticationSignedURLQueryString() {
  std::string cert_scope = GetCertScopeFromPlatform();
  if (cert_scope.empty()) {
    LOG(WARNING) << "Error retrieving certification scope required for "
                 << "device authentication.";
    return std::string();
  }
  std::string start_time = GetStartTime();
  CHECK(!start_time.empty());

  std::string base64_signature =
      ComputeBase64Signature(ComputeMessage(cert_scope, start_time));

  return GetDeviceAuthenticationSignedURLQueryStringFromComponents(
      cert_scope, start_time, base64_signature);
}

std::string GetDeviceAuthenticationSignedURLQueryStringFromComponents(
    const std::string& cert_scope, const std::string& start_time,
    const std::string& base64_signature) {
  CHECK(!cert_scope.empty());
  CHECK(!start_time.empty());

  if (base64_signature.empty()) {
    return std::string();
  }

  std::map<std::string, std::string> signed_query_components;
  signed_query_components["cert_scope"] = cert_scope;
  signed_query_components["start_time"] = start_time;
  signed_query_components["sig"] = base64_signature;

  std::string query;
  for (const auto& query_component : signed_query_components) {
    const std::string& key = query_component.first;
    const std::string& value = query_component.second;

    if (!query.empty()) query += "&";
    query += net::EscapeQueryParamValue(key, true);
    if (!value.empty()) {
      query += "=" + net::EscapeQueryParamValue(value, true);
    }
  }

  return query;
}

// Combine multiple message components into a string that will be used as the
// message that we will sign.
std::string ComputeMessage(const std::string& cert_scope,
                           const std::string& start_time) {
  // Build message from cert_scope and start_time.
  return BuildMessageFragment("cert_scope", cert_scope) +
         BuildMessageFragment("start_time", start_time);
}

void ComputeHMACSHA256SignatureWithProvidedKey(const std::string& message,
                                               const std::string& base64_key,
                                               uint8_t* signature,
                                               size_t signature_size_in_bytes) {
  CHECK_GE(signature_size_in_bytes, 32U);

  std::string key;
  base::Base64Decode(base64_key, &key);

  // Generate signature from message using HMAC-SHA256.
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(key)) {
    DLOG(ERROR) << "Unable to initialize HMAC-SHA256.";
  }
  if (!hmac.Sign(message, signature, signature_size_in_bytes)) {
    DLOG(ERROR) << "Unable to sign HMAC-SHA256.";
  }
}

}  // namespace browser
}  // namespace cobalt
