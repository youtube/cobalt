// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/system.h"

#include <string.h>
#include <string>

#include "starboard/common/log.h"
#include "starboard/linux/x64x11/system_properties.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/modp_b64/modp_b64.h"

#include "starboard/shared/deviceauth/deviceauth_internal.h"

#if SB_API_VERSION >= 11

namespace {
bool Base64Decode(const std::string& input, std::string* output) {
  std::string temp;
  temp.resize(modp_b64_decode_len(input.size()));

  // does not null terminate result since result is binary data!
  size_t input_size = input.size();
  int output_size = modp_b64_decode(&(temp[0]), input.data(), input_size);
  if (output_size == MODP_B64_ERROR)
    return false;

  temp.resize(output_size);
  output->swap(temp);
  return true;
}
}  // namespace

namespace starboard {
namespace shared {
namespace deviceauth {

// This function is meant to exist purely for reference.
//
// Implementation that uses a compiled-in key.
// The application may already fallback to querying for the secret key and
// applying this same logic if this function returns false.  Because of that,
// if you are considering using this implementation in production, it's quite
// possible that simply using the stub version of this function would work
// just as well for you.  A more production-ready version of this function might
// forward to an internal API that signs the message in a hardware-backed secure
// keystore.
bool SignWithCertificationSecretKey(const char* secret_key,
                                    const uint8_t* message,
                                    size_t message_size_in_bytes,
                                    uint8_t* digest,
                                    size_t digest_size_in_bytes) {
  if (digest_size_in_bytes < 32) {
    SB_LOG(ERROR) << "SbSystemSignWithCertificationSecretKey() must be called "
                  << "with an output digest buffer size of at least 32 bytes.";
    return false;
  }
  const size_t kBase64EncodedCertificationSecretLength = 1023;
  char base_64_secret_property[kBase64EncodedCertificationSecretLength + 1] = {
      0};
  strncpy(base_64_secret_property, kBase64EncodedCertificationSecret,
          kBase64EncodedCertificationSecretLength);

  std::string secret;
  SB_CHECK(Base64Decode(base_64_secret_property, &secret));

  uint8_t* result = HMAC(EVP_sha256(), secret.data(), secret.size(), message,
                         message_size_in_bytes, digest, nullptr);

  return result == digest;
}

} // namespace deviceauth

} // namespace shared

} // namespace starboard

#endif  // SB_API_VERSION >= 11
