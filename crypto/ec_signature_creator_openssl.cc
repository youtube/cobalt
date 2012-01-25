// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_signature_creator.h"

#include "base/logging.h"

namespace crypto {

// static
ECSignatureCreator* ECSignatureCreator::Create(ECPrivateKey* key) {
  NOTIMPLEMENTED();
  return NULL;
}

ECSignatureCreator::ECSignatureCreator(ECPrivateKey* key,
                                       HASH_HashType hash_type)
    : key_(key),
      hash_type_(hash_type) {
  NOTIMPLEMENTED();
}

ECSignatureCreator::~ECSignatureCreator() { }

bool ECSignatureCreator::Sign(const uint8* data,
                              int data_len,
                              std::vector<uint8>* signature) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace crypto
