// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A client specific QuicSession subclass.  This class owns the underlying
// QuicConnection and QuicConnectionHelper objects.  The connection stores
// a non-owning pointer to the helper so this session needs to ensure that
// the helper outlives the connection.

#ifndef NET_QUIC_QUIC_CLIENT_SESSION_H_
#define NET_QUIC_QUIC_CLIENT_SESSION_H_

#include "base/hash_tables.h"
#include "net/base/completion_callback.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_reliable_client_stream.h"
#include "net/quic/quic_session.h"

namespace net {

class QuicConnectionHelper;
class QuicStreamFactory;

class NET_EXPORT_PRIVATE QuicClientSession : public QuicSession {
 public:
  // Constructs a new session which will own |connection| and |helper|, but
  // not |stream_factory|, which must outlive this session.
  // TODO(rch): decouple the factory from the session via a Delegate interface.
  QuicClientSession(QuicConnection* connection,
                    QuicConnectionHelper* helper,
                    QuicStreamFactory* stream_factory);

  virtual ~QuicClientSession();

  // QuicSession methods:
  virtual QuicReliableClientStream* CreateOutgoingReliableStream() override;
  virtual QuicCryptoClientStream* GetCryptoStream() override;
  virtual void CloseStream(QuicStreamId stream_id) override;
  virtual void OnCryptoHandshakeComplete(QuicErrorCode error) override;

  // Perform a crypto handshake with the server.
  int CryptoConnect(const CompletionCallback& callback);

  // Causes the QuicConnectionHelper to start reading from the socket
  // and passing the data along to the QuicConnection.
  void StartReading();

 protected:
  // QuicSession methods:
  virtual ReliableQuicStream* CreateIncomingReliableStream(
      QuicStreamId id) override;

 private:
  typedef base::hash_map<QuicStreamId, ReliableQuicStream*> StreamMap;

  // A completion callback invoked when a read completes.
  void OnReadComplete(int result);

  base::WeakPtrFactory<QuicClientSession> weak_factory_;
  QuicCryptoClientStream crypto_stream_;
  scoped_ptr<QuicConnectionHelper> helper_;
  StreamMap streams_;
  QuicStreamFactory* stream_factory_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  bool read_pending_;
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientSession);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CLIENT_SESSION_H_
