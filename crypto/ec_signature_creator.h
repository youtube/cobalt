// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_EC_SIGNATURE_CREATOR_H_
#define CRYPTO_EC_SIGNATURE_CREATOR_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "crypto/crypto_export.h"

namespace crypto {

class ECPrivateKey;

// Signs data using a bare private key (as opposed to a full certificate).
// We need this class because SignatureCreator is hardcoded to use
// RSAPrivateKey.
class CRYPTO_EXPORT ECSignatureCreator {
 public:
  ~ECSignatureCreator();

  // Create an instance. The caller must ensure that the provided PrivateKey
  // instance outlives the created ECSignatureCreator.
  // TODO(rch):  This is currently hard coded to use SHA1. Ideally, we should
  // pass in the hash algorithm identifier.
  static ECSignatureCreator* Create(ECPrivateKey* key);

  // Signs |data_len| bytes from |data| and writes the results into
  // |signature| as a DER encoded ECDSA-Sig-Value from RFC 3279.
  //
  //  ECDSA-Sig-Value ::= SEQUENCE {
  //    r     INTEGER,
  //    s     INTEGER }
  bool Sign(const uint8* data,
            int data_len,
            std::vector<uint8>* signature);

 private:
  // Private constructor. Use the Create() method instead.
  explicit ECSignatureCreator(ECPrivateKey* key);

  ECPrivateKey* key_;

  DISALLOW_COPY_AND_ASSIGN(ECSignatureCreator);
};

}  // namespace crypto

#endif  // CRYPTO_EC_SIGNATURE_CREATOR_H_
