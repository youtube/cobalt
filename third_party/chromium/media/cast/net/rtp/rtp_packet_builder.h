// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test helper class that builds rtp packets.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_RTP_PACKET_BUILDER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_RTP_PACKET_BUILDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "third_party/chromium/media/cast/net/rtp/rtp_defines.h"

namespace media_m96 {
namespace cast {

// TODO(miu): Consolidate with RtpPacketizer as a single Cast packet
// serialization implementation.
class RtpPacketBuilder {
 public:
  RtpPacketBuilder();
  void SetKeyFrame(bool is_key);
  void SetFrameIds(uint32_t frame_id, uint32_t reference_frame_id);
  void SetPacketId(uint16_t packet_id);
  void SetMaxPacketId(uint16_t max_packet_id);
  void SetTimestamp(uint32_t timestamp);
  void SetSequenceNumber(uint16_t sequence_number);
  void SetMarkerBit(bool marker);
  void SetPayloadType(int payload_type);
  void SetSsrc(uint32_t ssrc);
  void BuildHeader(uint8_t* data, uint32_t data_length);

 private:
  bool is_key_;
  uint32_t frame_id_;
  uint16_t packet_id_;
  uint16_t max_packet_id_;
  uint32_t reference_frame_id_;
  uint32_t timestamp_;
  uint16_t sequence_number_;
  bool marker_;
  int payload_type_;
  uint32_t ssrc_;

  void BuildCastHeader(uint8_t* data, uint32_t data_length);
  void BuildCommonHeader(uint8_t* data, uint32_t data_length);

  DISALLOW_COPY_AND_ASSIGN(RtpPacketBuilder);
};

}  // namespace cast
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_RTP_PACKET_BUILDER_H_
