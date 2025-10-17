/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests that verify that congestion control options
// are correctly negotiated in the SDP offer/answer.

#include <string>

#include "absl/strings/str_cat.h"
#include "api/peer_connection_interface.h"
#include "api/test/rtc_error_matchers.h"
#include "pc/test/integration_test_helpers.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

using testing::Eq;
using ::testing::Gt;
using testing::HasSubstr;
using ::testing::IsTrue;
using testing::Not;

class PeerConnectionCongestionControlTest
    : public PeerConnectionIntegrationBaseTest {
 public:
  PeerConnectionCongestionControlTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}
};

TEST_F(PeerConnectionCongestionControlTest, OfferContainsCcfbIfEnabled) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  caller()->AddAudioVideoTracks();
  auto offer = caller()->CreateOfferAndWait();
  std::string offer_str = absl::StrCat(*offer);
  EXPECT_THAT(offer_str, HasSubstr("a=rtcp-fb:* ack ccfb\r\n"));
}

TEST_F(PeerConnectionCongestionControlTest, ReceiveOfferSetsCcfbFlag) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  {
    // Check that the callee parsed it.
    auto parsed_contents =
        callee()->pc()->remote_description()->description()->contents();
    EXPECT_FALSE(parsed_contents.empty());
    for (const auto& content : parsed_contents) {
      EXPECT_TRUE(content.media_description()->rtcp_fb_ack_ccfb());
    }
  }

  {
    // Check that the caller also parsed it.
    auto parsed_contents =
        caller()->pc()->remote_description()->description()->contents();
    EXPECT_FALSE(parsed_contents.empty());
    for (const auto& content : parsed_contents) {
      EXPECT_TRUE(content.media_description()->rtcp_fb_ack_ccfb());
    }
  }
  // Check that the answer does not contain transport-cc
  std::string answer_str = absl::StrCat(*caller()->pc()->remote_description());
  EXPECT_THAT(answer_str, Not(HasSubstr("transport-cc")));
}

TEST_F(PeerConnectionCongestionControlTest, CcfbGetsUsed) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  auto pc_internal = caller()->pc_internal();
  EXPECT_THAT(
      WaitUntil(
          [&] {
            return pc_internal->FeedbackAccordingToRfc8888CountForTesting();
          },
          Gt(0)),
      IsRtcOk());
  // There should be no transport-cc generated.
  EXPECT_THAT(pc_internal->FeedbackAccordingToTransportCcCountForTesting(),
              Eq(0));
}

TEST_F(PeerConnectionCongestionControlTest, TransportCcGetsUsed) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Disabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  auto pc_internal = caller()->pc_internal();
  EXPECT_THAT(
      WaitUntil(
          [&] {
            return pc_internal->FeedbackAccordingToTransportCcCountForTesting();
          },
          Gt(0)),
      IsRtcOk());
  // Test that RFC 8888 feedback is NOT generated when field trial disabled.
  EXPECT_THAT(pc_internal->FeedbackAccordingToRfc8888CountForTesting(), Eq(0));
}

}  // namespace webrtc
