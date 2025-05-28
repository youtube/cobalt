// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/keychain_password_mac.h"
#include "components/os_crypt/sync/os_crypt_metrics.h"
#include "components/os_crypt/sync/os_crypt_switches.h"
#include "crypto/aes_cbc.h"
#include "crypto/apple_keychain.h"
#include "crypto/kdf.h"
#include "crypto/mock_apple_keychain.h"
#include "crypto/subtle_passkey.h"

namespace os_crypt {
class EncryptionKeyCreationUtil;
}

namespace {

// Prefix for cypher text returned by current encryption version.  We prefix
// the cypher text with this string so that future data migration can detect
// this and migrate to different encryption without data loss.
constexpr char kObfuscationPrefixV10[] = "v10";

constexpr std::array<uint8_t, crypto::aes_cbc::kBlockSize> kIv{
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
};

}  // namespace

namespace OSCrypt {
bool EncryptString16(const std::u16string& plaintext, std::string* ciphertext) {
  return OSCryptImpl::GetInstance()->EncryptString16(plaintext, ciphertext);
}
bool DecryptString16(const std::string& ciphertext, std::u16string* plaintext) {
  return OSCryptImpl::GetInstance()->DecryptString16(ciphertext, plaintext);
}
bool EncryptString(const std::string& plaintext, std::string* ciphertext) {
  return OSCryptImpl::GetInstance()->EncryptString(plaintext, ciphertext);
}
bool DecryptString(const std::string& ciphertext, std::string* plaintext) {
  return OSCryptImpl::GetInstance()->DecryptString(ciphertext, plaintext);
}
void UseMockKeychainForTesting(bool use_mock) {
  OSCryptImpl::GetInstance()->UseMockKeychainForTesting(use_mock);
}
void UseLockedMockKeychainForTesting(bool use_locked) {
  OSCryptImpl::GetInstance()->UseLockedMockKeychainForTesting(use_locked);
}
std::string GetRawEncryptionKey() {
  return OSCryptImpl::GetInstance()->GetRawEncryptionKey();
}
void SetRawEncryptionKey(const std::string& key) {
  OSCryptImpl::GetInstance()->SetRawEncryptionKey(key);
}
bool IsEncryptionAvailable() {
  return OSCryptImpl::GetInstance()->IsEncryptionAvailable();
}
}  // namespace OSCrypt

// static
OSCryptImpl* OSCryptImpl::GetInstance() {
  return base::Singleton<OSCryptImpl,
                         base::LeakySingletonTraits<OSCryptImpl>>::get();
}

OSCryptImpl::OSCryptImpl() = default;
OSCryptImpl::~OSCryptImpl() = default;

bool OSCryptImpl::DeriveKey() {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());

  // Fast fail when there's no key and derivation already failed in an earlier
  // call to DeriveKey().
  if (!try_keychain_ && !key_) {
    return false;
  }

  // Fast fail if this object is pretending to have a locked keychain.
  // TODO(https://crbug.com/389737048): Replace this with a setter on the mock
  // keychain once it's possible to inject a mock keychain.
  if (use_mock_keychain_ && use_locked_mock_keychain_) {
    return false;
  }

  // If the key's already present, we are done.
  if (key_) {
    return true;
  }

  const bool mock_keychain_command_line_flag =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          os_crypt::switches::kUseMockKeychain);

  // Do the actual key derivation: for a mock keychain use the keychain's
  // password directly, and for a real keychain, use a randomly-generated
  // password stored inside the keychain.
  // TODO(https://crbug.com/389737048): These code paths should be identical -
  // it is dangerous that the test key derivation code path is different like
  // this. At minimum, the test code path should also use `KeychainPassword`'s
  // logic.
  std::string password;
  if (use_mock_keychain_ || mock_keychain_command_line_flag) {
    crypto::MockAppleKeychain keychain;
    password = keychain.GetEncryptionPassword();
  } else {
    crypto::AppleKeychain keychain;
    KeychainPassword encryptor_password(keychain);
    password = encryptor_password.GetPassword();
  }

  // At this point, whether `encryptor_password.GetPassword()` succeeded or
  // failed, the keychain has been tried. Never try it again.
  try_keychain_ = false;
  if (password.empty()) {
    return false;
  }

  static constexpr auto kSalt =
      std::to_array<uint8_t>({'s', 'a', 'l', 't', 'y', 's', 'a', 'l', 't'});
  static constexpr size_t kIterations = 1003;

  std::array<uint8_t, kDerivedKeySize> key;
  crypto::kdf::DeriveKeyPbkdf2HmacSha1({.iterations = kIterations},
                                       base::as_byte_span(password), kSalt, key,
                                       crypto::SubtlePassKey{});
  key_ = key;
  return true;
}

std::string OSCryptImpl::GetRawEncryptionKey() {
  return DeriveKey() ? std::string(base::as_string_view(*key_)) : std::string();
}

void OSCryptImpl::SetRawEncryptionKey(const std::string& raw_key) {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  CHECK(!key_);

  // Only an input of exactly `kDerivedKeySize` is acceptable for the key.
  auto input = base::as_byte_span(raw_key).to_fixed_extent<kDerivedKeySize>();
  if (input) {
    key_ = std::array<uint8_t, kDerivedKeySize>();
    base::span(*key_).copy_from(*input);
  }

  // This method is used over IPC to configure OSCryptImpl instances in
  // sandboxed helper processes. Those helper processes don't have access to
  // the keychain anyway, and if they try to access it, they'll crash.
  // Therefore, never try the keychain in this mode, even when given an empty
  // key.
  try_keychain_ = false;
}

bool OSCryptImpl::EncryptString16(const std::u16string& plaintext,
                              std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

bool OSCryptImpl::DecryptString16(const std::string& ciphertext,
                              std::u16string* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8)) {
    return false;
  }

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

bool OSCryptImpl::EncryptString(const std::string& plaintext,
                            std::string* ciphertext) {
  if (plaintext.empty()) {
    *ciphertext = std::string();
    return true;
  }

  if (!DeriveKey()) {
    VLOG(1) << "Key derivation failed";
    return false;
  }

  // Prefix the cypher text with version information.
  *ciphertext = kObfuscationPrefixV10;
  ciphertext->append(base::as_string_view(
      crypto::aes_cbc::Encrypt(*key_, kIv, base::as_byte_span(plaintext))));

  return true;
}

bool OSCryptImpl::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  if (ciphertext.empty()) {
    *plaintext = std::string();
    return true;
  }

  // Check that the incoming cyphertext was indeed encrypted with the expected
  // version.  If the prefix is not found then we'll assume we're dealing with
  // old data saved as clear text and we'll return it directly.
  // Credit card numbers are current legacy data, so false match with prefix
  // won't happen.
  const os_crypt::EncryptionPrefixVersion encryption_version =
      ciphertext.find(kObfuscationPrefixV10) == 0
          ? os_crypt::EncryptionPrefixVersion::kVersion10
          : os_crypt::EncryptionPrefixVersion::kNoVersion;

  os_crypt::LogEncryptionVersion(encryption_version);

  if (encryption_version == os_crypt::EncryptionPrefixVersion::kNoVersion) {
    return false;
  }

  if (!DeriveKey()) {
    VLOG(1) << "Key derivation failed";
    return false;
  }

  // Strip off the versioning prefix before decrypting.
  base::span<const uint8_t> raw_ciphertext =
      base::as_byte_span(ciphertext).subspan(strlen(kObfuscationPrefixV10));

  std::optional<std::vector<uint8_t>> maybe_plain =
      crypto::aes_cbc::Decrypt(*key_, kIv, base::as_byte_span(raw_ciphertext));

  if (!maybe_plain) {
    VLOG(1) << "Decryption failed";
    return false;
  }

  plaintext->assign(base::as_string_view(*maybe_plain));
  return true;
}

bool OSCryptImpl::IsEncryptionAvailable() {
  return DeriveKey();
}

void OSCryptImpl::UseMockKeychainForTesting(bool use_mock) {
  use_mock_keychain_ = use_mock;
  if (!use_mock_keychain_) {
    use_locked_mock_keychain_ = false;
  }
}

void OSCryptImpl::UseLockedMockKeychainForTesting(bool use_locked) {
  use_locked_mock_keychain_ = use_locked;
  if (use_locked_mock_keychain_) {
    use_mock_keychain_ = true;
  }
}

// static
base::Lock& OSCryptImpl::GetLock() {
  static base::NoDestructor<base::Lock> os_crypt_lock;
  return *os_crypt_lock;
}
