// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_session.h"

using base::StringPiece;

namespace net {

QuicCryptoStream::QuicCryptoStream(QuicSession* session)
    : ReliableQuicStream(kCryptoStreamId, session),
      handshake_complete_(false) {
  crypto_framer_.set_visitor(this);
}

void QuicCryptoStream::OnError(CryptoFramer* framer) {
  session()->ConnectionClose(framer->error(), false);
}

uint32 QuicCryptoStream::ProcessData(const char* data,
                                     uint32 data_len) {
  // Do not process handshake messages after the handshake is complete.
  if (handshake_complete()) {
    CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
    return 0;
  }
  if (!crypto_framer_.ProcessInput(StringPiece(data, data_len))) {
    CloseConnection(crypto_framer_.error());
    return 0;
  }
  return data_len;
}

void QuicCryptoStream::CloseConnection(QuicErrorCode error) {
  session()->connection()->SendConnectionClose(error);
}

void QuicCryptoStream::SetHandshakeComplete(QuicErrorCode error) {
  handshake_complete_ = true;
  session()->OnCryptoHandshakeComplete(error);
}

void QuicCryptoStream::SendHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  scoped_ptr<QuicData> data(crypto_framer_.ConstructHandshakeMessage(message));
  WriteData(string(data->data(), data->length()), false);
}

}  // namespace net
