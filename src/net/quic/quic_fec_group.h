// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tracks information about an FEC group, including the packets
// that have been seen, and the running parity.  Provided the ability
// to revive a dropped packet.

#ifndef NET_QUIC_QUIC_FEC_GROUP_H_
#define NET_QUIC_QUIC_FEC_GROUP_H_

#include <set>

#include "base/string_piece.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE QuicFecGroup {
 public:
  QuicFecGroup();
  ~QuicFecGroup();

  // Updates the FEC group based on the delivery of a data packet.
  // Returns false if this packet has already been seen, true otherwise.
  bool Update(const QuicPacketHeader& header,
              base::StringPiece decrypted_payload);

  // Updates the FEC group based on the delivery of an FEC packet.
  // Returns false if this packet has already been seen or if it does
  // not claim to protect all the packets previously seen in this group.
  bool UpdateFec(QuicPacketSequenceNumber fec_packet_sequence_number,
                 const QuicFecData& fec);

  // Returns true if a packet can be revived from this FEC group.
  bool CanRevive() const;

  // Returns true if all packets (FEC and data) from this FEC group have been
  // seen or revived
  bool IsFinished() const;

  // Revives the missing packet from this FEC group.  This may return a packet
  // that is null padded to a greater length than the original packet, but
  // the framer will handle it correctly.  Returns the length of the data
  // written to |decrypted_payload|, or 0 if the packet could not be revived.
  size_t Revive(QuicPacketHeader* header,
                char* decrypted_payload,
                size_t decrypted_payload_len);

  // Returns true of this FEC group protects any packets with sequence
  // numbers less than |num|.
  bool ProtectsPacketsBefore(QuicPacketSequenceNumber num) const;

  const base::StringPiece parity() const {
    return base::StringPiece(parity_, parity_len_);
  }

 private:
  bool UpdateParity(base::StringPiece payload);
  // Returns the number of missing packets, or size_t max if the number
  // of missing packets is not known.
  size_t NumMissingPackets() const;

  // Set of packets that we have recevied.
  std::set<QuicPacketSequenceNumber> received_packets_;
  // Sequence number of the first protected packet in this group (the one
  // with the lowest packet sequence number).  Will only be set once the FEC
  // packet has been seen.
  QuicPacketSequenceNumber min_protected_packet_;
  // Sequence number of the last protected packet in this group (the one
  // with the highest packet sequence number).  Will only be set once the FEC
  // packet has been seen.
  QuicPacketSequenceNumber max_protected_packet_;
  // The cumulative parity calculation of all received packets.
  char parity_[kMaxPacketSize];
  size_t parity_len_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_FEC_GROUP_H_
