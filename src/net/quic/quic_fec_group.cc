// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_fec_group.h"

#include <limits>

#include "base/logging.h"

using base::StringPiece;
using std::numeric_limits;
using std::set;

namespace net {

namespace {
const QuicPacketSequenceNumber kNoSequenceNumber = kuint64max;
}  // namespace

QuicFecGroup::QuicFecGroup()
    : min_protected_packet_(kNoSequenceNumber),
      max_protected_packet_(kNoSequenceNumber),
      parity_len_(0) {
}

QuicFecGroup::~QuicFecGroup() {}

bool QuicFecGroup::Update(const QuicPacketHeader& header,
                          StringPiece decrypted_payload) {
  if (received_packets_.count(header.packet_sequence_number) != 0) {
    return false;
  }
  if (min_protected_packet_ != kNoSequenceNumber &&
      max_protected_packet_ != kNoSequenceNumber &&
      (header.packet_sequence_number < min_protected_packet_ ||
       header.packet_sequence_number > max_protected_packet_)) {
    DLOG(ERROR) << "FEC group does not cover received packet: "
                << header.packet_sequence_number;
    return false;
  }
  if (!UpdateParity(decrypted_payload)) {
    return false;
  }
  received_packets_.insert(header.packet_sequence_number);
  return true;
}

bool QuicFecGroup::UpdateFec(
    QuicPacketSequenceNumber fec_packet_sequence_number,
    const QuicFecData& fec) {
  if (min_protected_packet_ != kNoSequenceNumber) {
    return false;
  }
  set<QuicPacketSequenceNumber>::const_iterator it = received_packets_.begin();
  while (it != received_packets_.end()) {
    if ((*it < fec.min_protected_packet_sequence_number) ||
        (*it >= fec_packet_sequence_number)) {
      DLOG(ERROR) << "FEC group does not cover received packet: " << *it;
      return false;
    }
    ++it;
  }
  if (!UpdateParity(fec.redundancy)) {
    return false;
  }
  min_protected_packet_ = fec.min_protected_packet_sequence_number;
  max_protected_packet_ = fec_packet_sequence_number - 1;
  return true;
}

bool QuicFecGroup::CanRevive() const {
  // We can revive if we're missing exactly 1 packet.
  return NumMissingPackets() == 1;
}

bool QuicFecGroup::IsFinished() const {
  // We are finished if we are not missing any packets.
  return NumMissingPackets() == 0;
}

size_t QuicFecGroup::Revive(QuicPacketHeader* header,
                            char* decrypted_payload,
                            size_t decrypted_payload_len) {
  if (!CanRevive()) {
    return 0;
  }

  // Identify the packet sequence number to be resurrected.
  QuicPacketSequenceNumber missing = kNoSequenceNumber;
  for (QuicPacketSequenceNumber i = min_protected_packet_;
       i <= max_protected_packet_; ++i) {
    // Is this packet missing?
    if (received_packets_.count(i) == 0) {
      missing = i;
      break;
    }
  }
  DCHECK_NE(kNoSequenceNumber, missing);

  DCHECK_LE(parity_len_, decrypted_payload_len);
  if (parity_len_ > decrypted_payload_len) {
    return 0;
  }
  for (size_t i = 0; i < parity_len_; ++i) {
    decrypted_payload[i] = parity_[i];
  }
  header->packet_sequence_number = missing;
  received_packets_.insert(missing);
  return parity_len_;
}

bool QuicFecGroup::ProtectsPacketsBefore(QuicPacketSequenceNumber num) const {
  if (max_protected_packet_ != kNoSequenceNumber) {
    return max_protected_packet_ < num;
  }
  // Since we might not yet have recevied the FEC packet, we must check
  // the packets we have received.
  return *received_packets_.begin() < num;
}

bool QuicFecGroup::UpdateParity(StringPiece payload) {
  DCHECK_LE(payload.size(), kMaxPacketSize);
  if (payload.size() > kMaxPacketSize) {
    DLOG(ERROR) << "Illegal payload size: " << payload.size();
    return false;
  }
  if (parity_len_ < payload.size()) {
    parity_len_ = payload.size();
  }
  DCHECK_LE(payload.size(), kMaxPacketSize);
  if (received_packets_.size() == 0 &&
      min_protected_packet_ == kNoSequenceNumber) {
    // Initialize the parity to the value of this payload
    memcpy(parity_, payload.data(), payload.size());
    if (payload.size() < kMaxPacketSize) {
      // TODO(rch): expand as needed.
      memset(parity_ + payload.size(), 0,
             kMaxPacketSize - payload.size());
    }
    return true;
  }
  // Update the parity by XORing in the data (padding with 0s if necessary).
  for (size_t i = 0; i < kMaxPacketSize; ++i) {
    uint8 byte = i < payload.size() ? payload[i] : 0x00;
    parity_[i] ^= byte;
  }
  return true;
}

size_t QuicFecGroup::NumMissingPackets() const {
  if (min_protected_packet_ == kNoSequenceNumber)
    return numeric_limits<size_t>::max();
  return (max_protected_packet_ - min_protected_packet_ + 1) -
      received_packets_.size();
}

}  // namespace net
