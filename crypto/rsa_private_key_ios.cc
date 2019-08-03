// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/rsa_private_key.h"

#include "base/logging.h"

namespace crypto {

// |RSAPrivateKey| is not used on iOS. This implementation was written so that
// it would compile. It may be possible to use the NSS implementation as a real
// implementation, but it hasn't yet been necessary.

// static
RSAPrivateKey* RSAPrivateKey::Create(uint16 num_bits) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitive(uint16 num_bits) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitiveFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
RSAPrivateKey* RSAPrivateKey::FindFromPublicKeyInfo(
    const std::vector<uint8>& input) {
  NOTIMPLEMENTED();
  return NULL;
}

RSAPrivateKey::RSAPrivateKey() : key_(NULL), public_key_(NULL) {}

RSAPrivateKey::~RSAPrivateKey() {
  if (public_key_)
    CFRelease(public_key_);
  if (key_)
    CFRelease(key_);
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) const {
  NOTIMPLEMENTED();
  return false;
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
