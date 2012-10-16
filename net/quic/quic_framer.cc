// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_framer.h"

#include "base/hash_tables.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_data_writer.h"
#include "net/quic/quic_utils.h"

using base::hash_set;
using base::StringPiece;

namespace net {

QuicFramer::QuicFramer(QuicDecrypter* decrypter, QuicEncrypter* encrypter)
    : visitor_(NULL),
      fec_builder_(NULL),
      error_(QUIC_NO_ERROR),
      decrypter_(decrypter),
      encrypter_(encrypter) {
}

QuicFramer::~QuicFramer() {}

bool QuicFramer::ConstructFragementDataPacket(
    const QuicPacketHeader& header,
    const QuicFragments& fragments,
    QuicPacket** packet) {
  // Compute the length of the packet.  We use "magic numbers" here because
  // sizeof(member_) is not necessairly the same as sizeof(member_wire_format).
  size_t len = kPacketHeaderSize;
  len += 1;  // fragment count
  for (size_t i = 0; i < fragments.size(); ++i) {
    len += 1;  // space for the 8 bit type
    len += ComputeFragmentPayloadLength(fragments[i]);
  }

  QuicDataWriter writer(len);

  if (!WritePacketHeader(header, &writer)) {
    return false;
  }

  // fragment count
  DCHECK_GE(256u, fragments.size());
  if (!writer.WriteUInt8(fragments.size())) {
    return false;
  }

  for (size_t i = 0; i < fragments.size(); ++i) {
    const QuicFragment& fragment = fragments[i];
    if (!writer.WriteUInt8(fragment.type)) {
          return false;
    }

    switch (fragment.type) {
      case STREAM_FRAGMENT:
        if (!AppendStreamFragmentPayload(*fragment.stream_fragment,
                                         &writer)) {
          return false;
        }
        break;
      case PDU_FRAGMENT:
        return RaiseError(QUIC_INVALID_FRAGMENT_DATA);
      case ACK_FRAGMENT:
        if (!AppendAckFragmentPayload(*fragment.ack_fragment, &writer)) {
          return false;
        }
        break;
      case RST_STREAM_FRAGMENT:
        if (!AppendRstStreamFragmentPayload(*fragment.rst_stream_fragment,
                                            &writer)) {
          return false;
        }
        break;
      case CONNECTION_CLOSE_FRAGMENT:
        if (!AppendConnectionCloseFragmentPayload(
            *fragment.connection_close_fragment, &writer)) {
          return false;
        }
        break;
      default:
        return RaiseError(QUIC_INVALID_FRAGMENT_DATA);
    }
  }

  *packet = new QuicPacket(writer.take(), len, true);
  if (fec_builder_) {
    fec_builder_->OnBuiltFecProtectedPayload(header,
                                             (*packet)->FecProtectedData());
  }

  return true;
}

bool QuicFramer::ConstructFecPacket(const QuicPacketHeader& header,
                                    const QuicFecData& fec,
                                    QuicPacket** packet) {
  // Compute the length of the packet.  We use "magic numbers" here because
  // sizeof(member_) is not necessairly the same as sizeof(member_wire_format).
  size_t len = kPacketHeaderSize;
  len += 6;  // first protected packet sequence number
  len += fec.redundancy.length();

  QuicDataWriter writer(len);

  if (!WritePacketHeader(header, &writer)) {
    return false;
  }

  if (!writer.WriteUInt48(fec.first_protected_packet_sequence_number)) {
    return false;
  }

  if (!writer.WriteBytes(fec.redundancy.data(), fec.redundancy.length())) {
    return false;
  }

  *packet = new QuicPacket(writer.take(), len, true);

  return true;
}

void QuicFramer::IncrementRetransmitCount(QuicPacket* packet) {
  CHECK_GT(packet->length(), kPacketHeaderSize);

  ++packet->mutable_data()[kRetransmissionOffset];
}

uint8 QuicFramer::GetRetransmitCount(QuicPacket* packet) {
  CHECK_GT(packet->length(), kPacketHeaderSize);

  return packet->mutable_data()[kRetransmissionOffset];
}

bool QuicFramer::ProcessPacket(const IPEndPoint& peer_address,
                               const QuicEncryptedPacket& packet) {
  DCHECK(!reader_.get());
  reader_.reset(new QuicDataReader(packet.data(), packet.length()));
  visitor_->OnPacket(peer_address);

  // First parse the packet header.
  QuicPacketHeader header;
  if (!ProcessPacketHeader(&header, packet)) {
    DLOG(WARNING) << "Unable to process header.";
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  if (!visitor_->OnPacketHeader(header)) {
    reader_.reset(NULL);
    return true;
  }

  if (packet.length() > kMaxPacketSize) {
    DLOG(WARNING) << "Packet too large: " << packet.length();
    return RaiseError(QUIC_PACKET_TOO_LARGE);
  }

  // Handle the payload.
  if ((header.flags & PACKET_FLAGS_FEC) == 0) {
    if (header.fec_group != 0) {
      StringPiece payload = reader_->PeekRemainingPayload();
      visitor_->OnFecProtectedPayload(payload);
    }
    if (!ProcessFragmentData()) {
      DLOG(WARNING) << "Unable to process fragment data.";
      return false;
    }
  } else {
    QuicFecData fec_data;
    fec_data.fec_group = header.fec_group;
    if (!reader_->ReadUInt48(
            &fec_data.first_protected_packet_sequence_number)) {
      set_detailed_error("Unable to read first protected packet.");
      return false;
    }

    fec_data.redundancy = reader_->ReadRemainingPayload();
    visitor_->OnFecData(fec_data);
  }

  visitor_->OnPacketComplete();
  reader_.reset(NULL);
  return true;
}

bool QuicFramer::ProcessRevivedPacket(const IPEndPoint& peer_address,
                                      const QuicPacketHeader& header,
                                      StringPiece payload) {
  DCHECK(!reader_.get());

  visitor_->OnPacket(peer_address);

  visitor_->OnPacketHeader(header);

  if (payload.length() > kMaxPacketSize) {
    set_detailed_error("Revived packet too large.");
    return RaiseError(QUIC_PACKET_TOO_LARGE);
  }

  reader_.reset(new QuicDataReader(payload.data(), payload.length()));
  if (!ProcessFragmentData()) {
    DLOG(WARNING) << "Unable to process fragment data.";
    return false;
  }

  visitor_->OnPacketComplete();
  reader_.reset(NULL);
  return true;
}

bool QuicFramer::WritePacketHeader(const QuicPacketHeader& header,
                                   QuicDataWriter* writer) {
  // ConnectionHeader
  if (!writer->WriteUInt64(header.guid)) {
    return false;
  }

  if (!writer->WriteUInt48(header.packet_sequence_number)) {
    return false;
  }

  if (!writer->WriteBytes(&header.retransmission_count, 1)) {
    return false;
  }

  // CongestionMonitoredHeader
  if (!writer->WriteUInt64(header.transmission_time)) {
    return false;
  }

  uint8 flags = static_cast<uint8>(header.flags);
  if (!writer->WriteBytes(&flags, 1)) {
     return false;
  }

  if (!writer->WriteBytes(&header.fec_group, 1)) {
    return false;
  }

  return true;
}

bool QuicFramer::ProcessPacketHeader(QuicPacketHeader* header,
                                     const QuicEncryptedPacket& packet) {
  // ConnectionHeader
  if (!reader_->ReadUInt64(&header->guid)) {
    set_detailed_error("Unable to read GUID.");
    return false;
  }

  if (!reader_->ReadUInt48(&header->packet_sequence_number)) {
    set_detailed_error("Unable to read sequence number.");
    return false;
  }

  if (!reader_->ReadBytes(&header->retransmission_count, 1)) {
    set_detailed_error("Unable to read retransmission count.");
    return false;
  }

  // CongestionMonitoredHeader
  if (!reader_->ReadUInt64(&header->transmission_time)) {
    set_detailed_error("Unable to read transmission time.");
    return false;
  }

  unsigned char flags;
  if (!reader_->ReadBytes(&flags, 1)) {
    set_detailed_error("Unable to read flags.");
    return false;
  }

  if (flags > PACKET_FLAGS_MAX) {
    set_detailed_error("Illegal flags value.");
    return false;
  }

  header->flags = static_cast<QuicPacketFlags>(flags);

  if (!DecryptPayload(packet)) {
    DLOG(WARNING) << "Unable to decrypt payload.";
    return RaiseError(QUIC_DECRYPTION_FAILURE);
  }

  if (!reader_->ReadBytes(&header->fec_group, 1)) {
    set_detailed_error("Unable to read fec group.");
    return false;
  }

  return true;
}

bool QuicFramer::ProcessFragmentData() {
  uint8 fragment_count;
  if (!reader_->ReadBytes(&fragment_count, 1)) {
    set_detailed_error("Unable to read fragment count.");
    return RaiseError(QUIC_INVALID_FRAGMENT_DATA);
  }

  for (uint8 i = 0; i < fragment_count; ++i) {
    uint8 fragment_type;
    if (!reader_->ReadBytes(&fragment_type, 1)) {
      set_detailed_error("Unable to read fragment type.");
      return RaiseError(QUIC_INVALID_FRAGMENT_DATA);
    }
    switch (fragment_type) {
      case STREAM_FRAGMENT:
        if (!ProcessStreamFragment()) {
          return RaiseError(QUIC_INVALID_FRAGMENT_DATA);
        }
        break;
      case PDU_FRAGMENT:
        if (!ProcessPDUFragment()) {
          return RaiseError(QUIC_INVALID_FRAGMENT_DATA);
        }
        break;
      case ACK_FRAGMENT: {
        QuicAckFragment fragment;
        if (!ProcessAckFragment(&fragment)) {
          return RaiseError(QUIC_INVALID_FRAGMENT_DATA);
        }
        break;
      }
      case RST_STREAM_FRAGMENT:
        if (!ProcessRstStreamFragment()) {
          return RaiseError(QUIC_INVALID_RST_STREAM_DATA);
        }
        break;
      case CONNECTION_CLOSE_FRAGMENT:
        if (!ProcessConnectionCloseFragment()) {
          return RaiseError(QUIC_INVALID_CONNECTION_CLOSE_DATA);
        }
        break;
      default:
        set_detailed_error("Illegal fragment type.");
        DLOG(WARNING) << "Illegal fragment type: " << (int)fragment_type;
        return RaiseError(QUIC_INVALID_FRAGMENT_DATA);
    }
  }

  return true;
}

bool QuicFramer::ProcessStreamFragment() {
  QuicStreamFragment fragment;
  if (!reader_->ReadUInt32(&fragment.stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  uint8 fin;
  if (!reader_->ReadBytes(&fin, 1)) {
    set_detailed_error("Unable to read fin.");
    return false;
  }
  if (fin > 1) {
    set_detailed_error("Invalid fin value.");
    return false;
  }
  fragment.fin = (fin == 1);

  if (!reader_->ReadUInt64(&fragment.offset)) {
    set_detailed_error("Unable to read offset.");
    return false;
  }

  if (!reader_->ReadStringPiece16(&fragment.data)) {
    set_detailed_error("Unable to read fragment data.");
    return false;
  }

  visitor_->OnStreamFragment(fragment);
  return true;
}

bool QuicFramer::ProcessPDUFragment() {
  return false;
}

bool QuicFramer::ProcessAckFragment(QuicAckFragment* fragment) {
  if (!reader_->ReadUInt48(&fragment->received_info.largest_received)) {
    set_detailed_error("Unable to read largest received.");
    return false;
  }

  if (!reader_->ReadUInt64(&fragment->received_info.time_received)) {
    set_detailed_error("Unable to read time received.");
    return false;
  }

  uint8 num_unacked_packets;
  if (!reader_->ReadBytes(&num_unacked_packets, 1)) {
    set_detailed_error("Unable to read num unacked packets.");
    return false;
  }

  for (int i = 0; i < num_unacked_packets; ++i) {
    QuicPacketSequenceNumber sequence_number;
    if (!reader_->ReadUInt48(&sequence_number)) {
      set_detailed_error("Unable to read sequence number in unacked packets.");
      return false;
    }
    fragment->received_info.missing_packets.insert(sequence_number);
  }

  if (!reader_->ReadUInt48(&fragment->sent_info.least_unacked)) {
    set_detailed_error("Unable to read least unacked.");
    return false;
  }

  uint8 num_non_retransmiting_packets;
  if (!reader_->ReadBytes(&num_non_retransmiting_packets, 1)) {
    set_detailed_error("Unable to read num non-retransmitting.");
    return false;
  }
  for (uint8 i = 0; i < num_non_retransmiting_packets; ++i) {
    QuicPacketSequenceNumber sequence_number;
    if (!reader_->ReadUInt48(&sequence_number)) {
      set_detailed_error(
          "Unable to read sequence number in non-retransmitting.");
      return false;
    }
    fragment->sent_info.non_retransmiting.insert(sequence_number);
  }

  uint8 congestion_info_type;
  if (!reader_->ReadBytes(&congestion_info_type, 1)) {
    set_detailed_error("Unable to read congestion info type.");
    return false;
  }
  fragment->congestion_info.type =
      static_cast<CongestionFeedbackType>(congestion_info_type);

  switch (fragment->congestion_info.type) {
    case kNone:
      break;
    case kInterArrival: {
      CongestionFeedbackMessageInterArrival* inter_arrival =
          &fragment->congestion_info.inter_arrival;
      if (!reader_->ReadUInt16(
              &inter_arrival->accumulated_number_of_lost_packets)) {
        set_detailed_error(
            "Unable to read accumulated number of lost packets.");
        return false;
      }
      if (!reader_->ReadBytes(&inter_arrival->offset_time, 2)) {
        set_detailed_error("Unable to read offset time.");
        return false;
      }
      if (!reader_->ReadUInt16(&inter_arrival->delta_time)) {
        set_detailed_error("Unable to read delta time.");
        return false;
      }
      break;
    }
    case kFixRate: {
      CongestionFeedbackMessageFixRate* fix_rate =
          &fragment->congestion_info.fix_rate;
      if (!reader_->ReadUInt32(&fix_rate->bitrate_in_bytes_per_second)) {
        set_detailed_error("Unable to read bitrate.");
        return false;
      }
      break;
    }
    case kTCP: {
      CongestionFeedbackMessageTCP* tcp = &fragment->congestion_info.tcp;
      if (!reader_->ReadUInt16(&tcp->accumulated_number_of_lost_packets)) {
        set_detailed_error(
            "Unable to read accumulated number of lost packets.");
        return false;
      }
      if (!reader_->ReadUInt16(&tcp->receive_window)) {
        set_detailed_error("Unable to read receive window.");
        return false;
      }
      break;
    }
    default:
      set_detailed_error("Illegal congestion info type.");
      DLOG(WARNING) << "Illegal congestion info type: "
                    << fragment->congestion_info.type;
      return RaiseError(QUIC_INVALID_FRAGMENT_DATA);
  }

  visitor_->OnAckFragment(*fragment);
  return true;
}

bool QuicFramer::ProcessRstStreamFragment() {
  QuicRstStreamFragment fragment;
  if (!reader_->ReadUInt32(&fragment.stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  if (!reader_->ReadUInt64(&fragment.offset)) {
    set_detailed_error("Unable to read offset in rst fragment.");
    return false;
  }

  uint32 details;
  if (!reader_->ReadUInt32(&details)) {
    set_detailed_error("Unable to read rst stream details.");
    return false;
  }
  fragment.details = static_cast<QuicErrorCode>(details);

  visitor_->OnRstStreamFragment(fragment);
  return true;
}

bool QuicFramer::ProcessConnectionCloseFragment() {
  QuicConnectionCloseFragment fragment;

  uint32 details;
  if (!reader_->ReadUInt32(&details)) {
    set_detailed_error("Unable to read connection close details.");
    return false;
  }
  fragment.details = static_cast<QuicErrorCode>(details);

  if (!ProcessAckFragment(&fragment.ack_fragment)) {
    DLOG(WARNING) << "Unable to process ack fragment.";
    return false;
  }

  visitor_->OnConnectionCloseFragment(fragment);
  return true;
}

void QuicFramer::WriteTransmissionTime(QuicTransmissionTime time,
                                       QuicPacket* packet) {
  QuicDataWriter::WriteUint64ToBuffer(
      time, packet->mutable_data() + kTransmissionTimeOffset);
}

QuicEncryptedPacket* QuicFramer::EncryptPacket(const QuicPacket& packet) {
  scoped_ptr<QuicData> out(encrypter_->Encrypt(packet.AssociatedData(),
                                               packet.Plaintext()));
  if (out.get() == NULL) {
    RaiseError(QUIC_ENCRYPTION_FAILURE);
    return NULL;
  }
  size_t len = kStartOfEncryptedData + out->length();
  char* buffer = new char[len];
  // TODO(rch): eliminate this buffer copy by passing in a buffer to Encrypt().
  memcpy(buffer, packet.data(), kStartOfEncryptedData);
  memcpy(buffer + kStartOfEncryptedData, out->data(), out->length());
  return new QuicEncryptedPacket(buffer, len, true);
}

size_t QuicFramer::GetMaxPlaintextSize(size_t ciphertext_size) {
  return encrypter_->GetMaxPlaintextSize(ciphertext_size);
}

bool QuicFramer::DecryptPayload(const QuicEncryptedPacket& packet) {
  StringPiece encrypted;
  if (!reader_->ReadStringPiece(&encrypted, reader_->BytesRemaining())) {
    return false;
  }
  DCHECK(decrypter_.get() != NULL);
  decrypted_.reset(decrypter_->Decrypt(packet.AssociatedData(), encrypted));
  if  (decrypted_.get() == NULL) {
    return false;
  }

  reader_.reset(new QuicDataReader(decrypted_->data(), decrypted_->length()));
  return true;
}

size_t QuicFramer::ComputeFragmentPayloadLength(const QuicFragment& fragment) {
  size_t len = 0;
  // We use "magic numbers" here because sizeof(member_) is not necessairly the
  // same as sizeof(member_wire_format).
  switch (fragment.type) {
    case STREAM_FRAGMENT:
      len += 4;  // stream id
      len += 1;  // fin
      len += 8;  // offset
      len += 2;  // space for the 16 bit length
      len += fragment.stream_fragment->data.size();
      break;
    case PDU_FRAGMENT:
      DLOG(INFO) << "PDU_FRAGMENT not yet supported";
      break;  // Need to support this eventually :>
    case ACK_FRAGMENT: {
      const QuicAckFragment& ack = *fragment.ack_fragment;
      len += 6;  // largest received packet sequence number
      len += 8;  // time delta
      len += 1;  // num missing packets
      len += 6 * ack.received_info.missing_packets.size();
      len += 6;  // least packet sequence number awaiting an ack
      len += 1;  // num non retransmitting packets
      len += 6 * ack.sent_info.non_retransmiting.size();
      len += 1;  // congestion control type
      switch (ack.congestion_info.type) {
        case kNone:
          break;
        case kInterArrival:
          len += 6;
          break;
        case kFixRate:
          len += 4;
          break;
        case kTCP:
          len += 4;
          break;
        default:
          set_detailed_error("Illegal feedback type.");
          DLOG(INFO) << "Illegal feedback type: " << ack.congestion_info.type;
          break;
      }
      break;
    }
    case RST_STREAM_FRAGMENT:
      len += 4;  // stream id
      len += 8;  // offset
      len += 4;  // details
      break;
    case CONNECTION_CLOSE_FRAGMENT:
      len += 4;  // details
      len += ComputeFragmentPayloadLength(
          QuicFragment(&fragment.connection_close_fragment->ack_fragment));
      break;
    default:
      set_detailed_error("Illegal fragment type.");
      DLOG(INFO) << "Illegal fragment type: " << fragment.type;
      break;
  }
  return len;
}

bool QuicFramer::AppendStreamFragmentPayload(
    const QuicStreamFragment& fragment,
    QuicDataWriter* writer) {
  if (!writer->WriteUInt32(fragment.stream_id)) {
    return false;
  }
  if (!writer->WriteUInt8(fragment.fin)) {
    return false;
  }
  if (!writer->WriteUInt64(fragment.offset)) {
    return false;
  }
  if (!writer->WriteUInt16(fragment.data.size())) {
    return false;
  }
  if (!writer->WriteBytes(fragment.data.data(),
                           fragment.data.size())) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendAckFragmentPayload(
    const QuicAckFragment& fragment,
    QuicDataWriter* writer) {
  if (!writer->WriteUInt48(fragment.received_info.largest_received)) {
    return false;
  }
  if (!writer->WriteUInt64(fragment.received_info.time_received)) {
    return false;
  }

  size_t num_unacked_packets = fragment.received_info.missing_packets.size();
  if (!writer->WriteBytes(&num_unacked_packets, 1)) {
    return false;
  }

  hash_set<QuicPacketSequenceNumber>::const_iterator it =
      fragment.received_info.missing_packets.begin();
  for (; it != fragment.received_info.missing_packets.end(); ++it) {
    if (!writer->WriteUInt48(*it)) {
      return false;
    }
  }

  if (!writer->WriteUInt48(fragment.sent_info.least_unacked)) {
    return false;
  }

  size_t num_non_retransmiting_packets =
      fragment.sent_info.non_retransmiting.size();
  if (!writer->WriteBytes(&num_non_retransmiting_packets, 1)) {
    return false;
  }

  it = fragment.sent_info.non_retransmiting.begin();
  while (it != fragment.sent_info.non_retransmiting.end()) {
    if (!writer->WriteUInt48(*it)) {
      return false;
    }
    ++it;
  }

  if (!writer->WriteBytes(&fragment.congestion_info.type, 1)) {
    return false;
  }

  switch (fragment.congestion_info.type) {
    case kNone:
      break;
    case kInterArrival: {
      const CongestionFeedbackMessageInterArrival& inter_arrival =
          fragment.congestion_info.inter_arrival;
      if (!writer->WriteUInt16(
              inter_arrival.accumulated_number_of_lost_packets)) {
        return false;
      }
      if (!writer->WriteBytes(&inter_arrival.offset_time, 2)) {
        return false;
      }
      if (!writer->WriteUInt16(inter_arrival.delta_time)) {
        return false;
      }
      break;
    }
    case kFixRate: {
      const CongestionFeedbackMessageFixRate& fix_rate =
          fragment.congestion_info.fix_rate;
      if (!writer->WriteUInt32(fix_rate.bitrate_in_bytes_per_second)) {
        return false;
      }
      break;
    }
    case kTCP: {
      const CongestionFeedbackMessageTCP& tcp = fragment.congestion_info.tcp;
      if (!writer->WriteUInt16(tcp.accumulated_number_of_lost_packets)) {
        return false;
      }
      if (!writer->WriteUInt16(tcp.receive_window)) {
        return false;
      }
      break;
    }
    default:
      return false;
  }

  return true;
}

bool QuicFramer::AppendRstStreamFragmentPayload(
        const QuicRstStreamFragment& fragment,
        QuicDataWriter* writer) {
  if (!writer->WriteUInt32(fragment.stream_id)) {
    return false;
  }
  if (!writer->WriteUInt64(fragment.offset)) {
    return false;
  }

  uint32 details = static_cast<uint32>(fragment.details);
  if (!writer->WriteUInt32(details)) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendConnectionCloseFragmentPayload(
    const QuicConnectionCloseFragment& fragment,
    QuicDataWriter* writer) {
  uint32 details = static_cast<uint32>(fragment.details);
  if (!writer->WriteUInt32(details)) {
    return false;
  }
  AppendAckFragmentPayload(fragment.ack_fragment, writer);
  return true;
}

bool QuicFramer::RaiseError(QuicErrorCode error) {
  DLOG(INFO) << detailed_error_;
  set_error(error);
  visitor_->OnError(this);
  reader_.reset(NULL);
  return false;
}

}  // namespace net
