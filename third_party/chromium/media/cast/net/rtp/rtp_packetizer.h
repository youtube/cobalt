// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_RTP_PACKETIZER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_RTP_PACKETIZER_H_

#include <stddef.h>
#include <stdint.h>

#include <cmath>

#include "third_party/chromium/media/cast/common/rtp_time.h"
#include "third_party/chromium/media/cast/net/rtp/packet_storage.h"

namespace media_m96 {
namespace cast {

class PacedSender;

struct RtpPacketizerConfig {
  RtpPacketizerConfig();
  ~RtpPacketizerConfig();

  // General.
  int payload_type;
  uint16_t max_payload_length;
  uint16_t sequence_number;

  // SSRC.
  unsigned int ssrc;
};

// This object is only called from the main cast thread.
// This class break encoded audio and video frames into packets and add an RTP
// header to each packet.
class RtpPacketizer {
 public:
  RtpPacketizer(PacedSender* const transport,
                PacketStorage* packet_storage,
                RtpPacketizerConfig rtp_packetizer_config);
  ~RtpPacketizer();

  void SendFrameAsPackets(const EncodedFrame& frame);

  // Return the next sequence number, and increment by one. Enables unique
  // incremental sequence numbers for every packet (including retransmissions).
  uint16_t NextSequenceNumber();

  size_t send_packet_count() const { return send_packet_count_; }
  size_t send_octet_count() const { return send_octet_count_; }

 private:
  void BuildCommonRTPheader(Packet* packet,
                            bool marker_bit,
                            RtpTimeTicks rtp_timestamp);

  RtpPacketizerConfig config_;
  PacedSender* const transport_;  // Not owned by this class.
  PacketStorage* packet_storage_;

  uint16_t sequence_number_;

  size_t send_packet_count_;
  size_t send_octet_count_;
};

}  // namespace cast
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_RTP_PACKETIZER_H_
