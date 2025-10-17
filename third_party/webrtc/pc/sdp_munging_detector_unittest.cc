/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/field_trials.h"
#include "api/field_trials_view.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "api/uma_metrics.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "media/base/stream_params.h"
#include "p2p/base/transport_description.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_rtc_certificate_generator.h"
#include "pc/test/integration_test_helpers.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/strings/string_format.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

// This file contains unit tests that relate to the behavior of the
// SDP munging detector module.
// Tests are written as integration tests with PeerConnection, since the
// behaviors are still linked so closely that it is hard to test them in
// isolation.

namespace webrtc {

using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Pair;

namespace {

std::unique_ptr<Thread> CreateAndStartThread() {
  auto thread = Thread::Create();
  thread->Start();
  return thread;
}

}  // namespace

class SdpMungingTest : public ::testing::Test {
 public:
  SdpMungingTest()
      // Note: We use a PeerConnectionFactory with a distinct
      // signaling thread, so that thread handling can be tested.
      : signaling_thread_(CreateAndStartThread()),
        pc_factory_(CreatePeerConnectionFactory(
            nullptr,
            nullptr,
            signaling_thread_.get(),
            FakeAudioCaptureModule::Create(),
            CreateBuiltinAudioEncoderFactory(),
            CreateBuiltinAudioDecoderFactory(),
            std::make_unique<
                VideoEncoderFactoryTemplate<LibvpxVp8EncoderTemplateAdapter,
                                            LibvpxVp9EncoderTemplateAdapter,
                                            OpenH264EncoderTemplateAdapter,
                                            LibaomAv1EncoderTemplateAdapter>>(),
            std::make_unique<
                VideoDecoderFactoryTemplate<LibvpxVp8DecoderTemplateAdapter,
                                            LibvpxVp9DecoderTemplateAdapter,
                                            OpenH264DecoderTemplateAdapter,
                                            Dav1dDecoderTemplateAdapter>>(),
            nullptr /* audio_mixer */,
            nullptr /* audio_processing */,
            nullptr /* audio_frame_processor */)) {
    metrics::Reset();
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection(
      std::unique_ptr<FieldTrialsView> field_trials = nullptr) {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(config, std::move(field_trials));
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection(
      const RTCConfiguration& config,
      std::unique_ptr<FieldTrialsView> field_trials) {
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    PeerConnectionDependencies pc_deps(observer.get());
    pc_deps.trials = std::move(field_trials);
    auto result =
        pc_factory_->CreatePeerConnectionOrError(config, std::move(pc_deps));
    EXPECT_TRUE(result.ok());
    observer->SetPeerConnectionInterface(result.value().get());
    return std::make_unique<PeerConnectionWrapper>(
        pc_factory_, result.MoveValue(), std::move(observer));
  }

 protected:
  std::unique_ptr<Thread> signaling_thread_;
  scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;

 private:
  AutoThread main_thread_;
};

TEST_F(SdpMungingTest, DISABLED_ReportUMAMetricsWithNoMunging) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  caller->AddTransceiver(MediaType::AUDIO);
  caller->AddTransceiver(MediaType::VIDEO);

  // Negotiate, gather candidates, then exchange ICE candidates.
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kNoModification, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Answer.Initial"),
      ElementsAre(Pair(SdpMungingType::kNoModification, 1)));

  EXPECT_THAT(WaitUntil([&] { return caller->IsIceGatheringDone(); }, IsTrue(),
                        {.timeout = kDefaultTimeout}),
              IsRtcOk());
  EXPECT_THAT(WaitUntil([&] { return callee->IsIceGatheringDone(); }, IsTrue(),
                        {.timeout = kDefaultTimeout}),
              IsRtcOk());
  for (const auto& candidate : caller->observer()->GetAllCandidates()) {
    callee->pc()->AddIceCandidate(candidate);
  }
  for (const auto& candidate : callee->observer()->GetAllCandidates()) {
    caller->pc()->AddIceCandidate(candidate);
  }
  EXPECT_THAT(
      WaitUntil([&] { return caller->pc()->peer_connection_state(); },
                Eq(PeerConnectionInterface::PeerConnectionState::kConnected),
                {.timeout = kDefaultTimeout}),
      IsRtcOk());
  EXPECT_THAT(
      WaitUntil([&] { return callee->pc()->peer_connection_state(); },
                Eq(PeerConnectionInterface::PeerConnectionState::kConnected),
                {.timeout = kDefaultTimeout}),
      IsRtcOk());

  caller->pc()->Close();
  callee->pc()->Close();

  EXPECT_THAT(
      metrics::Samples(
          "WebRTC.PeerConnection.SdpMunging.Offer.ConnectionEstablished"),
      ElementsAre(Pair(SdpMungingType::kNoModification, 1)));
  EXPECT_THAT(
      metrics::Samples(
          "WebRTC.PeerConnection.SdpMunging.Answer.ConnectionEstablished"),
      ElementsAre(Pair(SdpMungingType::kNoModification, 1)));

  EXPECT_THAT(metrics::Samples(
                  "WebRTC.PeerConnection.SdpMunging.Offer.ConnectionClosed"),
              ElementsAre(Pair(SdpMungingType::kNoModification, 1)));
  EXPECT_THAT(metrics::Samples(
                  "WebRTC.PeerConnection.SdpMunging.Answer.ConnectionClosed"),
              ElementsAre(Pair(SdpMungingType::kNoModification, 1)));
}

TEST_F(SdpMungingTest, InitialSetLocalDescriptionWithoutCreateOffer) {
  RTCConfiguration config;
  config.certificates.push_back(
      FakeRTCCertificateGenerator::GenerateCertificate());
  auto pc = CreatePeerConnection(config, nullptr);
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=fingerprint:sha-1 "
      "D9:AB:00:AA:12:7B:62:54:CF:AD:3B:55:F7:60:BC:F3:40:A7:0B:5B\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n";
  auto offer = CreateSessionDescription(SdpType::kOffer, sdp);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kWithoutCreateOffer, 1)));
}

TEST_F(SdpMungingTest, InitialSetLocalDescriptionWithoutCreateAnswer) {
  RTCConfiguration config;
  config.certificates.push_back(
      FakeRTCCertificateGenerator::GenerateCertificate());
  auto pc = CreatePeerConnection(config, nullptr);
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=fingerprint:sha-1 "
      "D9:AB:00:AA:12:7B:62:54:CF:AD:3B:55:F7:60:BC:F3:40:A7:0B:5B\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendrecv\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:111 opus/48000/2\r\n";
  auto offer = CreateSessionDescription(SdpType::kOffer, sdp);
  EXPECT_TRUE(pc->SetRemoteDescription(std::move(offer)));

  RTCError error;
  auto answer = CreateSessionDescription(SdpType::kAnswer, sdp);
  answer->description()->transport_infos()[0].description.connection_role =
      CONNECTIONROLE_ACTIVE;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(answer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Answer.Initial"),
      ElementsAre(Pair(SdpMungingType::kWithoutCreateAnswer, 1)));
}

TEST_F(SdpMungingTest, IceUfrag) {
  auto pc = CreatePeerConnection(
      FieldTrials::CreateNoGlobal("WebRTC-NoSdpMangleUfrag/Enabled/"));
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.ice_ufrag =
      "amungediceufragthisshouldberejected";
  RTCError error;
  // Ufrag is rejected.
  EXPECT_FALSE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kIceUfrag, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.SdpOutcome.Rejected"),
      ElementsAre(Pair(SdpMungingType::kIceUfrag, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Outcome"),
      ElementsAre(Pair(static_cast<int>(SdpMungingOutcome::kRejected), 1)));
}

TEST_F(SdpMungingTest, IceUfragCheckDisabledByFieldTrial) {
  auto pc = CreatePeerConnection(
      FieldTrials::CreateNoGlobal("WebRTC-NoSdpMangleUfrag/Disabled/"));
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.ice_ufrag =
      "amungediceufragthisshouldberejected";
  RTCError error;
  // Ufrag is not rejected.
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kIceUfrag, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.SdpOutcome.Accepted"),
      ElementsAre(Pair(SdpMungingType::kIceUfrag, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Outcome"),
      ElementsAre(Pair(static_cast<int>(SdpMungingOutcome::kAccepted), 1)));
}

TEST_F(SdpMungingTest, IceUfragWithCheckDisabledForTesting) {
  auto pc = CreatePeerConnection();
  pc->GetInternalPeerConnection()->DisableSdpMungingChecksForTesting();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.ice_ufrag =
      "amungediceufragthisshouldberejected";
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kIceUfrag, 1)));
}

TEST_F(SdpMungingTest, IcePwdCheckDisabledByFieldTrial) {
  auto pc = CreatePeerConnection(
      FieldTrials::CreateNoGlobal("WebRTC-NoSdpMangleUfrag/Disabled/"));
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.ice_pwd = "amungedicepwdthisshouldberejected";
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kIcePwd, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.SdpOutcome.Accepted"),
      ElementsAre(Pair(SdpMungingType::kIcePwd, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Outcome"),
      ElementsAre(Pair(static_cast<int>(SdpMungingOutcome::kAccepted), 1)));
}

TEST_F(SdpMungingTest, IcePwd) {
  auto pc = CreatePeerConnection(
      FieldTrials::CreateNoGlobal("WebRTC-NoSdpMangleUfrag/Enabled/"));
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.ice_pwd = "amungedicepwdthisshouldberejected";
  RTCError error;
  EXPECT_FALSE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kIcePwd, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.SdpOutcome.Rejected"),
      ElementsAre(Pair(SdpMungingType::kIcePwd, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Outcome"),
      ElementsAre(Pair(static_cast<int>(SdpMungingOutcome::kRejected), 1)));
}

TEST_F(SdpMungingTest, IceUfragRestrictedAddresses) {
  RTCConfiguration config;
  config.certificates.push_back(
      FakeRTCCertificateGenerator::GenerateCertificate());
  auto caller = CreatePeerConnection(
      config,
      FieldTrials::CreateNoGlobal("WebRTC-NoSdpMangleUfragRestrictedAddresses/"
                                  "127.0.0.1:12345|127.0.0.*:23456|*:34567/"));
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack("audio_track", {});
  auto offer = caller->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.ice_ufrag = "amungediceufrag";

  EXPECT_TRUE(caller->SetLocalDescription(offer->Clone()));
  EXPECT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  EXPECT_TRUE(callee->SetLocalDescription(answer->Clone()));
  EXPECT_TRUE(caller->SetRemoteDescription(std::move(answer)));

  static constexpr const char tmpl[] =
      "candidate:a0+B/1 1 udp 2130706432 %s typ host";

  // Addresses to test. First field is the address in string format,
  // second field is the expected outcome (success or failure).
  const std::vector<std::pair<const char*, bool>> address_tests = {
      {"127.0.0.1:12345", false}, {"127.0.0.2:23456", false},
      {"8.8.8.8:34567", false},   {"127.0.0.2:12345", true},
      {"127.0.1.1:23456", true},  {"8.8.8.8:3456", true},
  };

  int num_blocked = 0;
  for (const auto& address_test : address_tests) {
    std::optional<RTCError> result;
    const std::string candidate = StringFormat(
        tmpl, absl::StrReplaceAll(address_test.first, {{":", " "}}).c_str());
    caller->pc()->AddIceCandidate(
        std::unique_ptr<IceCandidateInterface>(
            CreateIceCandidate("", 0, candidate, nullptr)),
        [&result](RTCError error) { result = error; });

    ASSERT_THAT(
        WaitUntil([&] { return result.has_value(); }, ::testing::IsTrue()),
        IsRtcOk());
    if (address_test.second == true) {
      EXPECT_TRUE(result.value().ok());
    } else {
      std::pair<absl::string_view, absl::string_view> host =
          absl::StrSplit(address_test.first, ":");
      int port;
      ASSERT_TRUE(absl::SimpleAtoi(host.second, &port));
      EXPECT_FALSE(result.value().ok());
      EXPECT_EQ(result.value().type(), RTCErrorType::UNSUPPORTED_OPERATION);
      num_blocked++;
      EXPECT_THAT(
          metrics::Samples(
              "WebRTC.PeerConnection.RestrictedCandidates.SdpMungingType"),
          ElementsAre(Pair(SdpMungingType::kIceUfrag, num_blocked)));
      EXPECT_THAT(
          metrics::Samples("WebRTC.PeerConnection.RestrictedCandidates.Port"),
          Contains(Pair(port, 1)));
    }
  }
}

TEST_F(SdpMungingTest, IceUfragSdpRejectedAndRestrictedAddresses) {
  RTCConfiguration config;
  config.certificates.push_back(
      FakeRTCCertificateGenerator::GenerateCertificate());
  auto caller = CreatePeerConnection(
      config,
      FieldTrials::CreateNoGlobal("WebRTC-NoSdpMangleUfragRestrictedAddresses/"
                                  "127.0.0.1:12345|127.0.0.*:23456|*:34567/"
                                  "WebRTC-NoSdpMangleUfrag/Enabled/"));
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack("audio_track", {});
  auto offer = caller->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.ice_ufrag = "amungediceufrag";

  EXPECT_FALSE(caller->SetLocalDescription(offer->Clone()));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kIceUfrag, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.SdpOutcome.Rejected"),
      ElementsAre(Pair(SdpMungingType::kIceUfrag, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Outcome"),
      ElementsAre(Pair(static_cast<int>(SdpMungingOutcome::kRejected), 1)));
}

TEST_F(SdpMungingTest, IceMode) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.ice_mode = ICEMODE_LITE;
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kIceMode, 1)));
}

TEST_F(SdpMungingTest, IceOptions) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.transport_options.push_back(
      "something-unsupported");
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kIceOptions, 1)));
}

TEST_F(SdpMungingTest, IceOptionsRenomination) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.transport_options.push_back(
      ICE_OPTION_RENOMINATION);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kIceOptionsRenomination, 1)));
}

TEST_F(SdpMungingTest, DtlsRole) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].description.connection_role = CONNECTIONROLE_PASSIVE;
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kDtlsSetup, 1)));
}

TEST_F(SdpMungingTest, RemoveContentDefault) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto name = contents[0].mid();
  EXPECT_TRUE(offer->description()->RemoveContentByName(contents[0].mid()));
  std::string sdp;
  offer->ToString(&sdp);
  auto modified_offer = CreateSessionDescription(
      SdpType::kOffer,
      absl::StrReplaceAll(sdp, {{"a=group:BUNDLE " + name, "a=group:BUNDLE"}}));

  RTCError error;
  EXPECT_FALSE(pc->SetLocalDescription(std::move(modified_offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kNumberOfContents, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.SdpOutcome.Rejected"),
      ElementsAre(Pair(SdpMungingType::kNumberOfContents, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Outcome"),
      ElementsAre(Pair(static_cast<int>(SdpMungingOutcome::kRejected), 1)));
}

TEST_F(SdpMungingTest, RemoveContentKillswitch) {
  auto pc = CreatePeerConnection(FieldTrials::CreateNoGlobal(
      "WebRTC-NoSdpMangleNumberOfContents/Disabled/"));
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto name = contents[0].mid();
  EXPECT_TRUE(offer->description()->RemoveContentByName(contents[0].mid()));
  std::string sdp;
  offer->ToString(&sdp);
  auto modified_offer = CreateSessionDescription(
      SdpType::kOffer,
      absl::StrReplaceAll(sdp, {{"a=group:BUNDLE " + name, "a=group:BUNDLE"}}));

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(modified_offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kNumberOfContents, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.SdpOutcome.Accepted"),
      ElementsAre(Pair(SdpMungingType::kNumberOfContents, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Outcome"),
      ElementsAre(Pair(static_cast<int>(SdpMungingOutcome::kAccepted), 1)));
}

TEST_F(SdpMungingTest, TransceiverDirection) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();

  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto direction = media_description->direction();
  if (direction == RtpTransceiverDirection::kInactive) {
    media_description->set_direction(RtpTransceiverDirection::kSendRecv);
  } else {
    media_description->set_direction(RtpTransceiverDirection::kInactive);
  }
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kDirection, 1)));
}

TEST_F(SdpMungingTest, Mid) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  std::string name(contents[0].mid());
  contents[0].set_mid("amungedmid");

  auto& transport_infos = offer->description()->transport_infos();
  ASSERT_EQ(transport_infos.size(), 1u);
  transport_infos[0].content_name = "amungedmid";
  std::string sdp;
  offer->ToString(&sdp);
  auto modified_offer = CreateSessionDescription(
      SdpType::kOffer,
      absl::StrReplaceAll(
          sdp, {{"a=group:BUNDLE " + name, "a=group:BUNDLE amungedmid"}}));

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(modified_offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kMid, 1)));
}

TEST_F(SdpMungingTest, LegacySimulcast) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  uint32_t ssrc = media_description->first_ssrc();
  ASSERT_EQ(media_description->streams().size(), 1u);
  const std::string& cname = media_description->streams()[0].cname;

  std::string sdp;
  offer->ToString(&sdp);
  sdp += "a=ssrc-group:SIM " + absl::StrCat(ssrc) + " " +
         absl::StrCat(ssrc + 1) + "\r\n" +  //
         "a=ssrc-group:FID " + absl::StrCat(ssrc + 1) + " " +
         absl::StrCat(ssrc + 2) + "\r\n" +                                  //
         "a=ssrc:" + absl::StrCat(ssrc + 1) + " msid:- video_track\r\n" +   //
         "a=ssrc:" + absl::StrCat(ssrc + 1) + " cname:" + cname + "\r\n" +  //
         "a=ssrc:" + absl::StrCat(ssrc + 2) + " msid:- video_track\r\n" +   //
         "a=ssrc:" + absl::StrCat(ssrc + 2) + " cname:" + cname + "\r\n";
  auto modified_offer = CreateSessionDescription(SdpType::kOffer, sdp);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(modified_offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kVideoCodecsLegacySimulcast, 1)));
}

#ifdef WEBRTC_USE_H264
TEST_F(SdpMungingTest, H264SpsPpsIdrInKeyFrame) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  for (auto& codec : codecs) {
    if (codec.name == webrtc::kH264CodecName) {
      codec.SetParam(webrtc::kH264FmtpSpsPpsIdrInKeyframe,
                     webrtc::kParamValueTrue);
    }
  }
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(
          Pair(SdpMungingType::kVideoCodecsFmtpH264SpsPpsIdrInKeyframe, 1)));
}
#endif  // WEBRTC_USE_H264

TEST_F(SdpMungingTest, OpusStereo) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  for (auto& codec : codecs) {
    if (codec.name == kOpusCodecName) {
      codec.SetParam(kCodecParamStereo, kParamValueTrue);
    }
  }
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsFmtpOpusStereo, 1)));
}

TEST_F(SdpMungingTest, OpusFec) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  for (auto& codec : codecs) {
    if (codec.name == kOpusCodecName) {
      // Enabled by default so we need to remove the parameter.
      EXPECT_TRUE(codec.RemoveParam(kCodecParamUseInbandFec));
    }
  }
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsFmtpOpusFec, 1)));
}

TEST_F(SdpMungingTest, OpusDtx) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  for (auto& codec : codecs) {
    if (codec.name == kOpusCodecName) {
      codec.SetParam(kCodecParamUseDtx, kParamValueTrue);
    }
  }
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsFmtpOpusDtx, 1)));
}

TEST_F(SdpMungingTest, OpusCbr) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  for (auto& codec : codecs) {
    if (codec.name == kOpusCodecName) {
      codec.SetParam(kCodecParamCbr, kParamValueTrue);
    }
  }
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsFmtpOpusCbr, 1)));
}

TEST_F(SdpMungingTest, AudioCodecsRemoved) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  codecs.pop_back();
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsRemoved, 1)));
}

TEST_F(SdpMungingTest, AudioCodecsAdded) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  auto codec = CreateAudioCodec(SdpAudioFormat("pcmu", 8000, 1, {}));
  codec.id = 19;  // IANA reserved payload type, should not conflict.
  codecs.push_back(codec);
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsAdded, 1)));
}

TEST_F(SdpMungingTest, VideoCodecsRemoved) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  codecs.pop_back();
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kVideoCodecsRemoved, 1)));
}

TEST_F(SdpMungingTest, VideoCodecsAdded) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  auto codec = CreateVideoCodec(SdpVideoFormat("VP8", {}));
  codec.id = 19;  // IANA reserved payload type, should not conflict.
  codecs.push_back(codec);
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kVideoCodecsAdded, 1)));
}

TEST_F(SdpMungingTest, VideoCodecsAddedWithRawPacketization) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  auto codec = CreateVideoCodec(SdpVideoFormat("VP8", {}));
  codec.id = 19;  // IANA reserved payload type, should not conflict.
  codec.packetization = "raw";
  codecs.push_back(codec);
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(
          Pair(SdpMungingType::kVideoCodecsAddedWithRawPacketization, 1)));
}

TEST_F(SdpMungingTest, VideoCodecsModifiedWithRawPacketization) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  ASSERT_TRUE(!codecs.empty());
  codecs[0].packetization = "raw";
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(
          Pair(SdpMungingType::kVideoCodecsModifiedWithRawPacketization, 1)));
}

TEST_F(SdpMungingTest, MultiOpus) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  auto multiopus =
      CreateAudioCodec(SdpAudioFormat("multiopus", 48000, 4,
                                      {{"channel_mapping", "0,1,2,3"},
                                       {"coupled_streams", "2"},
                                       {"num_streams", "2"}}));
  multiopus.id = 19;  // IANA reserved payload type, should not conflict.
  codecs.push_back(multiopus);
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsAddedMultiOpus, 1)));
}

TEST_F(SdpMungingTest, L16) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  auto l16 = CreateAudioCodec(SdpAudioFormat("L16", 48000, 2, {}));
  l16.id = 19;  // IANA reserved payload type, should not conflict.
  codecs.push_back(l16);
  media_description->set_codecs(codecs);
  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsAddedL16, 1)));
}

TEST_F(SdpMungingTest, AudioSsrc) {
  // Note: same applies to video but is harder to write since one needs to
  // modify the ssrc-group too.
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  ASSERT_EQ(media_description->streams().size(), 1u);
  media_description->mutable_streams()[0].ssrcs[0] = 4404;

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kSsrcs, 1)));
}

TEST_F(SdpMungingTest, HeaderExtensionAdded) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  // VLA is off by default, id=42 should be unused.
  media_description->AddRtpHeaderExtension(
      {RtpExtension::kVideoLayersAllocationUri, 42});

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kRtpHeaderExtensionAdded, 1)));
}

TEST_F(SdpMungingTest, HeaderExtensionRemoved) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  media_description->ClearRtpHeaderExtensions();

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kRtpHeaderExtensionRemoved, 1)));
}

TEST_F(SdpMungingTest, HeaderExtensionModified) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto extensions = media_description->rtp_header_extensions();
  ASSERT_GT(extensions.size(), 0u);
  extensions[0].id = 42;  // id=42 should be unused.
  media_description->set_rtp_header_extensions(extensions);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kRtpHeaderExtensionModified, 1)));
}

TEST_F(SdpMungingTest, PayloadTypeChanged) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto codecs = media_description->codecs();
  ASSERT_GT(codecs.size(), 0u);
  codecs[0].id = 19;  // IANA reserved payload type, should not conflict.
  media_description->set_codecs(codecs);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kPayloadTypes, 1)));
}

TEST_F(SdpMungingTest, AudioCodecsReordered) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto codecs = media_description->codecs();
  ASSERT_GT(codecs.size(), 1u);
  std::swap(codecs[0], codecs[1]);
  media_description->set_codecs(codecs);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsReordered, 1)));
}

TEST_F(SdpMungingTest, VideoCodecsReordered) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto codecs = media_description->codecs();
  ASSERT_GT(codecs.size(), 1u);
  std::swap(codecs[0], codecs[1]);
  media_description->set_codecs(codecs);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kVideoCodecsReordered, 1)));
}

TEST_F(SdpMungingTest, AudioCodecsFmtp) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto codecs = media_description->codecs();
  ASSERT_GT(codecs.size(), 0u);
  codecs[0].params["dont"] = "munge";
  media_description->set_codecs(codecs);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsFmtp, 1)));
}

TEST_F(SdpMungingTest, VideoCodecsFmtp) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto codecs = media_description->codecs();
  ASSERT_GT(codecs.size(), 0u);
  codecs[0].params["dont"] = "munge";
  media_description->set_codecs(codecs);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kVideoCodecsFmtp, 1)));
}

TEST_F(SdpMungingTest, AudioCodecsRtcpFb) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto codecs = media_description->codecs();
  ASSERT_GT(codecs.size(), 0u);
  codecs[0].feedback_params.Add({"dont", "munge"});
  media_description->set_codecs(codecs);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsRtcpFb, 1)));
}

TEST_F(SdpMungingTest, AudioCodecsRtcpFbNack) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto codecs = media_description->codecs();
  ASSERT_GT(codecs.size(), 0u);
  codecs[0].feedback_params.Add(FeedbackParam("nack"));
  media_description->set_codecs(codecs);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsRtcpFbAudioNack, 1)));
}

TEST_F(SdpMungingTest, AudioCodecsRtcpFbRrtr) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto codecs = media_description->codecs();
  ASSERT_GT(codecs.size(), 0u);
  codecs[0].feedback_params.Add(FeedbackParam("rrtr"));
  media_description->set_codecs(codecs);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kAudioCodecsRtcpFbRrtr, 1)));
}

TEST_F(SdpMungingTest, VideoCodecsRtcpFb) {
  auto pc = CreatePeerConnection();
  pc->AddVideoTrack("video_track", {});

  auto offer = pc->CreateOffer();
  auto& contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto codecs = media_description->codecs();
  ASSERT_GT(codecs.size(), 0u);
  codecs[0].feedback_params.Add({"dont", "munge"});
  media_description->set_codecs(codecs);

  RTCError error;
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer), &error));
  EXPECT_THAT(
      metrics::Samples("WebRTC.PeerConnection.SdpMunging.Offer.Initial"),
      ElementsAre(Pair(SdpMungingType::kVideoCodecsRtcpFb, 1)));
}

}  // namespace webrtc
