// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A QuicSession, which demuxes a single connection to individual streams.

#ifndef NET_QUIC_QUIC_SESSION_H_
#define NET_QUIC_QUIC_SESSION_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/reliable_quic_stream.h"

namespace net {

class QuicCryptoStream;
class ReliableQuicStream;

class NET_EXPORT_PRIVATE QuicSession : public QuicConnectionVisitorInterface {
 public:
  QuicSession(QuicConnection* connection, bool is_server);

  virtual ~QuicSession();

  // QuicConnectionVisitorInterface methods:
  virtual bool OnPacket(const IPEndPoint& self_address,
                        const IPEndPoint& peer_address,
                        const QuicPacketHeader& header,
                        const std::vector<QuicStreamFrame>& frame) override;
  virtual void OnRstStream(const QuicRstStreamFrame& frame) override;
  virtual void ConnectionClose(QuicErrorCode error, bool from_peer) override;
  // Not needed for HTTP.
  virtual void OnAck(AckedPackets acked_packets) override {}
  virtual bool OnCanWrite() override;

  // Called by streams when they want to write data to the peer.
  virtual int WriteData(QuicStreamId id, base::StringPiece data,
                        QuicStreamOffset offset, bool fin);
  // Called by streams when they want to close the stream in both directions.
  void SendRstStream(QuicStreamId id,
                     QuicErrorCode error,
                     QuicStreamOffset offset);

  // Removes the stream associated with 'stream_id' from the active stream map.
  virtual void CloseStream(QuicStreamId stream_id);

  // Returns true once the crypto handshake is complete.
  virtual bool IsCryptoHandshakeComplete();

  // Called by the QuicCryptoStream when the handshake completes.
  // If |error| is QUIC_NO_ERROR then the handshake was succesful,
  // otherwise it failed.
  virtual void OnCryptoHandshakeComplete(QuicErrorCode error);

  // Returns true if the stream existed previously and has been closed.
  // Returns false if the stream is still active or if the stream has
  // not yet been created.
  bool IsClosedStream(QuicStreamId id);

  QuicConnection* connection() { return connection_.get(); }
  size_t num_active_requests() const { return stream_map_.size(); }
  const IPEndPoint& peer_address() const {
    return connection_->peer_address();
  }

  QuicPacketCreator::Options* options() { return connection()->options(); }

  // Returns the number of currently open streams, including those which have
  // been implicitly created.
  virtual size_t GetNumOpenStreams();

  void MarkWriteBlocked(QuicStreamId id);

 protected:
  // Creates a new stream, owned by the caller, to handle a peer-initiated
  // stream.  Returns NULL and does error handling if the stream can not be
  // created.
  virtual ReliableQuicStream* CreateIncomingReliableStream(QuicStreamId id) = 0;

  // Create a new stream, owned by the caller, to handle a locally-initiated
  // stream.  Returns NULL if max streams have already been opened.
  virtual ReliableQuicStream* CreateOutgoingReliableStream() = 0;

  // Return the reserved crypto stream.
  virtual QuicCryptoStream* GetCryptoStream() = 0;

  // Adds 'stream' to the active stream map.
  void ActivateStream(ReliableQuicStream* stream);

  // Returns the stream id for a new stream.
  QuicStreamId GetNextStreamId();

  ReliableQuicStream* GetIncomingReliableStream(QuicStreamId stream_id);

  size_t get_max_open_streams() const {
    return max_open_streams_;
  }

 private:
  friend class QuicSessionPeer;

  typedef base::hash_map<QuicStreamId, ReliableQuicStream*> ReliableStreamMap;

  ReliableQuicStream* GetStream(const QuicStreamId stream_id);

  scoped_ptr<QuicConnection> connection_;

  // Returns the maximum number of streams this connection can open.
  const size_t max_open_streams_;

  // Map from StreamId to pointers to streams that are owned by the caller.
  ReliableStreamMap stream_map_;
  QuicStreamId next_stream_id_;
  bool is_server_;

  // Set of stream ids that have been "implicitly created" by receipt
  // of a stream id larger than the next expected stream id.
  base::hash_set<QuicStreamId> implicitly_created_streams_;

  // A list of streams which need to write more data.
  std::list<QuicStreamId> write_blocked_streams_;

  QuicStreamId largest_peer_created_stream_id_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_H_
