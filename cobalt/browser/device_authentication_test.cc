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

#include "base/base64.h"
#include "base/base64url.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {

constexpr size_t kSHA256DigestSize = 32;

namespace {

std::string ToBase64Message(const std::string& cert_scope,
                            const std::string& start_time) {
  std::string base64_message;
  CHECK(base::Base64Encode(browser::ComputeMessage(cert_scope, start_time),
                           &base64_message));
  return base64_message;
}

std::string ComputeBase64URLSignatureFromBase64Message(
    const std::string& base64_message, const std::string& base64_secret_key) {
  uint8_t signature[kSHA256DigestSize];

  std::string message;
  CHECK(base::Base64Decode(base64_message, &message));
  ComputeHMACSHA256SignatureWithProvidedKey(message, base64_secret_key,
                                            signature, kSHA256DigestSize);

  std::string base_64_url_signature;
  base::Base64UrlEncode(std::string(signature, signature + kSHA256DigestSize),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base_64_url_signature);
  return base_64_url_signature;
}

std::string CreateSignedURLQueryString(const std::string& cert_scope,
                                       const std::string& start_time,
                                       const std::string& base64_secret_key) {
  return GetDeviceAuthenticationSignedURLQueryStringFromComponents(
      cert_scope, start_time,
      ComputeBase64URLSignatureFromBase64Message(
          ToBase64Message(cert_scope, start_time), base64_secret_key));
}

}  // namespace

TEST(DeviceAuthenticationTest, ComputeMessageTest) {
  EXPECT_EQ(
      "AAAACmNlcnRfc2NvcGUAAAANbXktY2VydC1zY29wZQAAAApzdGFydF90aW1lAAAACjE1NjUx"
      "NjE1NTY=",
      ToBase64Message("my-cert-scope", "1565161556"));
  EXPECT_EQ(
      "AAAACmNlcnRfc2NvcGUAAAATbXktb3RoZXItY2VydC1zY29wZQAAAApzdGFydF90aW1lAAAA"
      "CjE1NjUxNjMwNjU=",
      ToBase64Message("my-other-cert-scope", "1565163065"));
  EXPECT_EQ(
      "AAAACmNlcnRfc2NvcGUAAAATbXktb3RoZXItY2VydC1zY29wZQAAAApzdGFydF90aW1lAAAA"
      "CjE1NjUxNjMyNDU=",
      ToBase64Message("my-other-cert-scope", "1565163245"));
  EXPECT_EQ(
      "AAAACmNlcnRfc2NvcGUAAAATbXktb3RoZXItY2VydC1zY29wZQAAAApzdGFydF90aW1lAAAA"
      "CjI3OTAzMjY2Mzk=",
      ToBase64Message("my-other-cert-scope", "2790326639"));
  EXPECT_EQ(
      "AAAACmNlcnRfc2NvcGUAAAAEeWFjcwAAAApzdGFydF90aW1lAAAACjEzNDAwMDAzMTI=",
      ToBase64Message("yacs", "1340000312"));
}

TEST(DeviceAuthenticationTest, ComputeSignatureWithProvidedSecretTest) {
  EXPECT_EQ("5duoLo5TELzyMQ6Fz-nmtLH3-nR4GrYfJ5RqTWU33LY",
            ComputeBase64URLSignatureFromBase64Message(
                "AAAACmNlcnRfc2NvcGUAAAANbXktY2VydC1zY29wZQAAAApzdGFydF90aW1lAA"
                "AACjE1NjUxNjE1NTY=",
                "abcdefghijklmnop1234567890abcdefghijklmnopqr"));
  EXPECT_EQ("zrHJxDjsg60vF-fnGD9J7QaK2fw2QEZD7PIIfHMYmUg",
            ComputeBase64URLSignatureFromBase64Message(
                "AAAACmNlcnRfc2NvcGUAAAATbXktb3RoZXItY2VydC1zY29wZQAAAApzdGFydF"
                "90aW1lAAAACjE1NjUxNjMwNjU=",
                "abcdefghijklmnop1234567890abcdefghijklmnopqr"));
  EXPECT_EQ("FnycnpFnmzgcNBIdJoymQvrS0_1uet_bmCtpuAmQR2s",
            ComputeBase64URLSignatureFromBase64Message(
                "AAAACmNlcnRfc2NvcGUAAAATbXktb3RoZXItY2VydC1zY29wZQAAAApzdGFydF"
                "90aW1lAAAACjE1NjUxNjMyNDU=",
                "abcdefghi5555nop1234567890abcdef5555klmn6666"));
  EXPECT_EQ("N-nDY11_8HWFKEZPiQenQrC3SXuuPahs-er-n7Xh6ig",
            ComputeBase64URLSignatureFromBase64Message(
                "AAAACmNlcnRfc2NvcGUAAAATbXktb3RoZXItY2VydC1zY29wZQAAAApzdGFydF"
                "90aW1lAAAACjI3OTAzMjY2Mzk=",
                "abcdefghi5555nop1234567890abcdef5555klmn6666"));
  EXPECT_EQ("dX0apAEsG2yWm9da6qTded4qd-mqgExeuJam99z-AHk",
            ComputeBase64URLSignatureFromBase64Message(
                "AAAACmNlcnRfc2NvcGUAAAAEeWFjcwAAAApzdGFydF90aW1lAAAACjEzNDAwMD"
                "AzMTI=",
                "11111111111111111111111111111111111111111111"));
}

// This is the main end-to-end test for device authentication.
TEST(DeviceAuthenticationTest,
     GetDeviceAuthenticationSignedURLQueryStringFromComponentsTest) {
  EXPECT_EQ(
      "cert_scope=my-cert-scope&sig=5duoLo5TELzyMQ6Fz-nmtLH3-"
      "nR4GrYfJ5RqTWU33LY&start_time=1565161556",
      CreateSignedURLQueryString(
          "my-cert-scope", "1565161556",
          "abcdefghijklmnop1234567890abcdefghijklmnopqr"));
  EXPECT_EQ(
      "cert_scope=my-other-cert-scope&sig="
      "Lf2zunrdhjH8q3ehdUy0tneTtamWigcyTgQl7zxWgnQ&start_time=123456789",
      CreateSignedURLQueryString(
          "my-other-cert-scope", "123456789",
          "abcdefghijklmnop1234567890abcdefghijklmnopqr"));
  EXPECT_EQ(
      "cert_scope=my-other-cert-scope&sig="
      "c5YZB0Rtv8Nf8gLSbE052ZvCUEpouP28nUq77FEgg88&start_time=11111111",
      CreateSignedURLQueryString(
          "my-other-cert-scope", "11111111",
          "11111111111111111111111111111111111111111111"));
  EXPECT_EQ(
      "cert_scope=yacs&sig=YKjLEzSl2_ub-05Ajaww0HOOPElxEPUc4SEiQnGAfaE&start_"
      "time=11111111",
      CreateSignedURLQueryString(
          "yacs", "11111111", "11111111111111111111111111111111111111111111"));
}

TEST(DeviceAuthenticationTest, NoCertSignatureImpliesNoQueryParameters) {
  EXPECT_EQ("", GetDeviceAuthenticationSignedURLQueryStringFromComponents(
                    "my_cert_scope", "1234", ""));
}

}  // namespace browser
}  // namespace cobalt
