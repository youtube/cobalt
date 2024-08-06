// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_MOCK_RTP_FEEDBACK_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_MOCK_RTP_FEEDBACK_H_

#include <stdint.h>

#include "third_party/chromium/media/cast/net/rtp/rtp_parser/rtp_feedback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_m96 {
namespace cast {

class MockRtpFeedback : public RtpFeedback {
 public:
  MOCK_METHOD4(OnInitializeDecoder,
               int32_t(const int8_t payloadType,
                       const int frequency,
                       const uint8_t channels,
                       const uint32_t rate));

  MOCK_METHOD1(OnPacketTimeout, void(const int32_t id));
  MOCK_METHOD2(OnReceivedPacket,
               void(const int32_t id, const RtpRtcpPacketField packet_type));
  MOCK_METHOD2(OnPeriodicDeadOrAlive,
               void(const int32_t id, const RTPAliveType alive));
  MOCK_METHOD2(OnIncomingSSRCChanged,
               void(const int32_t id, const uint32_t ssrc));
  MOCK_METHOD3(OnIncomingCSRCChanged,
               void(const int32_t id, const uint32_t csrc, const bool added));
};

}  // namespace cast
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_MOCK_RTP_FEEDBACK_H_
