/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/pclf/peer_configurer.h"

#include <set>

#include "absl/strings/string_view.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/peer_network_dependencies.h"

namespace webrtc {
namespace webrtc_pc_e2e {

PeerConfigurer::PeerConfigurer(
    const PeerNetworkDependencies& network_dependencies)
    : components_(std::make_unique<InjectableComponents>(
          network_dependencies.network_thread,
          network_dependencies.network_manager,
          network_dependencies.packet_socket_factory)),
      params_(std::make_unique<Params>()),
      configurable_params_(std::make_unique<ConfigurableParams>()) {}

PeerConfigurer* PeerConfigurer::SetName(absl::string_view name) {
  params_->name = std::string(name);
  return this;
}

PeerConfigurer* PeerConfigurer::SetTaskQueueFactory(
    std::unique_ptr<TaskQueueFactory> task_queue_factory) {
  components_->pcf_dependencies->task_queue_factory =
      std::move(task_queue_factory);
  return this;
}
PeerConfigurer* PeerConfigurer::SetCallFactory(
    std::unique_ptr<CallFactoryInterface> call_factory) {
  components_->pcf_dependencies->call_factory = std::move(call_factory);
  return this;
}
PeerConfigurer* PeerConfigurer::SetEventLogFactory(
    std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory) {
  components_->pcf_dependencies->event_log_factory =
      std::move(event_log_factory);
  return this;
}
PeerConfigurer* PeerConfigurer::SetFecControllerFactory(
    std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory) {
  components_->pcf_dependencies->fec_controller_factory =
      std::move(fec_controller_factory);
  return this;
}
PeerConfigurer* PeerConfigurer::SetNetworkControllerFactory(
    std::unique_ptr<NetworkControllerFactoryInterface>
        network_controller_factory) {
  components_->pcf_dependencies->network_controller_factory =
      std::move(network_controller_factory);
  return this;
}
PeerConfigurer* PeerConfigurer::SetVideoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory) {
  components_->pcf_dependencies->video_encoder_factory =
      std::move(video_encoder_factory);
  return this;
}
PeerConfigurer* PeerConfigurer::SetVideoDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory) {
  components_->pcf_dependencies->video_decoder_factory =
      std::move(video_decoder_factory);
  return this;
}
PeerConfigurer* PeerConfigurer::SetAudioEncoderFactory(
    rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory) {
  components_->pcf_dependencies->audio_encoder_factory = audio_encoder_factory;
  return this;
}
PeerConfigurer* PeerConfigurer::SetAudioDecoderFactory(
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory) {
  components_->pcf_dependencies->audio_decoder_factory = audio_decoder_factory;
  return this;
}
PeerConfigurer* PeerConfigurer::SetAsyncResolverFactory(
    std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory) {
  components_->pc_dependencies->async_resolver_factory =
      std::move(async_resolver_factory);
  return this;
}
PeerConfigurer* PeerConfigurer::SetRTCCertificateGenerator(
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator) {
  components_->pc_dependencies->cert_generator = std::move(cert_generator);
  return this;
}
PeerConfigurer* PeerConfigurer::SetSSLCertificateVerifier(
    std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier) {
  components_->pc_dependencies->tls_cert_verifier =
      std::move(tls_cert_verifier);
  return this;
}

PeerConfigurer* PeerConfigurer::AddVideoConfig(VideoConfig config) {
  video_sources_.push_back(
      CreateSquareFrameGenerator(config, /*type=*/absl::nullopt));
  configurable_params_->video_configs.push_back(std::move(config));
  return this;
}
PeerConfigurer* PeerConfigurer::AddVideoConfig(
    VideoConfig config,
    std::unique_ptr<test::FrameGeneratorInterface> generator) {
  configurable_params_->video_configs.push_back(std::move(config));
  video_sources_.push_back(std::move(generator));
  return this;
}
PeerConfigurer* PeerConfigurer::AddVideoConfig(VideoConfig config,
                                               CapturingDeviceIndex index) {
  configurable_params_->video_configs.push_back(std::move(config));
  video_sources_.push_back(index);
  return this;
}
PeerConfigurer* PeerConfigurer::SetVideoSubscription(
    VideoSubscription subscription) {
  configurable_params_->video_subscription = std::move(subscription);
  return this;
}
PeerConfigurer* PeerConfigurer::SetVideoCodecs(
    std::vector<VideoCodecConfig> video_codecs) {
  params_->video_codecs = std::move(video_codecs);
  return this;
}
PeerConfigurer* PeerConfigurer::SetExtraVideoRtpHeaderExtensions(
    std::vector<std::string> extensions) {
  params_->extra_video_rtp_header_extensions = std::move(extensions);
  return this;
}
PeerConfigurer* PeerConfigurer::SetAudioConfig(AudioConfig config) {
  params_->audio_config = std::move(config);
  return this;
}
PeerConfigurer* PeerConfigurer::SetExtraAudioRtpHeaderExtensions(
    std::vector<std::string> extensions) {
  params_->extra_audio_rtp_header_extensions = std::move(extensions);
  return this;
}
PeerConfigurer* PeerConfigurer::SetUseUlpFEC(bool value) {
  params_->use_ulp_fec = value;
  return this;
}
PeerConfigurer* PeerConfigurer::SetUseFlexFEC(bool value) {
  params_->use_flex_fec = value;
  return this;
}
PeerConfigurer* PeerConfigurer::SetVideoEncoderBitrateMultiplier(
    double multiplier) {
  params_->video_encoder_bitrate_multiplier = multiplier;
  return this;
}
PeerConfigurer* PeerConfigurer::SetNetEqFactory(
    std::unique_ptr<NetEqFactory> neteq_factory) {
  components_->pcf_dependencies->neteq_factory = std::move(neteq_factory);
  return this;
}
PeerConfigurer* PeerConfigurer::SetAudioProcessing(
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) {
  components_->pcf_dependencies->audio_processing = audio_processing;
  return this;
}
PeerConfigurer* PeerConfigurer::SetAudioMixer(
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer) {
  components_->pcf_dependencies->audio_mixer = audio_mixer;
  return this;
}

PeerConfigurer* PeerConfigurer::SetUseNetworkThreadAsWorkerThread() {
  components_->worker_thread = components_->network_thread;
  return this;
}

PeerConfigurer* PeerConfigurer::SetRtcEventLogPath(absl::string_view path) {
  params_->rtc_event_log_path = std::string(path);
  return this;
}
PeerConfigurer* PeerConfigurer::SetAecDumpPath(absl::string_view path) {
  params_->aec_dump_path = std::string(path);
  return this;
}
PeerConfigurer* PeerConfigurer::SetRTCConfiguration(
    PeerConnectionInterface::RTCConfiguration configuration) {
  params_->rtc_configuration = std::move(configuration);
  return this;
}
PeerConfigurer* PeerConfigurer::SetRTCOfferAnswerOptions(
    PeerConnectionInterface::RTCOfferAnswerOptions options) {
  params_->rtc_offer_answer_options = std::move(options);
  return this;
}
PeerConfigurer* PeerConfigurer::SetBitrateSettings(
    BitrateSettings bitrate_settings) {
  params_->bitrate_settings = bitrate_settings;
  return this;
}

PeerConfigurer* PeerConfigurer::SetIceTransportFactory(
    std::unique_ptr<IceTransportFactory> factory) {
  components_->pc_dependencies->ice_transport_factory = std::move(factory);
  return this;
}

PeerConfigurer* PeerConfigurer::SetPortAllocatorExtraFlags(
    uint32_t extra_flags) {
  params_->port_allocator_extra_flags = extra_flags;
  return this;
}
std::unique_ptr<InjectableComponents> PeerConfigurer::ReleaseComponents() {
  RTC_CHECK(components_);
  auto components = std::move(components_);
  components_ = nullptr;
  return components;
}

// Returns Params and transfer ownership to the caller.
// Can be called once.
std::unique_ptr<Params> PeerConfigurer::ReleaseParams() {
  RTC_CHECK(params_);
  auto params = std::move(params_);
  params_ = nullptr;
  return params;
}

// Returns ConfigurableParams and transfer ownership to the caller.
// Can be called once.
std::unique_ptr<ConfigurableParams>
PeerConfigurer::ReleaseConfigurableParams() {
  RTC_CHECK(configurable_params_);
  auto configurable_params = std::move(configurable_params_);
  configurable_params_ = nullptr;
  return configurable_params;
}

// Returns video sources and transfer frame generators ownership to the
// caller. Can be called once.
std::vector<PeerConfigurer::VideoSource> PeerConfigurer::ReleaseVideoSources() {
  auto video_sources = std::move(video_sources_);
  video_sources_.clear();
  return video_sources;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
