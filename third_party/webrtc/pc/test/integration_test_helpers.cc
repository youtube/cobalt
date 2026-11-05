/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/test/integration_test_helpers.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/audio/builtin_audio_processing_builder.h"
#include "api/create_modular_peer_connection_factory.h"
#include "api/enable_media_with_defaults.h"
#include "api/environment/environment.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/rtc_error_matchers.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/fake_rtc_event_log_factory.h"
#include "media/base/stream_params.h"
#include "pc/peer_connection_factory.h"
#include "pc/session_description.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/firewall_socket_server.h"
#include "rtc_base/logging.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "system_wrappers/include/metrics.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"
#include "test/wait_until.h"

namespace webrtc {

using testing::NotNull;

PeerConnectionInterface::RTCOfferAnswerOptions IceRestartOfferAnswerOptions() {
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.ice_restart = true;
  return options;
}

void RemoveSsrcsAndMsids(std::unique_ptr<SessionDescriptionInterface>& sdp) {
  for (ContentInfo& content : sdp->description()->contents()) {
    content.media_description()->mutable_streams().clear();
  }
  sdp->description()->set_msid_signaling(0);
}

void RemoveSsrcsAndKeepMsids(
    std::unique_ptr<SessionDescriptionInterface>& sdp) {
  for (ContentInfo& content : sdp->description()->contents()) {
    std::string track_id;
    std::vector<std::string> stream_ids;
    if (!content.media_description()->streams().empty()) {
      const StreamParams& first_stream =
          content.media_description()->streams()[0];
      track_id = first_stream.id;
      stream_ids = first_stream.stream_ids();
    }
    content.media_description()->mutable_streams().clear();
    StreamParams new_stream;
    new_stream.id = track_id;
    new_stream.set_stream_ids(stream_ids);
    content.media_description()->AddStream(new_stream);
  }
}

void SetSdpType(std::unique_ptr<SessionDescriptionInterface>& sdp,
                SdpType sdpType) {
  std::string str;
  sdp->ToString(&str);
  sdp = CreateSessionDescription(sdpType, str);
}

scoped_refptr<MockStatsObserver>
PeerConnectionIntegrationWrapper::OldGetStatsForTrack(
    MediaStreamTrackInterface* track) {
  auto observer = make_ref_counted<MockStatsObserver>();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  EXPECT_TRUE(peer_connection_->GetStats(
      observer.get(), nullptr,
      PeerConnectionInterface::kStatsOutputLevelStandard));
#pragma clang diagnostic pop
  EXPECT_THAT(test_->GetWaiter().Until([&] { return observer->called(); },
                                       ::testing::IsTrue()),
              IsRtcOk());
  return observer;
}

scoped_refptr<const RTCStatsReport>
PeerConnectionIntegrationWrapper::NewGetStats(WaitUntilSettings settings) {
  auto callback = make_ref_counted<MockRTCStatsCollectorCallback>();
  peer_connection_->GetStats(callback.get());
  EXPECT_THAT(test_->GetWaiter(settings).Until(
                  [&] { return callback->called(); }, ::testing::IsTrue()),
              IsRtcOk());
  return callback->report();
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionIntegrationWrapper::CreateOfferAndWait() {
  auto observer = make_ref_counted<MockCreateSessionDescriptionObserver>();
  pc()->CreateOffer(observer.get(), offer_answer_options_);
  EXPECT_TRUE(test_->GetWaiter().Until([&] { return observer->called(); }));
  if (!observer->result()) {
    return nullptr;
  }
  auto description = observer->MoveDescription();
  if (generated_sdp_munger_) {
    generated_sdp_munger_(description);
  }
  return description;
}

bool PeerConnectionIntegrationWrapper::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  auto observer = make_ref_counted<FakeSetRemoteDescriptionObserver>();
  std::string sdp;
  EXPECT_TRUE(desc->ToString(&sdp));
  RTC_LOG(LS_INFO) << debug_name_
                   << ": SetRemoteDescription SDP: type=" << desc->GetType()
                   << " contents=\n"
                   << sdp;
  pc()->SetRemoteDescription(std::move(desc), observer);
  RemoveUnusedVideoRenderers();
  EXPECT_THAT(test_->GetWaiter().Until([&] { return observer->called(); },
                                       ::testing::IsTrue()),
              IsRtcOk());
  auto err = observer->error();
  if (!err.ok()) {
    RTC_LOG(LS_WARNING) << debug_name_
                        << ": SetRemoteDescription error: " << err.message();
  }
  return observer->error().ok();
}

bool PeerConnectionIntegrationWrapper::SetLocalDescriptionAndSendSdpMessage(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  auto observer = make_ref_counted<MockSetSessionDescriptionObserver>();
  RTC_LOG(LS_INFO) << debug_name_ << ": SetLocalDescriptionAndSendSdpMessage";
  SdpType type = desc->GetType();
  std::string sdp;
  EXPECT_TRUE(desc->ToString(&sdp));
  RTC_LOG(LS_INFO) << debug_name_ << ": local SDP type=" << desc->GetType()
                   << " contents=\n"
                   << sdp;
  pc()->SetLocalDescription(observer.get(), desc.release());
  RemoveUnusedVideoRenderers();
  SendSdpMessage(type, sdp);
  EXPECT_THAT(test_->GetWaiter().Until([&] { return observer->called(); },
                                       ::testing::IsTrue()),
              IsRtcOk());
  return true;
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionIntegrationWrapper::CreateAnswer() {
  auto observer = make_ref_counted<MockCreateSessionDescriptionObserver>();
  pc()->CreateAnswer(observer.get(), offer_answer_options_);
  EXPECT_THAT(test_->GetWaiter().Until([&] { return observer->called(); },
                                       ::testing::IsTrue()),
              IsRtcOk());
  if (!observer->result()) {
    return nullptr;
  }
  auto description = observer->MoveDescription();
  if (generated_sdp_munger_) {
    generated_sdp_munger_(description);
  }
  return description;
}

void PeerConnectionIntegrationWrapper::ReceiveIceMessage(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& msg) {
  RTC_LOG(LS_INFO) << debug_name_ << ": ReceiveIceMessage";
  std::optional<RTCError> result;
  pc()->AddIceCandidate(absl::WrapUnique(CreateIceCandidate(
                            sdp_mid, sdp_mline_index, msg, nullptr)),
                        [&result](RTCError r) { result = r; });
  EXPECT_THAT(test_->GetWaiter().Until([&] { return result.has_value(); },
                                       ::testing::IsTrue()),
              IsRtcOk());
  EXPECT_TRUE(result.value().ok());
}

int FindFirstMediaStatsIndexByKind(
    const std::string& kind,
    const std::vector<const RTCInboundRtpStreamStats*>& inbound_rtps) {
  for (size_t i = 0; i < inbound_rtps.size(); i++) {
    if (*inbound_rtps[i]->kind == kind) {
      return i;
    }
  }
  return -1;
}

void ReplaceFirstSsrc(StreamParams& stream, uint32_t ssrc) {
  stream.ssrcs[0] = ssrc;
  for (auto& group : stream.ssrc_groups) {
    group.ssrcs[0] = ssrc;
  }
}

TaskQueueMetronome::TaskQueueMetronome(TimeDelta tick_period)
    : tick_period_(tick_period) {}

TaskQueueMetronome::~TaskQueueMetronome() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}
void TaskQueueMetronome::RequestCallOnNextTick(
    absl::AnyInvocable<void() &&> callback) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  callbacks_.push_back(std::move(callback));
  // Only schedule a tick callback for the first `callback` addition.
  // Schedule on the current task queue to comply with RequestCallOnNextTick
  // requirements.
  if (callbacks_.size() == 1) {
    TaskQueueBase::Current()->PostDelayedTask(
        SafeTask(safety_.flag(),
                 [this] {
                   RTC_DCHECK_RUN_ON(&sequence_checker_);
                   std::vector<absl::AnyInvocable<void() &&>> callbacks;
                   callbacks_.swap(callbacks);
                   for (auto& callback : callbacks)
                     std::move(callback)();
                 }),
        tick_period_);
  }
}

TimeDelta TaskQueueMetronome::TickPeriod() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return tick_period_;
}

// Implementation of PeerConnectionIntegrationWrapper functions
void PeerConnectionIntegrationWrapper::StartWatchingDelayStats() {
  // Get the baseline numbers for audio_packets and audio_delay.
  auto received_stats = NewGetStats();
  ASSERT_THAT(received_stats, NotNull());
  auto inbound_stats =
      received_stats->GetStatsOfType<RTCInboundRtpStreamStats>();
  ASSERT_FALSE(inbound_stats.empty());
  auto rtp_stats = inbound_stats[0];
  ASSERT_TRUE(rtp_stats->relative_packet_arrival_delay.has_value());
  ASSERT_TRUE(rtp_stats->packets_received.has_value());
  rtp_stats_id_ = rtp_stats->id();
  audio_packets_stat_ = *rtp_stats->packets_received;
  audio_delay_stat_ = *rtp_stats->relative_packet_arrival_delay;
  audio_samples_stat_ = *rtp_stats->total_samples_received;
  audio_concealed_stat_ = *rtp_stats->concealed_samples;
}

void PeerConnectionIntegrationWrapper::UpdateDelayStats(std::string tag,
                                                        int desc_size) {
  auto report = NewGetStats();
  auto rtp_stats = report->GetAs<RTCInboundRtpStreamStats>(rtp_stats_id_);
  ASSERT_TRUE(rtp_stats);
  auto delta_packets = *rtp_stats->packets_received - audio_packets_stat_;
  auto delta_rpad =
      *rtp_stats->relative_packet_arrival_delay - audio_delay_stat_;
  auto recent_delay = delta_packets > 0 ? delta_rpad / delta_packets : -1;
  // The purpose of these checks is to sound the alarm early if we introduce
  // serious regressions. The numbers are not acceptable for production, but
  // occur on slow bots.
  //
  // An average relative packet arrival delay over the renegotiation of
  // > 100 ms indicates that something is dramatically wrong, and will impact
  // quality for sure.
  // Worst bots:
  // linux_x86_dbg at 0.206
#if !defined(NDEBUG)
  EXPECT_GT(0.25, recent_delay) << tag << " size " << desc_size;
#else
  EXPECT_GT(0.1, recent_delay) << tag << " size " << desc_size;
#endif
  auto delta_samples = *rtp_stats->total_samples_received - audio_samples_stat_;
  auto delta_concealed = *rtp_stats->concealed_samples - audio_concealed_stat_;
  // These limits should be adjusted down as we improve:
  //
  // Concealing more than 4000 samples during a renegotiation is unacceptable.
  // But some bots are slow.

  // Worst bots:
  // linux_more_configs bot at conceal count 5184
  // android_arm_rel at conceal count 9241
  // linux_x86_dbg at 15174
#if !defined(NDEBUG)
  EXPECT_GT(18000U, delta_concealed) << "Concealed " << delta_concealed
                                     << " of " << delta_samples << " samples";
#else
  EXPECT_GT(15000U, delta_concealed) << "Concealed " << delta_concealed
                                     << " of " << delta_samples << " samples";
#endif
  // Concealing more than 20% of samples during a renegotiation is
  // unacceptable.
  // Worst bots:
  // Nondebug: Linux32 Release at conceal rate 0.606597 (CI run)
  // Debug: linux_x86_dbg bot at conceal rate 0.854
  //        internal bot at conceal rate 0.967 (b/294020344)
  // TODO(https://crbug.com/webrtc/15393): Improve audio quality during
  // renegotiation so that we can reduce these thresholds, 99% is not even
  // close to the 20% deemed unacceptable above or the 0% that would be ideal.
  // Require at least 2000 samples (roughly 2x 20ms packets).
  if (delta_samples >= 2000) {
    audio_delay_stats_percentage_checked_ = true;
#if !defined(NDEBUG)
    EXPECT_LT(1.0 * delta_concealed / delta_samples, 0.99)
        << "Concealed " << delta_concealed << " of " << delta_samples
        << " samples";
#else
    EXPECT_LT(1.0 * delta_concealed / delta_samples, 0.7)
        << "Concealed " << delta_concealed << " of " << delta_samples
        << " samples";
#endif
  }
  // Increment trailing counters
  audio_packets_stat_ = *rtp_stats->packets_received;
  audio_delay_stat_ = *rtp_stats->relative_packet_arrival_delay;
  audio_samples_stat_ = *rtp_stats->total_samples_received;
  audio_concealed_stat_ = *rtp_stats->concealed_samples;
}

bool PeerConnectionIntegrationWrapper::Init(
    const PeerConnectionFactory::Options* options,
    const PeerConnectionInterface::RTCConfiguration* config,
    PeerConnectionDependencies dependencies,
    SocketServer* socket_server,
    Thread* network_thread,
    Thread* worker_thread,
    std::unique_ptr<FakeRtcEventLogFactory> event_log_factory,
    bool reset_encoder_factory,
    bool reset_decoder_factory,
    bool create_media_engine) {
  // There's an error in this test code if Init ends up being called twice.
  RTC_DCHECK(!peer_connection_);
  RTC_DCHECK(!peer_connection_factory_);

  auto network_manager = std::make_unique<FakeNetworkManager>(network_thread);
  fake_network_manager_ = network_manager.get();
  fake_network_manager_->AddInterface(kDefaultLocalAddress);

  network_thread_ = network_thread;

  fake_audio_capture_module_ =
      FakeAudioCaptureModule::Create(test_->CreateThread("AudioCaptureThread"));

  if (!fake_audio_capture_module_) {
    return false;
  }
  Thread* const signaling_thread = Thread::Current();

  PeerConnectionFactoryDependencies pc_factory_dependencies;
  pc_factory_dependencies.network_thread = network_thread;
  pc_factory_dependencies.worker_thread = worker_thread;
  pc_factory_dependencies.signaling_thread = signaling_thread;
  pc_factory_dependencies.socket_factory = socket_server;
  pc_factory_dependencies.network_manager = std::move(network_manager);
  pc_factory_dependencies.env = env_;
  pc_factory_dependencies.decode_metronome =
      std::make_unique<TaskQueueMetronome>(TimeDelta::Millis(8));

  pc_factory_dependencies.adm = fake_audio_capture_module_;
  if (create_media_engine) {
    // Standard creation method for APM may return a null pointer when
    // AudioProcessing is disabled with a build flag. Bypass that flag by
    // explicitly injecting the factory.
    pc_factory_dependencies.audio_processing_builder =
        std::make_unique<BuiltinAudioProcessingBuilder>();
    EnableMediaWithDefaults(pc_factory_dependencies);
  }

  if (reset_encoder_factory) {
    pc_factory_dependencies.video_encoder_factory.reset();
  }
  if (reset_decoder_factory) {
    pc_factory_dependencies.video_decoder_factory.reset();
  }

  if (event_log_factory) {
    event_log_factory_ = event_log_factory.get();
    pc_factory_dependencies.event_log_factory = std::move(event_log_factory);
  } else {
    pc_factory_dependencies.event_log_factory =
        std::make_unique<RtcEventLogFactory>();
  }
  peer_connection_factory_ =
      CreateModularPeerConnectionFactory(std::move(pc_factory_dependencies));

  if (!peer_connection_factory_) {
    fake_network_manager_ = nullptr;
    return false;
  }
  if (options) {
    peer_connection_factory_->SetOptions(*options);
  }
  if (config) {
    sdp_semantics_ = config->sdp_semantics;
  }

  peer_connection_ = CreatePeerConnection(config, std::move(dependencies));
  return peer_connection_.get() != nullptr;
}

namespace internal {

// Utility class for tests that run multiple operations that cause excessive
// logging at the INFO level or below. Use to raise the logging level to e.g.
// LS_WARNING or above. Once an instance of ScopedSetLoggingLevel goes out of
// scope, the logging level is restored to what it was previously set to.
class PeerConnectionIntegrationTestBase::ScopedSetLoggingLevel {
 public:
  explicit ScopedSetLoggingLevel(LoggingSeverity new_severity) {
    LogMessage::LogToDebug(new_severity);
  }
  ~ScopedSetLoggingLevel() { LogMessage::LogToDebug(previous_severity_); }

 private:
  const LoggingSeverity previous_severity_ = LogMessage::GetLogToDebug();
};

PeerConnectionIntegrationTestBase::PeerConnectionIntegrationTestBase(
    Environment env,
    SdpSemantics sdp_semantics)
    : sdp_semantics_(sdp_semantics),
      env_(std::move(env)),
      ss_(new VirtualSocketServer()),
      fss_(new FirewallSocketServer(ss_.get(), nullptr, false)),
      network_thread_(new Thread(fss_.get())),
      worker_thread_(Thread::Create()) {
  network_thread_->SetName("PCNetworkThread", this);
  worker_thread_->SetName("PCWorkerThread", this);
  RTC_CHECK(network_thread_->Start());
  RTC_CHECK(worker_thread_->Start());
  metrics::Reset();
}

PeerConnectionIntegrationTestBase::PeerConnectionIntegrationTestBase(
    Environment env,
    SdpSemantics sdp_semantics,
    TimeController* time_controller)
    : sdp_semantics_(sdp_semantics), env_(std::move(env)) {
  ss_ = std::make_unique<VirtualSocketServer>();
  fss_ = std::make_unique<FirewallSocketServer>(ss_.get(), nullptr, false);
  network_thread_ = time_controller->CreateThreadWithSocketServer(
      "PCNetworkThread", fss_.get());
  worker_thread_ = time_controller->CreateThread("PCWorkerThread");
  network_thread_->SetName("PCNetworkThread", this);
  worker_thread_->SetName("PCWorkerThread", this);
  metrics::Reset();
}

PeerConnectionIntegrationTestBase::~PeerConnectionIntegrationTestBase() {
  // The PeerConnections should be deleted before the TurnCustomizers.
  // A TurnPort is created with a raw pointer to a TurnCustomizer. The
  // TurnPort has the same lifetime as the PeerConnection, so it's expected
  // that the TurnCustomizer outlives the life of the PeerConnection or else
  // when Send() is called it will hit a seg fault.
  DestroyPeerConnections();
}

void PeerConnectionIntegrationTestBase::DestroyTurnServers() {
  ExecuteTask(*network_thread(), [this] {
    turn_servers_.clear();
    turn_customizers_.clear();
  });
}

void PeerConnectionIntegrationTestBase::DestroyThreads() {
  worker_thread_.reset();
  network_thread_.reset();
}

void PeerConnectionIntegrationTestBase::OverrideLoggingLevelForTest(
    LoggingSeverity new_severity) {
  RTC_DCHECK(!overridden_logging_level_);
  overridden_logging_level_ =
      std::make_unique<ScopedSetLoggingLevel>(new_severity);
}

std::unique_ptr<PeerConnectionIntegrationWrapper>
PeerConnectionIntegrationTestBase::CreatePeerConnectionWrapperInternal(
    const std::string& debug_name,
    Environment env) {
  return std::unique_ptr<PeerConnectionIntegrationWrapper>(
      new PeerConnectionIntegrationWrapper(debug_name, env, this));
}

}  // namespace internal

PeerConnectionIntegrationBaseTest::PeerConnectionIntegrationBaseTest(
    SdpSemantics sdp_semantics)
    : internal::PeerConnectionIntegrationTestBase(CreateTestEnvironment(),
                                                  sdp_semantics) {}

PeerConnectionIntegrationTestWithSimulatedTime::

    PeerConnectionIntegrationTestWithSimulatedTime(SdpSemantics sdp_semantics)
    : PeerConnectionIntegrationTestWithSimulatedTime(
          sdp_semantics,
          std::make_unique<GlobalSimulatedTimeController>(
              Timestamp::Seconds(1000))) {}

PeerConnectionIntegrationTestWithSimulatedTime::
    ~PeerConnectionIntegrationTestWithSimulatedTime() {
  DestroyPeerConnections();
  DestroyTurnServers();
  time_controller_->AdvanceTime(TimeDelta::Zero());
  // Explicitly destroy threads before time_controller_ is destroyed.
  // Simulated threads hold a pointer to the time controller and will
  // attempt to unregister from it in their destructor. Since time_controller_
  // is owned by this derived class, it will be destroyed BEFORE the base
  // class destroys the threads, causing a crash if we don't do this here.
  DestroyThreads();
}

PeerConnectionIntegrationTestWithSimulatedTime::
    PeerConnectionIntegrationTestWithSimulatedTime(
        SdpSemantics sdp_semantics,
        std::unique_ptr<GlobalSimulatedTimeController> time_controller)
    : internal::PeerConnectionIntegrationTestBase(
          [](TimeController* tc) {
            CreateTestEnvironmentOptions options;
            options.time = tc;
            return CreateTestEnvironment(std::move(options));
          }(time_controller.get()),
          sdp_semantics,
          time_controller.get()),
      time_controller_(std::move(time_controller)) {}

}  // namespace webrtc
