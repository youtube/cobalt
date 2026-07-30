// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/session_keys.h"

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "crypto/kdf.h"

namespace ash::secure_channel {

namespace {

// The salt used to compute the devired keys. SHA256 of "D2D".
constexpr auto kSalt = std::to_array<uint8_t>(
    {0x82, 0xAA, 0x55, 0xA0, 0xD3, 0x97, 0xF8, 0x83, 0x46, 0xCA, 0x1C,
     0xEE, 0x8D, 0x39, 0x09, 0xB9, 0x5F, 0x13, 0xFA, 0x7D, 0xEB, 0x1D,
     0x4A, 0xB3, 0x83, 0x76, 0xB8, 0x25, 0x6D, 0xA8, 0x55, 0x10});

// The number of bytes in the derived keys.
constexpr size_t kKeySize = 32;

// String used in the derivation of the inititor encoding key.
const std::string_view kInitiatorPurpose = "initiator";

// String used in the derivation of the responder encoding key.
const std::string_view kResponderPurpose = "responder";

}  // namespace

SessionKeys::SessionKeys(const std::string& session_symmetric_key) {
  const auto root_key = base::as_byte_span(session_symmetric_key);
  initiator_encode_key_ = base::as_string_view(
      crypto::kdf::Hkdf<kKeySize>(crypto::hash::kSha256, root_key, kSalt,
                                  base::as_byte_span(kInitiatorPurpose)));
  responder_encode_key_ = base::as_string_view(
      crypto::kdf::Hkdf<kKeySize>(crypto::hash::kSha256, root_key, kSalt,
                                  base::as_byte_span(kResponderPurpose)));
}

SessionKeys::SessionKeys() {}

SessionKeys::~SessionKeys() {}

std::string SessionKeys::initiator_encode_key() const {
  return initiator_encode_key_;
}

std::string SessionKeys::responder_encode_key() const {
  return responder_encode_key_;
}

}  // namespace ash::secure_channel
