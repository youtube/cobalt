// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session.h"

#include "net/base/net_errors.h"

namespace net {

QuicClientSession::QuicClientSession(QuicConnection* connection)
    : QuicSession(connection, false),
      crypto_stream_(this) {
}

QuicClientSession::~QuicClientSession() {
  STLDeleteValues(&streams_);
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
  streams_[stream->id()] = stream;

  ActivateStream(stream);
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return &crypto_stream_;
};

int QuicClientSession::CryptoConnect(const CompletionCallback& callback) {
  CryptoHandshakeMessage message;
  message.tag = kCHLO;
  crypto_stream_.SendHandshakeMessage(message);

  if (IsCryptoHandshakeComplete()) {
    return OK;
  }

  callback_ = callback;
  return ERR_IO_PENDING;
}

ReliableQuicStream* QuicClientSession::CreateIncomingReliableStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return NULL;
}

void QuicClientSession::CloseStream(QuicStreamId stream_id) {
  QuicSession::CloseStream(stream_id);

  StreamMap::iterator it = streams_.find(stream_id);
  DCHECK(it != streams_.end());
  if (it != streams_.end()) {
    ReliableQuicStream* stream = it->second;
    streams_.erase(it);
    delete stream;
  }
}

void QuicClientSession::OnCryptoHandshakeComplete(QuicErrorCode error) {
  if (!callback_.is_null()) {
    callback_.Run(error == QUIC_NO_ERROR ? OK : ERR_UNEXPECTED);
  }
}

}  // namespace net
