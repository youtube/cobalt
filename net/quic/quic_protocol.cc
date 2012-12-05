// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_protocol.h"

using base::StringPiece;
using std::ostream;

namespace net {

QuicStreamFrame::QuicStreamFrame() {}

QuicStreamFrame::QuicStreamFrame(QuicStreamId stream_id,
                                 bool fin,
                                 uint64 offset,
                                 StringPiece data)
    : stream_id(stream_id),
      fin(fin),
      offset(offset),
      data(data) {
}

ReceivedPacketInfo::ReceivedPacketInfo() {}

ReceivedPacketInfo::~ReceivedPacketInfo() {}

SentPacketInfo::SentPacketInfo() {}

SentPacketInfo::~SentPacketInfo() {}

ostream& operator<<(ostream& os, const QuicAckFrame& s) {
  os << "largest_received: " << s.received_info.largest_received
     << " time: " << s.received_info.time_received.ToMicroseconds()
     << " missing: ";
  for (SequenceSet::const_iterator it = s.received_info.missing_packets.begin();
       it != s.received_info.missing_packets.end(); ++it) {
    os << *it << " ";
  }

  os << " least_waiting: " << s.sent_info.least_unacked
     << " no_retransmit: ";
  for (SequenceSet::const_iterator it = s.sent_info.non_retransmiting.begin();
       it != s.sent_info.non_retransmiting.end(); ++it) {
    os << *it << " ";
  }
  os << "\n";
  return os;
}

QuicFecData::QuicFecData() {}

bool QuicFecData::operator==(const QuicFecData& other) const {
  if (fec_group != other.fec_group) {
    return false;
  }
  if (min_protected_packet_sequence_number !=
      other.min_protected_packet_sequence_number) {
    return false;
  }
  if (redundancy != other.redundancy) {
    return false;
  }
  return true;
}

QuicData::~QuicData() {
  if (owns_buffer_) {
    delete [] const_cast<char*>(buffer_);
  }
}

}  // namespace net
