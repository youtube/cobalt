// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The base class for client/server reliable streams.

#ifndef NET_QUIC_RELIABLE_QUIC_STREAM_H_
#define NET_QUIC_RELIABLE_QUIC_STREAM_H_

#include "net/quic/quic_stream_sequencer.h"

namespace net {

class IPEndPoint;
class QuicSession;

// All this does right now is send data to subclasses via the sequencer.
class NET_EXPORT_PRIVATE ReliableQuicStream {
 public:
  ReliableQuicStream(QuicStreamId id,
                     QuicSession* session);

  virtual ~ReliableQuicStream();

  bool WillAcceptStreamFrame(const QuicStreamFrame& frame) const;
  virtual bool OnStreamFrame(const QuicStreamFrame& frame);

  // Called when we get a stream reset from the client.
  // The rst will be passed through the sequencer, which will call
  // TerminateFromPeer when 'offset' bytes have been processed.
  void OnStreamReset(QuicErrorCode error, QuicStreamOffset ofset);

  // Called when we get or send a connection close, and should immediately
  // close the stream.  This is not passed through the sequencer,
  // but is handled immediately.
  virtual void ConnectionClose(QuicErrorCode error, bool from_peer);

  // Called by the sequencer, when we should process a stream termination or
  // stream close from the peer.
  virtual void TerminateFromPeer(bool half_close);

  virtual uint32 ProcessData(const char* data, uint32 data_len) = 0;

  // Called to close the stream from this end.
  virtual void Close(QuicErrorCode error);

  // This block of functions wraps the sequencer's functions of the same
  // name.
  virtual bool IsHalfClosed() const;
  virtual bool HasBytesToRead() const;

  QuicStreamId id() const { return id_; }

  QuicErrorCode error() const { return error_; }

  bool read_side_closed() const { return read_side_closed_; }
  bool write_side_closed() const { return write_side_closed_; }

  const IPEndPoint& GetPeerAddress() const;

 protected:
  virtual int WriteData(base::StringPiece data, bool fin);
  // Close the read side of the socket.  Further frames will not be accepted.
  virtual void CloseReadSide();
  // Close the write side of the socket.  Further writes will fail.
  void CloseWriteSide();

  QuicSession* session() { return session_; }

 private:
  friend class ReliableQuicStreamPeer;

  QuicStreamSequencer sequencer_;
  QuicStreamId id_;
  QuicStreamOffset offset_;
  QuicSession* session_;
  QuicErrorCode error_;
  // True if the read side is closed and further frames should be rejected.
  bool read_side_closed_;
  // True if the write side is closed, and further writes should fail.
  bool write_side_closed_;
};

}  // namespace net

#endif  // NET_QUIC_RELIABLE_QUIC_STREAM_H_
