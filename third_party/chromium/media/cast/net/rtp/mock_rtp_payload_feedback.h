// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_MOCK_RTP_PAYLOAD_FEEDBACK_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_MOCK_RTP_PAYLOAD_FEEDBACK_H_

#include "third_party/chromium/media/cast/net/rtp/rtp_defines.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_m96 {
namespace cast {

class MockRtpPayloadFeedback : public RtpPayloadFeedback {
 public:
  MockRtpPayloadFeedback();
  ~MockRtpPayloadFeedback() override;

  MOCK_METHOD1(CastFeedback, void(const RtcpCastMessage& cast_feedback));
};

}  // namespace cast
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_RTP_MOCK_RTP_PAYLOAD_FEEDBACK_H_
