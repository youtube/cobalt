// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_client_stream.h"

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_protocol.h"

namespace net {

QuicCryptoClientStream::QuicCryptoClientStream(QuicSession* session)
    : QuicCryptoStream(session) {
}


void QuicCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  // Do not process handshake messages after the handshake is complete.
  if (handshake_complete()) {
    CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
    return;
  }

  if (message.tag != kSHLO) {
    CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
    return;
  }

  // TODO(rch): correctly validate the message
  SetHandshakeComplete(QUIC_NO_ERROR);
  return;
}

}  // namespace net
