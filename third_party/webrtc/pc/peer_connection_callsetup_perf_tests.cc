/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/data_channel_interface.h"
#include "api/field_trials.h"
#include "api/field_trials_view.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/metrics/metric.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "p2p/base/transport_description.h"
#include "pc/sdp_utils.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "pc/test/peer_connection_test_wrapper.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

using ::testing::IsTrue;
using ::testing::Values;

using ::webrtc::test::GetGlobalMetricsLogger;
using ::webrtc::test::ImprovementDirection;
using ::webrtc::test::Unit;
namespace webrtc {

// All tests in this file require SCTP support.
#ifdef WEBRTC_HAVE_SCTP

class PeerConnectionDataChannelOpenTest
    : public ::testing::TestWithParam<
          std::tuple</*field_trials=*/std::string,
                     /*signal_candidates_from_client=*/bool,
                     /*dtls_role=*/ConnectionRole>> {
 public:
  PeerConnectionDataChannelOpenTest()
      : background_thread_(std::make_unique<Thread>(&vss_)) {
    RTC_CHECK(background_thread_->Start());
    // Delay is set to 50ms so we get a 100ms RTT.
    vss_.set_delay_mean(/*delay_ms=*/50);
    vss_.UpdateDelayDistribution();
  }

  scoped_refptr<PeerConnectionTestWrapper> CreatePc(
      std::unique_ptr<FieldTrialsView> field_trials = nullptr) {
    auto pc_wrapper = make_ref_counted<PeerConnectionTestWrapper>(
        "pc", &vss_, background_thread_.get(), background_thread_.get());
    pc_wrapper->CreatePc({}, CreateBuiltinAudioEncoderFactory(),
                         CreateBuiltinAudioDecoderFactory(),
                         std::move(field_trials));
    return pc_wrapper;
  }

  void SignalIceCandidates(
      scoped_refptr<PeerConnectionTestWrapper> from_pc_wrapper,
      scoped_refptr<PeerConnectionTestWrapper> to_pc_wrapper) {
    from_pc_wrapper->SignalOnIceCandidateReady.connect(
        to_pc_wrapper.get(), &PeerConnectionTestWrapper::AddIceCandidate);
  }

  void Negotiate(scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper,
                 scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper,
                 ConnectionRole remote_role) {
    std::unique_ptr<SessionDescriptionInterface> offer =
        CreateOffer(local_pc_wrapper);
    scoped_refptr<MockSetSessionDescriptionObserver> p1 =
        SetLocalDescription(local_pc_wrapper, offer.get());
    std::unique_ptr<SessionDescriptionInterface> modified_offer =
        offer->Clone();
    // Modify offer role to get desired remote role.
    if (remote_role == CONNECTIONROLE_PASSIVE) {
      auto& transport_infos = modified_offer->description()->transport_infos();
      ASSERT_TRUE(!transport_infos.empty());
      transport_infos[0].description.connection_role = CONNECTIONROLE_ACTIVE;
    }
    scoped_refptr<MockSetSessionDescriptionObserver> p2 =
        SetRemoteDescription(remote_pc_wrapper, modified_offer.get());
    EXPECT_TRUE(Await({p1, p2}));
    std::unique_ptr<SessionDescriptionInterface> answer =
        CreateAnswer(remote_pc_wrapper);
    p1 = SetLocalDescription(remote_pc_wrapper, answer.get());
    p2 = SetRemoteDescription(local_pc_wrapper, answer.get());
    EXPECT_TRUE(Await({p1, p2}));
  }

  bool WaitForDataChannelOpen(scoped_refptr<webrtc::DataChannelInterface> dc) {
    return WaitUntil(
               [&] {
                 return dc->state() == DataChannelInterface::DataState::kOpen;
               },
               IsTrue(), {.timeout = webrtc::TimeDelta::Millis(5000)})
        .ok();
  }

 protected:
  std::unique_ptr<SessionDescriptionInterface> CreateOffer(
      scoped_refptr<PeerConnectionTestWrapper> pc_wrapper) {
    auto observer = make_ref_counted<MockCreateSessionDescriptionObserver>();
    pc_wrapper->pc()->CreateOffer(observer.get(), {});
    EXPECT_THAT(WaitUntil([&] { return observer->called(); }, IsTrue()),
                IsRtcOk());
    return observer->MoveDescription();
  }

  std::unique_ptr<SessionDescriptionInterface> CreateAnswer(
      scoped_refptr<PeerConnectionTestWrapper> pc_wrapper) {
    auto observer = make_ref_counted<MockCreateSessionDescriptionObserver>();
    pc_wrapper->pc()->CreateAnswer(observer.get(), {});
    EXPECT_THAT(WaitUntil([&] { return observer->called(); }, IsTrue()),
                IsRtcOk());
    return observer->MoveDescription();
  }

  scoped_refptr<MockSetSessionDescriptionObserver> SetLocalDescription(
      scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      SessionDescriptionInterface* sdp) {
    auto observer = make_ref_counted<MockSetSessionDescriptionObserver>();
    pc_wrapper->pc()->SetLocalDescription(
        observer.get(), CloneSessionDescription(sdp).release());
    return observer;
  }

  scoped_refptr<MockSetSessionDescriptionObserver> SetRemoteDescription(
      scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      SessionDescriptionInterface* sdp) {
    auto observer = make_ref_counted<MockSetSessionDescriptionObserver>();
    pc_wrapper->pc()->SetRemoteDescription(
        observer.get(), CloneSessionDescription(sdp).release());
    return observer;
  }

  // To avoid ICE candidates arriving before the remote endpoint has received
  // the offer it is important to SetLocalDescription() and
  // SetRemoteDescription() are kicked off without awaiting in-between. This
  // helper is used to await multiple observers.
  bool Await(
      std::vector<scoped_refptr<MockSetSessionDescriptionObserver>> observers) {
    for (auto& observer : observers) {
      auto result = WaitUntil([&] { return observer->called(); }, IsTrue());

      if (!result.ok() || !observer->result()) {
        return false;
      }
    }
    return true;
  }

  VirtualSocketServer vss_;
  std::unique_ptr<Thread> background_thread_;
};

TEST_P(PeerConnectionDataChannelOpenTest, OpenAtCaller) {
  std::string trials = std::get<0>(GetParam());
  bool skip_candidates_from_caller = std::get<1>(GetParam());
  ConnectionRole role = std::get<2>(GetParam());
  std::string role_string;
  ASSERT_TRUE(ConnectionRoleToString(role, &role_string));

  scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper =
      CreatePc(FieldTrials::CreateNoGlobal(trials));
  scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper =
      CreatePc(FieldTrials::CreateNoGlobal(trials));

  if (!skip_candidates_from_caller) {
    SignalIceCandidates(local_pc_wrapper, remote_pc_wrapper);
  }
  SignalIceCandidates(remote_pc_wrapper, local_pc_wrapper);

  auto dc = local_pc_wrapper->CreateDataChannel("test", {});
  Negotiate(local_pc_wrapper, remote_pc_wrapper, role);
  uint64_t start_time = TimeNanos();
  EXPECT_TRUE(WaitForDataChannelOpen(dc));
  uint64_t open_time = TimeNanos();
  uint64_t setup_time = open_time - start_time;

  double setup_time_millis = setup_time / kNumNanosecsPerMillisec;
  std::string test_description =
      "emulate_server=" + absl::StrCat(skip_candidates_from_caller) +
      "/dtls_role=" + role_string + "/trials=" + trials;
  GetGlobalMetricsLogger()->LogSingleValueMetric(
      "TimeToOpenDataChannel", test_description, setup_time_millis,
      Unit::kMilliseconds, ImprovementDirection::kSmallerIsBetter);
}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionDataChannelOpenTest,
    PeerConnectionDataChannelOpenTest,
    ::testing::Combine(
        testing::Values(  // Field trials to use.
                          // WebRTC 1.0 + DTLS 1.2
            "WebRTC-IceHandshakeDtls/Disabled/WebRTC-ForceDtls13/"
            "Disabled/",
            // SPED + DTLS 1.2
            "WebRTC-IceHandshakeDtls/Enabled/WebRTC-ForceDtls13/"
            "Disabled/",
            // WebRTC 1.0 + DTLS 1.3
            "WebRTC-IceHandshakeDtls/Disabled/WebRTC-ForceDtls13/"
            "Enabled/",
            // SPED + DTLS 1.3
            "WebRTC-IceHandshakeDtls/Enabled/WebRTC-ForceDtls13/"
            "Enabled/"),
        testing::Bool(),  // Whether to skip signaling candidates from
                          // first connection.
        testing::Values(
            // Default, other side will send
            // the DTLS handshake.
            CONNECTIONROLE_ACTIVE,
            // Local side will send the DTLS
            // handshake.
            CONNECTIONROLE_PASSIVE)));

#endif  // WEBRTC_HAVE_SCTP

}  // namespace webrtc
