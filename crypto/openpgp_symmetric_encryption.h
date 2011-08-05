// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_OPENPGP_SYMMETRIC_ENCRYPTION_H_
#define CRYPTO_OPENPGP_SYMMETRIC_ENCRYPTION_H_
#pragma once

#include <string>

#include "base/string_piece.h"
#include "crypto/crypto_export.h"

namespace crypto {

// OpenPGPSymmetricEncrytion implements enough of RFC 4880 to read and write
// uncompressed, symmetrically encrypted data. You can create ciphertext
// compatable with this code from the command line with:
//    gpg --compress-algo=NONE --cipher-algo=AES -c
//
// Likewise, the output of this can be decrypted on the command line with:
//    gpg < input
class CRYPTO_EXPORT OpenPGPSymmetricEncrytion {
 public:
  enum Result {
    OK,
    UNKNOWN_CIPHER,  // you forgot to pass --cipher-algo=AES to gpg
    UNKNOWN_HASH,
    NOT_SYMMETRICALLY_ENCRYPTED,  // it's OpenPGP data, but not correct form
    COMPRESSED,  // you forgot to pass --compress-algo=NONE
    PARSE_ERROR,  // it's not OpenPGP data.
    INTERNAL_ERROR,
  };

  static Result Decrypt(base::StringPiece encrypted,
                        base::StringPiece passphrase,
                        std::string *out);

  static std::string Encrypt(base::StringPiece plaintext,
                             base::StringPiece passphrase);
};

}  // namespace crypto

#endif  // CRYPTO_OPENPGP_SYMMETRIC_ENCRYPTION_H_

