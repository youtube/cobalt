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

using base::StringPiece;
using std::map;
using std::numeric_limits;

namespace net {

QuicFramer::QuicFramer(QuicDecrypter* decrypter, QuicEncrypter* encrypter)
    : visitor_(NULL),
      fec_builder_(NULL),
      error_(QUIC_NO_ERROR),
      decrypter_(decrypter),
      encrypter_(encrypter) {
}

QuicFramer::~QuicFramer() {}

bool QuicFramer::ConstructFrameDataPacket(
    const QuicPacketHeader& header,
    const QuicFrames& frames,
    QuicPacket** packet) {
  // Compute the length of the packet.  We use "magic numbers" here because
  // sizeof(member_) is not necessarily the same as sizeof(member_wire_format).
  size_t len = kPacketHeaderSize;
  len += 1;  // frame count
  for (size_t i = 0; i < frames.size(); ++i) {
    len += 1;  // space for the 8 bit type
    len += ComputeFramePayloadLength(frames[i]);
  }

  QuicDataWriter writer(len);

  if (!WritePacketHeader(header, &writer)) {
    return false;
  }

  // frame count
  if (frames.size() > 256u) {
    return false;
  }
  if (!writer.WriteUInt8(frames.size())) {
    return false;
  }

  for (size_t i = 0; i < frames.size(); ++i) {
    const QuicFrame& frame = frames[i];
    if (!writer.WriteUInt8(frame.type)) {
          return false;
    }

    switch (frame.type) {
      case STREAM_FRAME:
        if (!AppendStreamFramePayload(*frame.stream_frame,
                                         &writer)) {
          return false;
        }
        break;
      case PDU_FRAME:
        return RaiseError(QUIC_INVALID_FRAME_DATA);
      case ACK_FRAME:
        if (!AppendAckFramePayload(*frame.ack_frame, &writer)) {
          return false;
        }
        break;
      case CONGESTION_FEEDBACK_FRAME:
        if (!AppendQuicCongestionFeedbackFramePayload(
                *frame.congestion_feedback_frame, &writer)) {
          return false;
        }
        break;
      case RST_STREAM_FRAME:
        if (!AppendRstStreamFramePayload(*frame.rst_stream_frame,
                                            &writer)) {
          return false;
        }
        break;
      case CONNECTION_CLOSE_FRAME:
        if (!AppendConnectionCloseFramePayload(
            *frame.connection_close_frame, &writer)) {
          return false;
        }
        break;
      default:
        return RaiseError(QUIC_INVALID_FRAME_DATA);
    }
  }

  DCHECK_EQ(len, writer.length());
  *packet = new QuicPacket(writer.take(), len, true, PACKET_FLAGS_NONE);
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

  if (!writer.WriteUInt48(fec.min_protected_packet_sequence_number)) {
    return false;
  }

  if (!writer.WriteBytes(fec.redundancy.data(), fec.redundancy.length())) {
    return false;
  }

  *packet = new QuicPacket(writer.take(), len, true, PACKET_FLAGS_FEC);

  return true;
}

bool QuicFramer::ProcessPacket(const IPEndPoint& self_address,
                               const IPEndPoint& peer_address,
                               const QuicEncryptedPacket& packet) {
  DCHECK(!reader_.get());
  reader_.reset(new QuicDataReader(packet.data(), packet.length()));
  visitor_->OnPacket(self_address, peer_address);

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
    if (!ProcessFrameData()) {
      DCHECK_NE(QUIC_NO_ERROR, error_);  // ProcessFrameData sets the error.
      DLOG(WARNING) << "Unable to process frame data.";
      return false;
    }
  } else {
    QuicFecData fec_data;
    fec_data.fec_group = header.fec_group;
    if (!reader_->ReadUInt48(
            &fec_data.min_protected_packet_sequence_number)) {
      set_detailed_error("Unable to read first protected packet.");
      return RaiseError(QUIC_INVALID_FEC_DATA);
    }

    fec_data.redundancy = reader_->ReadRemainingPayload();
    visitor_->OnFecData(fec_data);
  }

  visitor_->OnPacketComplete();
  reader_.reset(NULL);
  return true;
}

bool QuicFramer::ProcessRevivedPacket(const QuicPacketHeader& header,
                                      StringPiece payload) {
  DCHECK(!reader_.get());

  visitor_->OnRevivedPacket();

  visitor_->OnPacketHeader(header);

  if (payload.length() > kMaxPacketSize) {
    set_detailed_error("Revived packet too large.");
    return RaiseError(QUIC_PACKET_TOO_LARGE);
  }

  reader_.reset(new QuicDataReader(payload.data(), payload.length()));
  if (!ProcessFrameData()) {
    DCHECK_NE(QUIC_NO_ERROR, error_);  // ProcessFrameData sets the error.
    DLOG(WARNING) << "Unable to process frame data.";
    return false;
  }

  visitor_->OnPacketComplete();
  reader_.reset(NULL);
  return true;
}

bool QuicFramer::WritePacketHeader(const QuicPacketHeader& header,
                                   QuicDataWriter* writer) {
  if (!writer->WriteUInt64(header.guid)) {
    return false;
  }

  if (!writer->WriteUInt48(header.packet_sequence_number)) {
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
  if (!reader_->ReadUInt64(&header->guid)) {
    set_detailed_error("Unable to read GUID.");
    return false;
  }

  if (!reader_->ReadUInt48(&header->packet_sequence_number)) {
    set_detailed_error("Unable to read sequence number.");
    return false;
  }
  if (header->packet_sequence_number == 0u) {
    set_detailed_error("Packet sequence numbers cannot be 0.");
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

bool QuicFramer::ProcessFrameData() {
  uint8 frame_count;
  if (!reader_->ReadBytes(&frame_count, 1)) {
    set_detailed_error("Unable to read frame count.");
    return RaiseError(QUIC_INVALID_FRAME_DATA);
  }

  for (uint8 i = 0; i < frame_count; ++i) {
    uint8 frame_type;
    if (!reader_->ReadBytes(&frame_type, 1)) {
      set_detailed_error("Unable to read frame type.");
      return RaiseError(QUIC_INVALID_FRAME_DATA);
    }
    switch (frame_type) {
      case STREAM_FRAME:
        if (!ProcessStreamFrame()) {
          return RaiseError(QUIC_INVALID_FRAME_DATA);
        }
        break;
      case PDU_FRAME:
        if (!ProcessPDUFrame()) {
          return RaiseError(QUIC_INVALID_FRAME_DATA);
        }
        break;
      case ACK_FRAME: {
        QuicAckFrame frame;
        if (!ProcessAckFrame(&frame)) {
          return RaiseError(QUIC_INVALID_FRAME_DATA);
        }
        break;
      }
      case CONGESTION_FEEDBACK_FRAME: {
        QuicCongestionFeedbackFrame frame;
        if (!ProcessQuicCongestionFeedbackFrame(&frame)) {
          return RaiseError(QUIC_INVALID_FRAME_DATA);
        }
        break;
      }
      case RST_STREAM_FRAME:
        if (!ProcessRstStreamFrame()) {
          return RaiseError(QUIC_INVALID_RST_STREAM_DATA);
        }
        break;
      case CONNECTION_CLOSE_FRAME:
        if (!ProcessConnectionCloseFrame()) {
          return RaiseError(QUIC_INVALID_CONNECTION_CLOSE_DATA);
        }
        break;
      default:
        set_detailed_error("Illegal frame type.");
        DLOG(WARNING) << "Illegal frame type: " << (int)frame_type;
        return RaiseError(QUIC_INVALID_FRAME_DATA);
    }
  }

  return true;
}

bool QuicFramer::ProcessStreamFrame() {
  QuicStreamFrame frame;
  if (!reader_->ReadUInt32(&frame.stream_id)) {
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
  frame.fin = (fin == 1);

  if (!reader_->ReadUInt64(&frame.offset)) {
    set_detailed_error("Unable to read offset.");
    return false;
  }

  if (!reader_->ReadStringPiece16(&frame.data)) {
    set_detailed_error("Unable to read frame data.");
    return false;
  }

  visitor_->OnStreamFrame(frame);
  return true;
}

bool QuicFramer::ProcessPDUFrame() {
  return false;
}

bool QuicFramer::ProcessAckFrame(QuicAckFrame* frame) {
  if (!ProcessReceivedInfo(&frame->received_info)) {
    return false;
  }
  if (!ProcessSentInfo(&frame->sent_info)) {
    return false;
  }
  visitor_->OnAckFrame(*frame);
  return true;
}

bool QuicFramer::ProcessReceivedInfo(ReceivedPacketInfo* received_info) {
  if (!reader_->ReadUInt48(&received_info->largest_received)) {
     set_detailed_error("Unable to read largest received.");
     return false;
  }

  uint8 num_missing_packets;
  if (!reader_->ReadBytes(&num_missing_packets, 1)) {
    set_detailed_error("Unable to read num missing packets.");
    return false;
  }

  for (int i = 0; i < num_missing_packets; ++i) {
    QuicPacketSequenceNumber sequence_number;
    if (!reader_->ReadUInt48(&sequence_number)) {
      set_detailed_error("Unable to read sequence number in missing packets.");
      return false;
    }
    received_info->missing_packets.insert(sequence_number);
  }

  return true;
}

bool QuicFramer::ProcessSentInfo(SentPacketInfo* sent_info) {
  if (!reader_->ReadUInt48(&sent_info->least_unacked)) {
    set_detailed_error("Unable to read least unacked.");
    return false;
  }

  return true;
}

bool QuicFramer::ProcessQuicCongestionFeedbackFrame(
    QuicCongestionFeedbackFrame* frame) {
  uint8 feedback_type;
  if (!reader_->ReadBytes(&feedback_type, 1)) {
    set_detailed_error("Unable to read congestion feedback type.");
    return false;
  }
  frame->type =
      static_cast<CongestionFeedbackType>(feedback_type);

  switch (frame->type) {
    case kInterArrival: {
      CongestionFeedbackMessageInterArrival* inter_arrival =
          &frame->inter_arrival;
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
      uint8 num_received_packets;
      if (!reader_->ReadBytes(&num_received_packets, 1)) {
        set_detailed_error("Unable to read num received packets.");
        return false;
      }

      if (num_received_packets > 0u) {
        uint64 smallest_received;
        if (!reader_->ReadUInt48(&smallest_received)) {
          set_detailed_error("Unable to read smallest received.");
          return false;
        }

        uint64 time_received_us;
        if (!reader_->ReadUInt64(&time_received_us)) {
          set_detailed_error("Unable to read time received.");
          return false;
        }

        inter_arrival->received_packet_times[smallest_received] =
            QuicTime::FromMicroseconds(time_received_us);

        for (int i = 0; i < num_received_packets - 1; ++i) {
          uint16 sequence_delta;
          if (!reader_->ReadUInt16(&sequence_delta)) {
            set_detailed_error(
                "Unable to read sequence delta in received packets.");
            return false;
          }

          int32 time_delta_us;
          if (!reader_->ReadBytes(&time_delta_us, sizeof(time_delta_us))) {
            set_detailed_error(
                "Unable to read time delta in received packets.");
            return false;
          }
          QuicPacketSequenceNumber packet = smallest_received + sequence_delta;
          inter_arrival->received_packet_times[packet] =
              QuicTime::FromMicroseconds(time_received_us + time_delta_us);
        }
      }
      break;
    }
    case kFixRate: {
      CongestionFeedbackMessageFixRate* fix_rate = &frame->fix_rate;
      if (!reader_->ReadUInt32(&fix_rate->bitrate_in_bytes_per_second)) {
        set_detailed_error("Unable to read bitrate.");
        return false;
      }
      break;
    }
    case kTCP: {
      CongestionFeedbackMessageTCP* tcp = &frame->tcp;
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
      set_detailed_error("Illegal congestion feedback type.");
      DLOG(WARNING) << "Illegal congestion feedback type: "
                    << frame->type;
      return RaiseError(QUIC_INVALID_FRAME_DATA);
  }

  visitor_->OnCongestionFeedbackFrame(*frame);
  return true;
}

bool QuicFramer::ProcessRstStreamFrame() {
  QuicRstStreamFrame frame;
  if (!reader_->ReadUInt32(&frame.stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  if (!reader_->ReadUInt64(&frame.offset)) {
    set_detailed_error("Unable to read offset in rst frame.");
    return false;
  }

  uint32 error_code;
  if (!reader_->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read rst stream error code.");
    return false;
  }
  frame.error_code = static_cast<QuicErrorCode>(error_code);

  StringPiece error_details;
  if (!reader_->ReadStringPiece16(&error_details)) {
    set_detailed_error("Unable to read rst stream error details.");
    return false;
  }
  frame.error_details = error_details.as_string();

  visitor_->OnRstStreamFrame(frame);
  return true;
}

bool QuicFramer::ProcessConnectionCloseFrame() {
  QuicConnectionCloseFrame frame;

  uint32 error_code;
  if (!reader_->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read connection close error code.");
    return false;
  }
  frame.error_code = static_cast<QuicErrorCode>(error_code);

  StringPiece error_details;
  if (!reader_->ReadStringPiece16(&error_details)) {
    set_detailed_error("Unable to read connection close error details.");
    return false;
  }
  frame.error_details = error_details.as_string();

  if (!ProcessAckFrame(&frame.ack_frame)) {
    DLOG(WARNING) << "Unable to process ack frame.";
    return false;
  }

  visitor_->OnConnectionCloseFrame(frame);
  return true;
}

void QuicFramer::WriteSequenceNumber(QuicPacketSequenceNumber sequence_number,
                                     QuicPacket* packet) {
  QuicDataWriter::WriteUint48ToBuffer(
      sequence_number, packet->mutable_data() + kSequenceNumberOffset);
}

void QuicFramer::WriteFecGroup(QuicFecGroupNumber fec_group,
                               QuicPacket* packet) {
  QuicDataWriter::WriteUint8ToBuffer(
      fec_group, packet->mutable_data() + kFecGroupOffset);
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

size_t QuicFramer::ComputeFramePayloadLength(const QuicFrame& frame) {
  size_t len = 0;
  // We use "magic numbers" here because sizeof(member_) is not necessairly the
  // same as sizeof(member_wire_format).
  switch (frame.type) {
    case STREAM_FRAME:
      len += 4;  // stream id
      len += 1;  // fin
      len += 8;  // offset
      len += 2;  // space for the 16 bit length
      len += frame.stream_frame->data.size();
      break;
    case PDU_FRAME:
      DLOG(INFO) << "PDU_FRAME not yet supported";
      break;  // Need to support this eventually :>
    case ACK_FRAME: {
      const QuicAckFrame& ack = *frame.ack_frame;
      len += 6;  // largest received packet sequence number
      len += 1;  // num missing packets
      len += 6 * ack.received_info.missing_packets.size();
      len += 6;  // least packet sequence number awaiting an ack
          break;
    }
    case CONGESTION_FEEDBACK_FRAME: {
      const QuicCongestionFeedbackFrame& congestion_feedback =
          *frame.congestion_feedback_frame;
      len += 1;  // congestion feedback type

      switch (congestion_feedback.type) {
        case kInterArrival: {
          const CongestionFeedbackMessageInterArrival& inter_arrival =
              congestion_feedback.inter_arrival;
          len += 6;
          len += 1;  // num received packets
          if (inter_arrival.received_packet_times.size() > 0) {
            len += 6;  // smallest received
            len += 8;  // time
            // 2 bytes per sequence number delta plus 4 bytes per delta time.
            len += 6 * (inter_arrival.received_packet_times.size() - 1);
          }
          break;
        }
        case kFixRate:
          len += 4;
          break;
        case kTCP:
          len += 4;
          break;
        default:
          set_detailed_error("Illegal feedback type.");
          DLOG(INFO) << "Illegal feedback type: " << congestion_feedback.type;
          break;
      }
      break;
    }
    case RST_STREAM_FRAME:
      len += 4;  // stream id
      len += 8;  // offset
      len += 4;  // error code
      len += 2;  // error details size
      len += frame.rst_stream_frame->error_details.size();
      break;
    case CONNECTION_CLOSE_FRAME:
      len += 4;  // error code
      len += 2;  // error details size
      len += frame.connection_close_frame->error_details.size();
      len += ComputeFramePayloadLength(
          QuicFrame(&frame.connection_close_frame->ack_frame));
      break;
    default:
      set_detailed_error("Illegal frame type.");
      DLOG(INFO) << "Illegal frame type: " << frame.type;
      break;
  }
  return len;
}

bool QuicFramer::AppendStreamFramePayload(
    const QuicStreamFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteUInt32(frame.stream_id)) {
    return false;
  }
  if (!writer->WriteUInt8(frame.fin)) {
    return false;
  }
  if (!writer->WriteUInt64(frame.offset)) {
    return false;
  }
  if (!writer->WriteUInt16(frame.data.size())) {
    return false;
  }
  if (!writer->WriteBytes(frame.data.data(),
                           frame.data.size())) {
    return false;
  }
  return true;
}

// TODO(ianswett): Use varints or another more compact approach for all deltas.
// TODO(ianswett): Deal with acks which are too large to fit into a packet.
bool QuicFramer::AppendAckFramePayload(
    const QuicAckFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteUInt48(frame.received_info.largest_received)) {
    return false;
  }

  DCHECK_GE(numeric_limits<uint8>::max(),
            frame.received_info.missing_packets.size());
  uint8 num_missing_packets = frame.received_info.missing_packets.size();
  if (!writer->WriteBytes(&num_missing_packets, 1)) {
    return false;
  }

  SequenceSet::const_iterator it = frame.received_info.missing_packets.begin();
  for (; it != frame.received_info.missing_packets.end(); ++it) {
    if (!writer->WriteUInt48(*it)) {
      return false;
    }
  }

  if (!writer->WriteUInt48(frame.sent_info.least_unacked)) {
    return false;
  }

  return true;
}

bool QuicFramer::AppendQuicCongestionFeedbackFramePayload(
    const QuicCongestionFeedbackFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteBytes(&frame.type, 1)) {
    return false;
  }

  switch (frame.type) {
    case kInterArrival: {
      const CongestionFeedbackMessageInterArrival& inter_arrival =
          frame.inter_arrival;
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
      DCHECK_GE(numeric_limits<uint8>::max(),
                inter_arrival.received_packet_times.size());
      if (inter_arrival.received_packet_times.size() >
          numeric_limits<uint8>::max()) {
        return false;
      }

      // TODO(ianswett): Make num_received_packets a varint.
      uint8 num_received_packets =
          inter_arrival.received_packet_times.size();
      if (!writer->WriteBytes(&num_received_packets, 1)) {
        return false;
      }
      if (num_received_packets > 0) {
        TimeMap::const_iterator it =
            inter_arrival.received_packet_times.begin();

        QuicPacketSequenceNumber lowest_sequence = it->first;
        if (!writer->WriteUInt48(lowest_sequence)) {
          return false;
        }

        QuicTime lowest_time = it->second;
        // TODO(ianswett): Use time deltas from the connection's first received
        // packet.
        if (!writer->WriteUInt64(lowest_time.ToMicroseconds())) {
          return false;
        }

        for (++it; it != inter_arrival.received_packet_times.end(); ++it) {
          QuicPacketSequenceNumber sequence_delta = it->first - lowest_sequence;
          DCHECK_GE(numeric_limits<uint16>::max(), sequence_delta);
          if (sequence_delta > numeric_limits<uint16>::max()) {
            return false;
          }
          if (!writer->WriteUInt16(static_cast<uint16>(sequence_delta))) {
            return false;
          }

          int32 time_delta_us =
              it->second.Subtract(lowest_time).ToMicroseconds();
          if (!writer->WriteBytes(&time_delta_us, sizeof(time_delta_us))) {
            return false;
          }
        }
      }
      break;
    }
    case kFixRate: {
      const CongestionFeedbackMessageFixRate& fix_rate =
          frame.fix_rate;
      if (!writer->WriteUInt32(fix_rate.bitrate_in_bytes_per_second)) {
        return false;
      }
      break;
    }
    case kTCP: {
      const CongestionFeedbackMessageTCP& tcp = frame.tcp;
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

bool QuicFramer::AppendRstStreamFramePayload(
        const QuicRstStreamFrame& frame,
        QuicDataWriter* writer) {
  if (!writer->WriteUInt32(frame.stream_id)) {
    return false;
  }
  if (!writer->WriteUInt64(frame.offset)) {
    return false;
  }

  uint32 error_code = static_cast<uint32>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }

  if (!writer->WriteStringPiece16(frame.error_details)) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendConnectionCloseFramePayload(
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  uint32 error_code = static_cast<uint32>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }
  if (!writer->WriteStringPiece16(frame.error_details)) {
    return false;
  }
  AppendAckFramePayload(frame.ack_frame, writer);
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
