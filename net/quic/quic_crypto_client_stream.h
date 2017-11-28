// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
#define NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_

#include "net/quic/quic_crypto_stream.h"

namespace net {

class QuicSession;
struct CryptoHandshakeMessage;

class NET_EXPORT_PRIVATE QuicCryptoClientStream : public QuicCryptoStream {

 public:
  explicit QuicCryptoClientStream(QuicSession* session);

  // CryptoFramerVisitorInterface implementation
  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicCryptoClientStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
