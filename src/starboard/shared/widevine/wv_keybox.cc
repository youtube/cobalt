// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/ce_cdm/oemcrypto/mock/src/oemcrypto_keybox_mock.h"

#include <openssl/bn.h>
#include <vector>

extern "C" {
#include COBALT_WIDEVINE_KEYBOX_TRANSFORM_INCLUDE
};

#include "third_party/ce_cdm/oemcrypto/mock/src/wv_keybox.h"

namespace wvoec_mock {

namespace {
#if defined(COBALT_WIDEVINE_KEYBOX_INCLUDE)
#include COBALT_WIDEVINE_KEYBOX_INCLUDE
#else  // COBALT_WIDEVINE_KEYBOX_INCLUDE
#error "COBALT_WIDEVINE_KEYBOX_INCLUDE is not defined."
#endif  // COBALT_WIDEVINE_KEYBOX_INCLUDE
}  // namespace

// |kKeybox| provided in obfiscated form: the key field need to be
// de-obfiscated before installing to CryptoEngine, rest fields
// left as-is and should not be de-obfuscated.
bool WvKeybox::Prepare() {
  // Create a temporary kKeybox's copy with ofuscated key.
  WidevineKeybox keybox(kKeybox);

  // Replace obfuscated with de-obfiscated and install.
  uint8_t clear_key[sizeof(kObfuscatedKey)];
  COBALT_WIDEVINE_KEYBOX_TRANSFORM_FUNCTION(
      clear_key, const_cast<unsigned char*>(kObfuscatedKey));
  SbMemoryCopy(&keybox.device_key_, clear_key, sizeof(keybox.device_key_));

  // Erase |de_obfuscated| because it contains clear key
  SbMemorySet(&clear_key, 0, sizeof(clear_key));
  bool installed = InstallKeybox(reinterpret_cast<const uint8_t*>(&keybox),
                                 sizeof(WidevineKeybox));
  // Erase temporary kKeybox's copy because it is de-obfuscated
  SbMemorySet(&keybox, 0, sizeof(WidevineKeybox));
  return installed;
}

}  // namespace wvoec_mock
