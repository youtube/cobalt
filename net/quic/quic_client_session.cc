// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session.h"

#include "base/message_loop.h"
#include "base/stl_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_stream_factory.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

QuicClientSession::QuicClientSession(QuicConnection* connection,
                                     QuicConnectionHelper* helper,
                                     QuicStreamFactory* stream_factory)
    : QuicSession(connection, false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(crypto_stream_(this)),
      helper_(helper),
      stream_factory_(stream_factory),
      read_buffer_(new IOBufferWithSize(kMaxPacketSize)),
      read_pending_(false) {
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

  if (GetNumOpenStreams() == 0) {
    stream_factory_->OnIdleSession(this);
  }
}

void QuicClientSession::OnCryptoHandshakeComplete(QuicErrorCode error) {
  if (!callback_.is_null()) {
    callback_.Run(error == QUIC_NO_ERROR ? OK : ERR_UNEXPECTED);
  }
}

void QuicClientSession::StartReading() {
  if (read_pending_) {
    return;
  }
  read_pending_ = true;
  int rv = helper_->Read(read_buffer_, read_buffer_->size(),
                         base::Bind(&QuicClientSession::OnReadComplete,
                                    weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING) {
    return;
  }

  // Data was read, process it.
  // Schedule the work through the message loop to avoid recursive
  // callbacks.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&QuicClientSession::OnReadComplete,
                 weak_factory_.GetWeakPtr(), rv));
}

void QuicClientSession::OnReadComplete(int result) {
  read_pending_ = false;
  // TODO(rch): Inform the connection about the result.
  if (result > 0) {
    scoped_refptr<IOBufferWithSize> buffer(read_buffer_);
    read_buffer_ = new IOBufferWithSize(kMaxPacketSize);
    QuicEncryptedPacket packet(buffer->data(), result);
    IPEndPoint local_address;
    IPEndPoint peer_address;
    helper_->GetLocalAddress(&local_address);
    helper_->GetPeerAddress(&peer_address);
    // ProcessUdpPacket might result in |this| being deleted, so we
    // use a weak pointer to be safe.
    connection()->ProcessUdpPacket(local_address, peer_address, packet);
    if (!connection()->connected()) {
      stream_factory_->OnSessionClose(this);
      return;
    }
    StartReading();
  }
}

}  // namespace net
