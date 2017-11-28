// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic packet creation.

#ifndef NET_QUIC_QUIC_PACKET_CREATOR_H_
#define NET_QUIC_QUIC_PACKET_CREATOR_H_

#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE QuicPacketCreator : public QuicFecBuilderInterface {
 public:
  // Options for controlling how packets are created.
  struct Options {
    Options() {
      Clear();
    }
    void Clear() {
      memset(this, 0, sizeof(Options));
      max_packet_length = kMaxPacketSize;
      max_num_packets = std::numeric_limits<size_t>::max();
    }

    // TODO(alyssar, rch) max frames/packet
    size_t max_packet_length;
    // The maximum number of packets we'd like to serialize.
    // If the data can't be fully serialized, DataToStream will only consume as
    // much data as fits into this many packets.
    size_t max_num_packets;
    bool random_reorder;   // Inefficient: rewrite if used at scale.
    // TODO(rch) should probably be max packets per group.
    bool use_fec;
  };

  QuicPacketCreator(QuicGuid guid, QuicFramer* framer);

  virtual ~QuicPacketCreator();

  // QuicFecBuilderInterface
  virtual void OnBuiltFecProtectedPayload(const QuicPacketHeader& header,
                                          base::StringPiece payload) override;

  typedef std::pair<QuicPacketSequenceNumber, QuicPacket*> PacketPair;

  // Converts a raw payload to a series of QuicPackets.  Returns the number of
  // bytes consumed from data.
  // If data is empty and fin is true, the expected behavior is to consume the
  // fin but return 0.
  size_t DataToStream(QuicStreamId id,
                      base::StringPiece data,
                      QuicStreamOffset offset,
                      bool fin,
                      std::vector<PacketPair>* packets);

  PacketPair ResetStream(QuicStreamId id,
                         QuicStreamOffset offset,
                         QuicErrorCode error);

  PacketPair CloseConnection(QuicConnectionCloseFrame* close_frame);

  PacketPair AckPacket(QuicAckFrame* ack_frame);

  PacketPair CongestionFeedbackPacket(
      QuicCongestionFeedbackFrame* feedback_frame);

  // Increments the current sequence number in QuicPacketCreator and sets it
  // into the packet and returns the new sequence number.
  QuicPacketSequenceNumber SetNewSequenceNumber(QuicPacket* packet);

  QuicPacketSequenceNumber sequence_number() const {
    return sequence_number_;
  }

  void set_sequence_number(QuicPacketSequenceNumber s) {
    sequence_number_ = s;
  }

  Options* options() {
    return &options_;
  }

 private:
  void FillPacketHeader(QuicFecGroupNumber fec_group,
                        QuicPacketFlags flags,
                        QuicPacketHeader* header);

  Options options_;
  QuicGuid guid_;
  QuicFramer* framer_;
  QuicPacketSequenceNumber sequence_number_;
  QuicFecGroupNumber fec_group_number_;
  scoped_ptr<QuicFecGroup> fec_group_;

};

}  // namespace net

#endif  // NET_QUIC_QUIC_PACKET_CREATOR_H_
