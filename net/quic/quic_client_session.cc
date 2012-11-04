// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session.h"

namespace net {

QuicClientSession::QuicClientSession(QuicConnection* connection)
    : QuicSession(connection, false),
      crypto_stream_(this) {
}

QuicClientSession::~QuicClientSession() {
}

QuicReliableClientStream* QuicClientSession::CreateOutgoingReliableStream() {
  if (!crypto_stream_.handshake_complete()) {
    DLOG(INFO) << "Crypto handshake not complete, no outgoing stream created.";
    return NULL;
  }
  if (GetNumOpenStreams() >= get_max_open_streams()) {
    DLOG(INFO) << "Failed to create a new outgoing stream. "
               << "Already " << GetNumOpenStreams() << " open.";
    return NULL;
  }
  QuicReliableClientStream* stream =
       new QuicReliableClientStream(GetNextStreamId(), this);

  ActivateStream(stream);
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return &crypto_stream_;
};

void QuicClientSession::CryptoConnect() {
  CryptoHandshakeMessage message;
  message.tag = kCHLO;
  crypto_stream_.SendHandshakeMessage(message);
}

ReliableQuicStream* QuicClientSession::CreateIncomingReliableStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return NULL;
}

}  // namespace net
