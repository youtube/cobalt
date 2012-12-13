// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/logging.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using std::make_pair;
using std::min;
using std::pair;
using std::vector;

namespace net {

QuicPacketCreator::QuicPacketCreator(QuicGuid guid, QuicFramer* framer)
    : guid_(guid),
      framer_(framer),
      sequence_number_(0),
      fec_group_number_(1) {
  framer_->set_fec_builder(this);
}

QuicPacketCreator::~QuicPacketCreator() {
}

void QuicPacketCreator::OnBuiltFecProtectedPayload(
    const QuicPacketHeader& header,
    StringPiece payload) {
  if (fec_group_.get()) {
    fec_group_->Update(header, payload);
  }
}

size_t QuicPacketCreator::DataToStream(QuicStreamId id,
                                       StringPiece data,
                                       QuicStreamOffset offset,
                                       bool fin,
                                       vector<PacketPair>* packets) {
  DCHECK_GT(options_.max_packet_length,
            QuicUtils::StreamFramePacketOverhead(1));

  QuicPacketHeader header;

  QuicPacket* packet = NULL;
  QuicFrames frames;
  QuicFecGroupNumber current_fec_group = 0;
  QuicFecData fec_data;

  int num_data_packets = options_.max_num_packets;

  if (options_.use_fec) {
    DCHECK_LT(1u, options_.max_num_packets);
    --num_data_packets;
    DCHECK(!fec_group_.get());
    fec_group_.reset(new QuicFecGroup);
    current_fec_group = fec_group_number_;
    fec_data.fec_group = current_fec_group;
    fec_data.min_protected_packet_sequence_number = sequence_number_ + 1;
  }

  size_t unconsumed_bytes = data.size();
  if (data.size() != 0) {
    size_t max_frame_len = framer_->GetMaxPlaintextSize(
        options_.max_packet_length -
        QuicUtils::StreamFramePacketOverhead(1));
    DCHECK_GT(max_frame_len, 0u);
    size_t frame_len = min<size_t>(max_frame_len, unconsumed_bytes);

    while (unconsumed_bytes > 0 && num_data_packets > 0) {
      --num_data_packets;
      bool set_fin = false;
      if (unconsumed_bytes <= frame_len) {  // last loop
        frame_len = min(unconsumed_bytes, frame_len);
        set_fin = fin && !options_.separate_fin_packet;
      }
      StringPiece data_frame(data.data() + data.size() - unconsumed_bytes,
                                frame_len);

      QuicStreamFrame frame(id, set_fin, offset, data_frame);
      frames.push_back(QuicFrame(&frame));
      FillPacketHeader(current_fec_group, PACKET_FLAGS_NONE, &header);
      offset += frame_len;
      unconsumed_bytes -= frame_len;

      // Produce the data packet (which might fin the stream).
      framer_->ConstructFrameDataPacket(header, frames, &packet);
      DCHECK_GE(options_.max_packet_length, packet->length());
      packets->push_back(make_pair(header.packet_sequence_number, packet));
      frames.clear();
    }
    // If we haven't finished serializing all the data, don't set any final fin.
    if (unconsumed_bytes > 0) {
      fin = false;
    }
  }

  // Create a new packet for the fin, if necessary.
  // We intentionally don't worry about separate_fin_packet messing with
  // max_number_of_packets: we'd rather queue an extra packet than deal with a
  // fin not getting consumed with the last data in a packet.
  if (fin && (options_.separate_fin_packet || data.size() == 0)) {
    FillPacketHeader(current_fec_group, PACKET_FLAGS_NONE, &header);
    QuicStreamFrame frame(id, true, offset, "");
    frames.push_back(QuicFrame(&frame));
    framer_->ConstructFrameDataPacket(header, frames, &packet);
    packets->push_back(make_pair(header.packet_sequence_number, packet));
    frames.clear();
  }

  // Create a new FEC packet, if necessary
  if (current_fec_group != 0) {
    FillPacketHeader(current_fec_group, PACKET_FLAGS_FEC, &header);
    fec_data.redundancy = fec_group_->parity();
    QuicPacket* fec_packet;
    framer_->ConstructFecPacket(header, fec_data, &fec_packet);
    packets->push_back(make_pair(header.packet_sequence_number, fec_packet));
    ++fec_group_number_;
  }
  /*
  if (options_.random_reorder) {
    int32 seed = ACMRandom::HostnamePidTimeSeed();
    ACMRandom random(seed);
    DLOG(INFO) << "Seed " << seed;

    vector<PacketPair> tmp_store;
    tmp_store.swap(*packets);

    while (tmp_store.size() != 0) {
      int idx = random.Uniform(tmp_store.size());
      packets->push_back(tmp_store[idx]);
      tmp_store.erase(tmp_store.begin() + idx);
    }
  }
  */
  fec_group_.reset(NULL);
  DCHECK(options_.max_num_packets >= packets->size() ||
         options_.separate_fin_packet);

  return data.size() - unconsumed_bytes;
}

QuicPacketCreator::PacketPair QuicPacketCreator::ResetStream(
    QuicStreamId id,
    QuicStreamOffset offset,
    QuicErrorCode error) {
  QuicPacketHeader header;
  FillPacketHeader(0, PACKET_FLAGS_NONE, &header);

  QuicRstStreamFrame close_frame(id, offset, error);

  QuicPacket* packet;
  QuicFrames frames;
  frames.push_back(QuicFrame(&close_frame));
  framer_->ConstructFrameDataPacket(header, frames, &packet);
  return make_pair(header.packet_sequence_number, packet);
}

QuicPacketCreator::PacketPair QuicPacketCreator::CloseConnection(
    QuicConnectionCloseFrame* close_frame) {

  QuicPacketHeader header;
  FillPacketHeader(0, PACKET_FLAGS_NONE, &header);

  QuicPacket* packet;
  QuicFrames frames;
  frames.push_back(QuicFrame(close_frame));
  framer_->ConstructFrameDataPacket(header, frames, &packet);
  return make_pair(header.packet_sequence_number, packet);
}

QuicPacketCreator::PacketPair QuicPacketCreator::AckPacket(
    QuicAckFrame* ack_frame) {

  QuicPacketHeader header;
  FillPacketHeader(0, PACKET_FLAGS_NONE, &header);

  QuicPacket* packet;
  QuicFrames frames;
  frames.push_back(QuicFrame(ack_frame));
  framer_->ConstructFrameDataPacket(header, frames, &packet);
  return make_pair(header.packet_sequence_number, packet);
}

QuicPacketSequenceNumber QuicPacketCreator::SetNewSequenceNumber(
    QuicPacket* packet) {
  ++sequence_number_;
  framer_->WriteSequenceNumber(sequence_number_, packet);
  return sequence_number_;
}

void QuicPacketCreator::FillPacketHeader(QuicFecGroupNumber fec_group,
                                         QuicPacketFlags flags,
                                         QuicPacketHeader* header) {
  header->guid = guid_;
  header->flags = flags;
  header->packet_sequence_number = ++sequence_number_;
  header->fec_group = fec_group;
}

}  // namespace net
