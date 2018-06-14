// Copyright 2018 Google Inc. All Rights Reserved.

#include <cstring>

#include "starboard/keyboxes/linux/linux.h"
#include "third_party/cdm/oemcrypto/mock/src/oemcrypto_keybox_mock.h"
#include "third_party/cdm/oemcrypto/mock/src/wv_keybox.h"

namespace wvoec_mock {

namespace {

#if defined(COBALT_WIDEVINE_KEYBOX_INCLUDE)
#include COBALT_WIDEVINE_KEYBOX_INCLUDE
#else  // COBALT_WIDEVINE_KEYBOX_INCLUDE
#error "COBALT_WIDEVINE_KEYBOX_INCLUDE is not defined."
#endif  // COBALT_WIDEVINE_KEYBOX_INCLUDE

}  // namespace

bool WvKeybox::Prepare() {
  // Make a local copy of the keybox.
  WidevineKeybox keybox;
  memcpy(&keybox, &kKeybox, sizeof(WidevineKeybox));

  // Unobfuscate the key.
  uint8_t clear[sizeof(kObfuscatedKey)];
  // Despite the non-const type, linux_client does not modify the second
  // argument.
  linux_client(clear, const_cast<uint8_t*>(kObfuscatedKey));

  // Move the clear key into the local copy of the keybox.
  // NOTE: Clear device keys are half the size of obfuscated ones.
  memcpy(&keybox.device_key_, clear, sizeof(keybox.device_key_));
  memset(clear, 0, sizeof(clear));

  // Install the local copy of the keybox.
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&keybox);
  InstallKeybox(bytes, sizeof(keybox));

  // Wipe the local copy of the keybox.
  memset(&keybox, 0, sizeof(keybox));

  return true;
}

}  // namespace wvoec_mock
