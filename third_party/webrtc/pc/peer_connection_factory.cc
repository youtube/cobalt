/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/peer_connection_factory.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/audio_options.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/ice_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/network_control.h"
#include "api/units/data_rate.h"
#include "call/call_config.h"
#include "call/rtp_transport_controller_send_factory.h"
#include "media/base/codec.h"
#include "media/base/media_engine.h"
#include "p2p/base/basic_async_resolver_factory.h"
#include "p2p/base/default_ice_transport_factory.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/client/basic_port_allocator.h"
#include "pc/audio_track.h"
#include "pc/codec_vendor.h"
#include "pc/connection_context.h"
#include "pc/ice_server_parsing.h"
#include "pc/local_audio_source.h"
#include "pc/media_factory.h"
#include "pc/media_stream.h"
#include "pc/media_stream_proxy.h"
#include "pc/media_stream_track_proxy.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_factory_proxy.h"
#include "pc/peer_connection_proxy.h"
#include "pc/rtp_parameters_conversion.h"
#include "pc/video_track.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/system/file_wrapper.h"

namespace webrtc {
namespace {

Environment AssembleEnvironment(PeerConnectionFactoryDependencies& deps) {
  // Assemble Environment here rather than in ConnectionContext::Create
  // to avoid dependency on EnvironmentFactory by ConnectionContext and thus its
  // users.
  EnvironmentFactory env_factory = deps.env.has_value()
                                       ? EnvironmentFactory(*deps.env)
                                       : EnvironmentFactory();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  env_factory.Set(std::move(deps.trials));
  env_factory.Set(std::move(deps.task_queue_factory));
#pragma clang diagnostic pop

  // Clear Environment from `deps` to avoid accidental usage of the wrong
  // Environment.
  deps.env = std::nullopt;
  return env_factory.Create();
}

}  // namespace

scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    PeerConnectionFactoryDependencies dependencies) {
  // The PeerConnectionFactory must be created on the signaling thread.
  if (dependencies.signaling_thread &&
      !dependencies.signaling_thread->IsCurrent()) {
    return dependencies.signaling_thread->BlockingCall([&dependencies] {
      return CreateModularPeerConnectionFactory(std::move(dependencies));
    });
  }

  auto pc_factory = PeerConnectionFactory::Create(std::move(dependencies));
  if (!pc_factory) {
    return nullptr;
  }
  // Verify that the invocation and the initialization ended up agreeing on the
  // thread.
  RTC_DCHECK_RUN_ON(pc_factory->signaling_thread());
  return PeerConnectionFactoryProxy::Create(
      pc_factory->signaling_thread(), pc_factory->worker_thread(), pc_factory);
}

// Static
scoped_refptr<PeerConnectionFactory> PeerConnectionFactory::Create(
    PeerConnectionFactoryDependencies dependencies) {
  auto context = ConnectionContext::Create(AssembleEnvironment(dependencies),
                                           &dependencies);
  if (!context) {
    return nullptr;
  }
  return make_ref_counted<PeerConnectionFactory>(context, &dependencies);
}

PeerConnectionFactory::PeerConnectionFactory(
    scoped_refptr<ConnectionContext> context,
    PeerConnectionFactoryDependencies* dependencies)
    : context_(context),
      codec_vendor_(context_->media_engine(),
                    context_->use_rtx(),
                    context_->env().field_trials()),

      event_log_factory_(std::move(dependencies->event_log_factory)),
      fec_controller_factory_(std::move(dependencies->fec_controller_factory)),
      network_state_predictor_factory_(
          std::move(dependencies->network_state_predictor_factory)),
      injected_network_controller_factory_(
          std::move(dependencies->network_controller_factory)),
      neteq_factory_(std::move(dependencies->neteq_factory)),
      transport_controller_send_factory_(
          (dependencies->transport_controller_send_factory)
              ? std::move(dependencies->transport_controller_send_factory)
              : std::make_unique<RtpTransportControllerSendFactory>()),
      decode_metronome_(std::move(dependencies->decode_metronome)),
      encode_metronome_(std::move(dependencies->encode_metronome)) {}

PeerConnectionFactory::PeerConnectionFactory(
    PeerConnectionFactoryDependencies dependencies)
    : PeerConnectionFactory(
          ConnectionContext::Create(AssembleEnvironment(dependencies),
                                    &dependencies),
          &dependencies) {}

PeerConnectionFactory::~PeerConnectionFactory() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  worker_thread()->BlockingCall([this] {
    RTC_DCHECK_RUN_ON(worker_thread());
    decode_metronome_ = nullptr;
    encode_metronome_ = nullptr;
  });
}

void PeerConnectionFactory::SetOptions(const Options& options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  options_ = options;
}

RtpCapabilities PeerConnectionFactory::GetRtpSenderCapabilities(
    MediaType kind) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  switch (kind) {
    case MediaType::AUDIO: {
      Codecs cricket_codecs;
      cricket_codecs = codec_vendor_.audio_send_codecs().codecs();
      auto extensions =
          GetDefaultEnabledRtpHeaderExtensions(media_engine()->voice());
      return ToRtpCapabilities(cricket_codecs, extensions);
    }
    case MediaType::VIDEO: {
      Codecs cricket_codecs;
      cricket_codecs = codec_vendor_.video_send_codecs().codecs();
      auto extensions =
          GetDefaultEnabledRtpHeaderExtensions(media_engine()->video());
      return ToRtpCapabilities(cricket_codecs, extensions);
    }
    default:
      return RtpCapabilities();
  }
  RTC_DLOG(LS_ERROR) << "Got unexpected MediaType " << kind;
  RTC_CHECK_NOTREACHED();
}

RtpCapabilities PeerConnectionFactory::GetRtpReceiverCapabilities(
    MediaType kind) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  switch (kind) {
    case MediaType::AUDIO: {
      Codecs cricket_codecs;
      cricket_codecs = codec_vendor_.audio_recv_codecs().codecs();
      auto extensions =
          GetDefaultEnabledRtpHeaderExtensions(media_engine()->voice());
      return ToRtpCapabilities(cricket_codecs, extensions);
    }
    case MediaType::VIDEO: {
      Codecs cricket_codecs = codec_vendor_.video_recv_codecs().codecs();
      auto extensions =
          GetDefaultEnabledRtpHeaderExtensions(media_engine()->video());
      return ToRtpCapabilities(cricket_codecs, extensions);
    }
    default:
      return RtpCapabilities();
  }
  RTC_DLOG(LS_ERROR) << "Got unexpected MediaType " << kind;
  RTC_CHECK_NOTREACHED();
}

scoped_refptr<AudioSourceInterface> PeerConnectionFactory::CreateAudioSource(
    const AudioOptions& options) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  scoped_refptr<LocalAudioSource> source(LocalAudioSource::Create(&options));
  return source;
}

bool PeerConnectionFactory::StartAecDump(FILE* file, int64_t max_size_bytes) {
  RTC_DCHECK_RUN_ON(worker_thread());
  return media_engine()->voice().StartAecDump(FileWrapper(file),
                                              max_size_bytes);
}

void PeerConnectionFactory::StopAecDump() {
  RTC_DCHECK_RUN_ON(worker_thread());
  media_engine()->voice().StopAecDump();
}

MediaEngineInterface* PeerConnectionFactory::media_engine() const {
  RTC_DCHECK(context_);
  return context_->media_engine();
}

RTCErrorOr<scoped_refptr<PeerConnectionInterface>>
PeerConnectionFactory::CreatePeerConnectionOrError(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    PeerConnectionDependencies dependencies) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  // TODO(https://crbug.com/webrtc/13528): Remove support for kPlanB.
  if (configuration.sdp_semantics == SdpSemantics::kPlanB_DEPRECATED) {
    RTC_LOG(LS_WARNING)
        << "PeerConnection constructed with legacy SDP semantics!";
  }

  RTCError err = IceConfig(configuration).IsValid();
  if (!err.ok()) {
    RTC_LOG(LS_ERROR) << "Invalid ICE configuration: " << err.message();
    return err;
  }

  ServerAddresses stun_servers;
  std::vector<RelayServerConfig> turn_servers;
  err = ParseAndValidateIceServersFromConfiguration(configuration, stun_servers,
                                                    turn_servers);
  if (!err.ok()) {
    return err;
  }

  if (!dependencies.observer) {
    RTC_LOG(LS_ERROR) << "PeerConnection initialized without a "
                         "PeerConnectionObserver";
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Attempt to create a PeerConnection without an observer");
  }

  EnvironmentFactory env_factory(context_->env());

  // Field trials active for this PeerConnection is the first of:
  // a) Specified in the PeerConnectionDependencies
  // b) Specified in the PeerConnectionFactoryDependencies
  // c) Created as default by the EnvironmentFactory.
  env_factory.Set(std::move(dependencies.trials));

  if (event_log_factory_ != nullptr) {
    worker_thread()->BlockingCall([&] {
      Environment env_for_rtc_event_log = env_factory.Create();
      env_factory.Set(event_log_factory_->Create(env_for_rtc_event_log));
    });
  }

  const Environment env = env_factory.Create();

  // Set internal defaults if optional dependencies are not set.
  if (!dependencies.cert_generator) {
    dependencies.cert_generator = std::make_unique<RTCCertificateGenerator>(
        signaling_thread(), network_thread());
  }

  if (!dependencies.async_dns_resolver_factory) {
    dependencies.async_dns_resolver_factory =
        std::make_unique<BasicAsyncDnsResolverFactory>();
  }

  if (!dependencies.allocator) {
    dependencies.allocator = std::make_unique<BasicPortAllocator>(
        env, context_->default_network_manager(),
        context_->default_socket_factory(), configuration.turn_customizer);
    dependencies.allocator->SetPortRange(
        configuration.port_allocator_config.min_port,
        configuration.port_allocator_config.max_port);
    dependencies.allocator->set_flags(
        configuration.port_allocator_config.flags);
  }

  if (!dependencies.ice_transport_factory) {
    dependencies.ice_transport_factory =
        std::make_unique<DefaultIceTransportFactory>();
  }

  dependencies.allocator->SetNetworkIgnoreMask(options().network_ignore_mask);
  dependencies.allocator->SetVpnList(configuration.vpn_list);

  std::unique_ptr<NetworkControllerFactoryInterface>
      network_controller_factory =
          std::move(dependencies.network_controller_factory);
  std::unique_ptr<Call> call = worker_thread()->BlockingCall(
      [this, &env, &configuration, &network_controller_factory] {
        return CreateCall_w(env, std::move(configuration),
                            std::move(network_controller_factory));
      });

  auto pc = PeerConnection::Create(env, context_, options_, std::move(call),
                                   configuration, dependencies, stun_servers,
                                   turn_servers);
  // We configure the proxy with a pointer to the network thread for methods
  // that need to be invoked there rather than on the signaling thread.
  // Internally, the proxy object has a member variable named `worker_thread_`
  // which will point to the network thread (and not the factory's
  // worker_thread()).  All such methods have thread checks though, so the code
  // should still be clear (outside of macro expansion).
  return scoped_refptr<PeerConnectionInterface>(PeerConnectionProxy::Create(
      signaling_thread(), network_thread(), std::move(pc)));
}

scoped_refptr<MediaStreamInterface>
PeerConnectionFactory::CreateLocalMediaStream(const std::string& stream_id) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  return MediaStreamProxy::Create(signaling_thread(),
                                  MediaStream::Create(stream_id));
}

scoped_refptr<VideoTrackInterface> PeerConnectionFactory::CreateVideoTrack(
    scoped_refptr<VideoTrackSourceInterface> source,
    absl::string_view id) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  scoped_refptr<VideoTrackInterface> track =
      VideoTrack::Create(id, source, worker_thread());
  return VideoTrackProxy::Create(signaling_thread(), worker_thread(), track);
}

scoped_refptr<AudioTrackInterface> PeerConnectionFactory::CreateAudioTrack(
    const std::string& id,
    AudioSourceInterface* source) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(id, scoped_refptr<AudioSourceInterface>(source));
  return AudioTrackProxy::Create(signaling_thread(), track);
}

std::unique_ptr<Call> PeerConnectionFactory::CreateCall_w(
    const Environment& env,
    const PeerConnectionInterface::RTCConfiguration& configuration,
    std::unique_ptr<NetworkControllerFactoryInterface>
        per_call_network_controller_factory) {
  RTC_DCHECK_RUN_ON(worker_thread());

  CallConfig call_config(env, network_thread());
  if (!media_engine() || !context_->call_factory()) {
    return nullptr;
  }
  call_config.audio_state = media_engine()->voice().GetAudioState();

  FieldTrialParameter<DataRate> min_bandwidth("min",
                                              DataRate::KilobitsPerSec(30));
  FieldTrialParameter<DataRate> start_bandwidth("start",
                                                DataRate::KilobitsPerSec(300));
  FieldTrialParameter<DataRate> max_bandwidth("max",
                                              DataRate::KilobitsPerSec(2000));
  ParseFieldTrial({&min_bandwidth, &start_bandwidth, &max_bandwidth},
                  env.field_trials().Lookup("WebRTC-PcFactoryDefaultBitrates"));

  call_config.bitrate_config.min_bitrate_bps =
      saturated_cast<int>(min_bandwidth->bps());
  call_config.bitrate_config.start_bitrate_bps =
      saturated_cast<int>(start_bandwidth->bps());
  call_config.bitrate_config.max_bitrate_bps =
      saturated_cast<int>(max_bandwidth->bps());

  call_config.fec_controller_factory = fec_controller_factory_.get();
  call_config.network_state_predictor_factory =
      network_state_predictor_factory_.get();
  call_config.neteq_factory = neteq_factory_.get();

  if (per_call_network_controller_factory != nullptr) {
    RTC_LOG(LS_INFO) << "Using pc injected network controller factory";
    call_config.per_call_network_controller_factory =
        std::move(per_call_network_controller_factory);
  } else if (IsTrialEnabled("WebRTC-Bwe-InjectedCongestionController")) {
    RTC_LOG(LS_INFO) << "Using pcf injected network controller factory";
    call_config.network_controller_factory =
        injected_network_controller_factory_.get();
  } else {
    RTC_LOG(LS_INFO) << "Using default network controller factory";
  }

  call_config.rtp_transport_controller_send_factory =
      transport_controller_send_factory_.get();
  call_config.decode_metronome = decode_metronome_.get();
  call_config.encode_metronome = encode_metronome_.get();
  call_config.pacer_burst_interval = configuration.pacer_burst_interval;
  return context_->call_factory()->CreateCall(std::move(call_config));
}

bool PeerConnectionFactory::IsTrialEnabled(absl::string_view key) const {
  return absl::StartsWith(field_trials().Lookup(key), "Enabled");
}

}  // namespace webrtc
