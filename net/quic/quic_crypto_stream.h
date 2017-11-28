// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_STREAM_H_
#define NET_QUIC_QUIC_CRYPTO_STREAM_H_

#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/reliable_quic_stream.h"

namespace net {

class QuicSession;
struct CryptoHandshakeMessage;

// Crypto handshake messages in QUIC take place over a reserved
// reliable stream with the id 1.  Each endpoint (client and server)
// will allocate an instance of a subclass of QuicCryptoStream
// to send and receive handshake messages.  (In the normal 1-RTT
// handshake, the client will send a client hello, CHLO, message.
// The server will receive this message and respond with a server
// hello message, SHLO.  At this point both sides will have established
// a crypto context they can use to send encrypted messages.
//
// For more details: http://goto.google.com/quic-crypto
class NET_EXPORT_PRIVATE QuicCryptoStream
    : public ReliableQuicStream,
      public CryptoFramerVisitorInterface {

 public:
  explicit QuicCryptoStream(QuicSession* session);

  // CryptoFramerVisitorInterface implementation
  virtual void OnError(CryptoFramer* framer) override;
  virtual void OnHandshakeMessage(const CryptoHandshakeMessage& message) = 0;

  // ReliableQuicStream implementation
  virtual uint32 ProcessData(const char* data, uint32 data_len) override;

  // Sends |message| to the peer.
  void SendHandshakeMessage(const CryptoHandshakeMessage& message);

  bool handshake_complete() { return handshake_complete_; }

 protected:
  // Closes the connection
  void CloseConnection(QuicErrorCode error);

  void SetHandshakeComplete(QuicErrorCode error);

 private:
  CryptoFramer crypto_framer_;
  bool handshake_complete_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_STREAM_H_
