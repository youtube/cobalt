// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_private_key.h"

#include "base/logging.h"

namespace crypto {

ECPrivateKey::~ECPrivateKey() {}

// static
ECPrivateKey* ECPrivateKey::Create() {
  NOTIMPLEMENTED();
  return NULL;
}

// static
ECPrivateKey* ECPrivateKey::CreateSensitive() {
  NOTIMPLEMENTED();
  return NULL;
}

// static
ECPrivateKey* ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
    const std::string& password,
    const std::vector<uint8>& encrypted_private_key_info,
    const std::vector<uint8>& subject_public_key_info) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
ECPrivateKey* ECPrivateKey::CreateSensitiveFromEncryptedPrivateKeyInfo(
    const std::string& password,
    const std::vector<uint8>& encrypted_private_key_info,
    const std::vector<uint8>& subject_public_key_info) {
  NOTIMPLEMENTED();
  return NULL;
}

bool ECPrivateKey::ExportEncryptedPrivateKey(
    const std::string& password,
    int iterations,
    std::vector<uint8>* output) {
  NOTIMPLEMENTED();
  return false;
}

bool ECPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  NOTIMPLEMENTED();
  return false;
}

bool ECPrivateKey::ExportValue(std::vector<uint8>* output) {
  NOTIMPLEMENTED();
  return false;
}

bool ECPrivateKey::ExportECParams(std::vector<uint8>* output) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace crypto
