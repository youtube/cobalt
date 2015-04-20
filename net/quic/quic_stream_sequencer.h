// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_STREAM_SEQUENCER_H_
#define NET_QUIC_QUIC_STREAM_SEQUENCER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_protocol.h"

using std::map;
using std::string;

namespace net {

class QuicSession;
class ReliableQuicStream;

// Buffers frames until we have something which can be passed
// up to the next layer.
// TOOD(alyssar) add some checks for overflow attempts [1, 256,] [2, 256]
class NET_EXPORT_PRIVATE QuicStreamSequencer {
 public:
  static size_t kMaxUdpPacketSize;

  explicit QuicStreamSequencer(ReliableQuicStream* quic_stream);
  QuicStreamSequencer(size_t max_frame_memory,
                      ReliableQuicStream* quic_stream);

  virtual ~QuicStreamSequencer();

  // Returns the expected value of OnStreamFrame for this frame.
  bool WillAcceptStreamFrame(const QuicStreamFrame& frame) const;

  // If the frame is the next one we need in order to process in-order data,
  // ProcessData will be immediately called on the stream until all buffered
  // data is processed or the stream fails to consume data.  Any unconsumed
  // data will be buffered.
  //
  // If the frame is not the next in line, it will either be buffered, and
  // this will return true, or it will be rejected and this will return false.
  bool OnStreamFrame(const QuicStreamFrame& frame);

  // Wait until we've seen 'offset' bytes, and then terminate the stream.
  void CloseStreamAtOffset(QuicStreamOffset offset, bool half_close);

  // Once data is buffered, it's up to the stream to read it when the stream
  // can handle more data.  The following three functions make that possible.

  // Returns true if the sequncer has bytes available for reading.
  bool HasBytesToRead() const;

  // Returns true if the sequencer has delivered a half close.
  bool IsHalfClosed() const;

  // Returns true if the sequencer has delivered a full close.
  bool IsClosed() const;

 private:
  friend class QuicStreamSequencerPeer;

  // TODO(alyssar) use something better than strings.
  typedef map<QuicStreamOffset, string> FrameMap;

  void FlushBufferedFrames();

  bool MaybeCloseStream();

  ReliableQuicStream* stream_;  // The stream which owns this sequencer.
  QuicStreamOffset num_bytes_consumed_;  // The last data consumed by the stream
  FrameMap frames_;  // sequence number -> frame
  size_t max_frame_memory_;  //  the maximum memory the sequencer can buffer.
  // The offset, if any, we got a stream cancelation for.  When this many bytes
  // have been processed, the stream will be half or full closed depending on
  // the half_close_ bool.
  QuicStreamOffset close_offset_;
  // Only valid if close_offset_ is set.  Indicates if it's a half or a full
  // close.
  bool half_close_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_STREAM_SEQUENCER_H_
