// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_QUIC_DECRYPTER_H_
#define NET_QUIC_CRYPTO_QUIC_DECRYPTER_H_

#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE QuicDecrypter {
 public:
  virtual ~QuicDecrypter() {}

  static QuicDecrypter* Create(CryptoTag algorithm);

  // Returns a newly created QuicData object containing the decrypted
  // |ciphertext| or NULL if there is an error.
  virtual QuicData* Decrypt(base::StringPiece associated_data,
                            base::StringPiece ciphertext) = 0;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_QUIC_DECRYPTER_H_
