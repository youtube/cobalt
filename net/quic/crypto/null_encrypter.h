// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_NULL_ENCRYPTER_H_
#define NET_QUIC_CRYPTO_NULL_ENCRYPTER_H_

#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/quic_encrypter.h"

namespace net {

// A NullEncrypter is a QuicEncrypter used before a crypto negotiation
// has occurred.  It does not actually encrypt the payload, but does
// generate a MAC (fnv128) over both the payload and associated data.
class NET_EXPORT_PRIVATE NullEncrypter : public QuicEncrypter {
 public:
  virtual ~NullEncrypter() {}

  // QuicEncrypter implementation
  virtual QuicData* Encrypt(base::StringPiece associated_data,
                            base::StringPiece plaintext) override;
  virtual size_t GetMaxPlaintextSize(size_t ciphertext_size) override;
  virtual size_t GetCiphertextSize(size_t plaintext_size) override;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_NULL_ENCRYPTER_H_
