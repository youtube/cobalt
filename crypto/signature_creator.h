// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SIGNATURE_CREATOR_H_
#define CRYPTO_SIGNATURE_CREATOR_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"

// Forward declaration for openssl/*.h
typedef struct env_md_ctx_st EVP_MD_CTX;

namespace crypto {

class RSAPrivateKey;

// Signs data using a bare private key (as opposed to a full certificate).
// Currently can only sign data using SHA-1 or SHA-256 with RSA PKCS#1v1.5.
class CRYPTO_EXPORT SignatureCreator {
 public:
  // The set of supported hash functions. Extend as required.
  enum HashAlgorithm {
    SHA1,
    SHA256,
  };

  ~SignatureCreator();

  // Create an instance. The caller must ensure that the provided PrivateKey
  // instance outlives the created SignatureCreator. Uses the HashAlgorithm
  // specified.
  static std::unique_ptr<SignatureCreator> Create(RSAPrivateKey* key,
                                                  HashAlgorithm hash_alg);

  // Signs the precomputed |hash_alg| digest |data| using private |key| as
  // specified in PKCS #1 v1.5.
  static bool Sign(RSAPrivateKey* key,
                   HashAlgorithm hash_alg,
                   const uint8_t* data,
                   int data_len,
                   std::vector<uint8_t>* signature);

  // Update the signature with more data.
  bool Update(const uint8_t* data_part, int data_part_len);

  // Finalize the signature.
  bool Final(std::vector<uint8_t>* signature);

 private:
  // Private constructor. Use the Create() method instead.
  SignatureCreator();

  EVP_MD_CTX* sign_context_;

  DISALLOW_COPY_AND_ASSIGN(SignatureCreator);
};

}  // namespace crypto

#endif  // CRYPTO_SIGNATURE_CREATOR_H_
