// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_protocol.h"
#include "base/stl_util.h"

using base::StringPiece;
using std::map;
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

// TODO(ianswett): Initializing largest_received to 0 should not be necessary.
ReceivedPacketInfo::ReceivedPacketInfo() : largest_received(0) {}

ReceivedPacketInfo::~ReceivedPacketInfo() {}

void ReceivedPacketInfo::RecordAck(QuicPacketSequenceNumber sequence_number,
                                   QuicTime time) {
  DCHECK(!ContainsAck(sequence_number));
  received_packet_times[sequence_number] = time;
  if (largest_received < sequence_number) {
    largest_received = sequence_number;
  }
}

bool ReceivedPacketInfo::ContainsAck(
    QuicPacketSequenceNumber sequence_number) const {
  return ContainsKey(received_packet_times, sequence_number);
}

void ReceivedPacketInfo::ClearAcksBefore(
    QuicPacketSequenceNumber least_unacked) {
  for (QuicPacketSequenceNumber i = received_packet_times.begin()->first;
       i < least_unacked; ++i) {
    received_packet_times.erase(i);
  }
}

SentPacketInfo::SentPacketInfo() {}

SentPacketInfo::~SentPacketInfo() {}

QuicAckFrame::QuicAckFrame(QuicPacketSequenceNumber largest_received,
                           QuicTime time_received,
                           QuicPacketSequenceNumber least_unacked) {
  received_info.largest_received = largest_received;
  for (QuicPacketSequenceNumber seq_num = 1;
       seq_num <= largest_received; ++seq_num) {
    received_info.RecordAck(seq_num, time_received);
  }
  sent_info.least_unacked = least_unacked;
  congestion_info.type = kNone;
}

ostream& operator<<(ostream& os, const QuicAckFrame& s) {
  os << "sent info { least_waiting: " << s.sent_info.least_unacked << " } "
     << "received info { largest_received: "
     << s.received_info.largest_received
     << " received packets: [ ";
  for (map<QuicPacketSequenceNumber, QuicTime>::const_iterator it =
           s.received_info.received_packet_times.begin();
       it != s.received_info.received_packet_times.end(); ++it) {
    os << it->first << "@" << it->second.ToMilliseconds() << " ";
  }

  os << "] } congestion info { type: " << s.congestion_info.type;
  switch (s.congestion_info.type) {
    case kNone:
      break;
    case kInterArrival: {
      const CongestionFeedbackMessageInterArrival& inter_arrival =
          s.congestion_info.inter_arrival;
      os << " accumulated_number_of_lost_packets: "
         << inter_arrival.accumulated_number_of_lost_packets;
      os << " offset_time: " << inter_arrival.offset_time;
      os << " delta_time: " << inter_arrival.delta_time;
      break;
    }
    case kFixRate: {
      os << " bitrate_in_bytes_per_second: "
         << s.congestion_info.fix_rate.bitrate_in_bytes_per_second;
      break;
    }
    case kTCP: {
      const CongestionFeedbackMessageTCP& tcp = s.congestion_info.tcp;
      os << " accumulated_number_of_lost_packets: "
         << tcp.accumulated_number_of_lost_packets;
      os << " receive_window: " << tcp.receive_window;
      break;
    }
  }
  os << " }\n";
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
