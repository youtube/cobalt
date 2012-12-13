// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session.h"

#include "net/quic/quic_connection.h"

using base::StringPiece;
using base::hash_map;
using base::hash_set;
using std::vector;

namespace net {

QuicSession::QuicSession(QuicConnection* connection, bool is_server)
    : connection_(connection),
      max_open_streams_(kDefaultMaxStreamsPerConnection),
      next_stream_id_(is_server ? 2 : 3),
      is_server_(is_server),
      largest_peer_created_stream_id_(0) {
  connection_->set_visitor(this);
}

QuicSession::~QuicSession() {
}

bool QuicSession::OnPacket(const IPEndPoint& self_address,
                           const IPEndPoint& peer_address,
                           const QuicPacketHeader& header,
                           const vector<QuicStreamFrame>& frames) {
  if (header.guid != connection()->guid()) {
    DLOG(INFO) << "Got packet header for invalid GUID: " << header.guid;
    return false;
  }
  for (size_t i = 0; i < frames.size(); ++i) {
    // TODO(rch) deal with the error case of stream id 0
    if (IsClosedStream(frames[i].stream_id)) continue;

    ReliableQuicStream* stream = GetStream(frames[i].stream_id);
    if (stream == NULL) return false;
    if (!stream->WillAcceptStreamFrame(frames[i])) return false;

    // TODO(alyssar) check against existing connection address: if changed, make
    // sure we update the connection.
  }

  for (size_t i = 0; i < frames.size(); ++i) {
    ReliableQuicStream* stream = GetStream(frames[i].stream_id);
    if (stream) {
      stream->OnStreamFrame(frames[i]);
    }
  }
  return true;
}

void QuicSession::OnRstStream(const QuicRstStreamFrame& frame) {
  ReliableQuicStream* stream = GetStream(frame.stream_id);
  if (!stream) {
    return;  // Errors are handled by GetStream.
  }
  stream->OnStreamReset(frame.error_code, frame.offset);
}

void QuicSession::ConnectionClose(QuicErrorCode error, bool from_peer) {
  while (stream_map_.size() != 0) {
    ReliableStreamMap::iterator it = stream_map_.begin();
    QuicStreamId id = it->first;
    it->second->ConnectionClose(error, from_peer);
    // The stream should call CloseStream as part of ConnectionClose.
    if (stream_map_.find(id) != stream_map_.end()) {
      LOG(DFATAL) << "Stream failed to close under ConnectionClose";
      CloseStream(id);
    }
  }
}

bool QuicSession::OnCanWrite() {
  // We latch this here rather than doing a traditional loop, because streams
  // may be modifying the list as we loop.
  int remaining_writes = write_blocked_streams_.size();

  while (connection_->NumQueuedPackets() == 0 &&
         remaining_writes > 0) {
    DCHECK(!write_blocked_streams_.empty());
    ReliableQuicStream* stream = GetStream(write_blocked_streams_.front());
    write_blocked_streams_.pop_front();
    if (stream != NULL) {
      // If the stream can't write all bytes, it'll re-add itself to the blocked
      // list.
      stream->OnCanWrite();
    }
    --remaining_writes;
  }

  return write_blocked_streams_.empty();
}

int QuicSession::WriteData(QuicStreamId id, StringPiece data,
                           QuicStreamOffset offset, bool fin) {
  return connection_->SendStreamData(id, data, offset, fin, NULL);
}

void QuicSession::SendRstStream(QuicStreamId id,
                                QuicErrorCode error,
                                QuicStreamOffset offset) {
  connection_->SendRstStream(id, error, offset);
  CloseStream(id);
}

void QuicSession::CloseStream(QuicStreamId stream_id) {
  DLOG(INFO) << "Closing stream " << stream_id;

  ReliableStreamMap::iterator it = stream_map_.find(stream_id);
  if (it == stream_map_.end()) {
    DLOG(INFO) << "Stream is already closed: " << stream_id;
    return;
  }
  stream_map_.erase(it);
}

bool QuicSession::IsCryptoHandshakeComplete() {
  return GetCryptoStream()->handshake_complete();
}

void QuicSession::OnCryptoHandshakeComplete(QuicErrorCode error) {
  // TODO(rch): tear down the connection if error != QUIC_NO_ERROR.
}

void QuicSession::ActivateStream(ReliableQuicStream* stream) {
  LOG(INFO) << "num_streams: " << stream_map_.size()
            << ". activating " << stream->id();
  DCHECK(stream_map_.count(stream->id()) == 0);
  stream_map_[stream->id()] = stream;
}

QuicStreamId QuicSession::GetNextStreamId() {
  QuicStreamId id = next_stream_id_;
  next_stream_id_ += 2;
  return id;
}

ReliableQuicStream* QuicSession::GetStream(const QuicStreamId stream_id) {
  if (stream_id == kCryptoStreamId) {
    return GetCryptoStream();
  }

  ReliableStreamMap::iterator it = stream_map_.find(stream_id);
  if (it != stream_map_.end()) {
    return it->second;
  }

  if (stream_id % 2 == next_stream_id_ % 2) {
    // We've received a frame for a locally-created stream that is not
    // currently active.  This is an error.
    connection()->SendConnectionClose(QUIC_PACKET_FOR_NONEXISTENT_STREAM);
    return NULL;
  }

  return GetIncomingReliableStream(stream_id);
}

ReliableQuicStream* QuicSession::GetIncomingReliableStream(
    QuicStreamId stream_id) {
  if (IsClosedStream(stream_id)) {
    return NULL;
  }

  implicitly_created_streams_.erase(stream_id);
  if (stream_id > largest_peer_created_stream_id_) {
    // TODO(rch) add unit test for this
    if (stream_id - largest_peer_created_stream_id_ > kMaxStreamIdDelta) {
      connection()->SendConnectionClose(QUIC_INVALID_STREAM_ID);
      return NULL;
    }
    if (largest_peer_created_stream_id_ != 0) {
      for (QuicStreamId id = largest_peer_created_stream_id_ + 2;
           id < stream_id;
           id += 2) {
        implicitly_created_streams_.insert(id);
      }
    }
    largest_peer_created_stream_id_ = stream_id;
  }
  ReliableQuicStream* stream = CreateIncomingReliableStream(stream_id);
  if (stream == NULL) {
    return NULL;
  }
  ActivateStream(stream);
  return stream;
}

bool QuicSession::IsClosedStream(QuicStreamId id) {
  DCHECK_NE(0u, id);
  if (id == kCryptoStreamId) {
    return false;
  }
  if (stream_map_.count(id) != 0) {
    // Stream is active
    return false;
  }
  if (id % 2 == next_stream_id_ % 2) {
    // Locally created streams are strictly in-order.  If the id is in the
    // range of created streams and it's not active, it must have been closed.
    return id < next_stream_id_;
  }
  // For peer created streams, we also need to consider implicitly created
  // streams.
  return id <= largest_peer_created_stream_id_ &&
      implicitly_created_streams_.count(id) == 0;
}

size_t QuicSession::GetNumOpenStreams() {
  return stream_map_.size() + implicitly_created_streams_.size();
}

void QuicSession::MarkWriteBlocked(QuicStreamId id) {
  write_blocked_streams_.push_back(id);
}

}  // namespace net
