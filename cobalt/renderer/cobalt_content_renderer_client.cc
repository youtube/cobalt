// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/renderer/cobalt_content_renderer_client.h"

#include <string>

#include "base/time/time.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "cobalt/renderer/cobalt_render_frame_observer.h"
#include "components/cdm/renderer/widevine_key_system_info.h"
#include "components/js_injection/renderer/js_communication.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"
#include "media/renderers/video_overlay_factory.h"
#include "media/starboard/starboard_renderer_factory.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "starboard/media.h"
#include "starboard/player.h"

namespace cobalt {

namespace {

// TODO(b/376542844): Eliminate the usage of hardcoded MIME string once we
// support to query codec capabilities with configs. The profile information
// gets lost with hardcoded MIME string. This can sometimes cause issues. For
// example, vp9 profile 2 indicates hdr support, so an implementation accepts
// "codecs=vp9" may reject "codecs=vp9.2".
std::string GetMimeFromVideoType(const ::media::VideoType& type) {
  // The MIME string is for very basic video codec supportability check.
  switch (type.codec) {
    case ::media::VideoCodec::kH264:
      return "video/mp4; codecs=\"avc1.4d4015\"";
    case ::media::VideoCodec::kVP9:
      return "video/webm; codecs=\"vp9\"";
    case ::media::VideoCodec::kAV1:
      return "video/mp4; codecs=\"av01.0.08M.08\"";
    default:
      return "";
  }
}

// TODO(b/376542844): Eliminate the usage of hardcoded MIME string once we
// support to query codec capabilities with configs.
std::string GetMimeFromAudioType(const ::media::AudioType& type) {
  // The MIME string is for very basic audio codec supportability check.
  switch (type.codec) {
    case ::media::AudioCodec::kAAC:
      return "audio/mp4; codecs=\"mp4a.40.2\"";
    case ::media::AudioCodec::kAC3:
      return "audio/mp4; codecs=\"ac-3\"";
    case ::media::AudioCodec::kEAC3:
      return "audio/mp4; codecs=\"ec-3\"";
    case ::media::AudioCodec::kOpus:
      return "audio/webm; codecs=\"opus\"";
    // TODO(b/375232937): Support IAMF
    default:
      return "";
  }
}

::media::SupportedCodecs GetStarboardEmeSupportedCodecs() {
  ::media::SupportedCodecs codecs =
      ::media::EME_CODEC_AAC | ::media::EME_CODEC_AVC1 |
      ::media::EME_CODEC_VP9_PROFILE0 | ::media::EME_CODEC_VP9_PROFILE2 |
      ::media::EME_CODEC_VP8 | ::media::EME_CODEC_OPUS |
      ::media::EME_CODEC_VORBIS | ::media::EME_CODEC_MPEG_H_AUDIO |
      ::media::EME_CODEC_FLAC | ::media::EME_CODEC_HEVC_PROFILE_MAIN |
      ::media::EME_CODEC_HEVC_PROFILE_MAIN10 | ::media::EME_CODEC_AV1 |
      ::media::EME_CODEC_AC3 | ::media::EME_CODEC_EAC3;
  // TODO(b/375232937) Add IAMF
  return codecs;
}

}  // namespace

CobaltContentRendererClient::CobaltContentRendererClient()
    : video_geometry_setter_service_(
          std::unique_ptr<media::VideoGeometrySetterService,
                          base::OnTaskRunnerDeleter>(
              nullptr,
              base::OnTaskRunnerDeleter(nullptr))) {
  DCHECK(content::RenderThread::IsMainThread());
}

CobaltContentRendererClient::~CobaltContentRendererClient() = default;

void CobaltContentRendererClient::RenderThreadStarted() {
  if (!video_geometry_setter_service_) {
    video_geometry_setter_service_ =
        std::unique_ptr<media::VideoGeometrySetterService,
                        base::OnTaskRunnerDeleter>(
            new media::VideoGeometrySetterService,
            base::OnTaskRunnerDeleter(
                base::SingleThreadTaskRunner::GetCurrentDefault()));
  }
}

void CobaltContentRendererClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  DCHECK(video_geometry_setter_service_);
  binders->Add<media::mojom::VideoGeometrySetter>(
      video_geometry_setter_service_->GetBindCallback(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void CobaltContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new js_injection::JsCommunication(render_frame);
  new CobaltRenderFrameObserver(render_frame);
}

#if BUILDFLAG(IS_ANDROID)
void AddStarboardCmaKeySystems(::media::KeySystemInfos* key_system_infos) {
  ::media::SupportedCodecs codecs = GetStarboardEmeSupportedCodecs();

  using Robustness = cdm::WidevineKeySystemInfo::Robustness;

  const base::flat_set<::media::EncryptionScheme> kEncryptionSchemes = {
      ::media::EncryptionScheme::kCenc, ::media::EncryptionScheme::kCbcs};

  const base::flat_set<::media::CdmSessionType> kSessionTypes = {
      ::media::CdmSessionType::kTemporary};

  key_system_infos->emplace_back(new cdm::WidevineKeySystemInfo(
      codecs,                        // Regular codecs.
      kEncryptionSchemes,            // Encryption schemes.
      kSessionTypes,                 // Session types.
      codecs,                        // Hardware secure codecs.
      kEncryptionSchemes,            // Hardware secure encryption schemes.
      kSessionTypes,                 // Hardware secure session types.
      Robustness::HW_SECURE_CRYPTO,  // Max audio robustness.
      Robustness::HW_SECURE_ALL,     // Max video robustness.
      ::media::EmeFeatureSupport::ALWAYS_ENABLED,    // Persistent state.
      ::media::EmeFeatureSupport::ALWAYS_ENABLED));  // Distinctive identifier.
}
#endif

void CobaltContentRendererClient::GetSupportedKeySystems(
    ::media::GetSupportedKeySystemsCB cb) {
  ::media::KeySystemInfos key_systems;
#if BUILDFLAG(IS_ANDROID)
  AddStarboardCmaKeySystems(&key_systems);
  std::move(cb).Run(std::move(key_systems));
#endif
}

bool CobaltContentRendererClient::IsSupportedAudioType(
    const ::media::AudioType& type) {
  std::string mime = GetMimeFromAudioType(type);
  SbMediaSupportType support_type = kSbMediaSupportTypeNotSupported;
  if (!mime.empty()) {
    support_type = SbMediaCanPlayMimeAndKeySystem(mime.c_str(), "");
  }
  bool result = support_type != kSbMediaSupportTypeNotSupported;
  LOG(INFO) << __func__ << "(" << type.codec << ") -> "
            << (result ? "true" : "false");
  return result;
}

bool CobaltContentRendererClient::IsSupportedVideoType(
    const ::media::VideoType& type) {
  std::string mime = GetMimeFromVideoType(type);
  SbMediaSupportType support_type = kSbMediaSupportTypeNotSupported;
  if (!mime.empty()) {
    support_type = SbMediaCanPlayMimeAndKeySystem(mime.c_str(), "");
  }
  bool result = support_type != kSbMediaSupportTypeNotSupported;
  LOG(INFO) << __func__ << "(" << type.codec << ") -> "
            << (result ? "true" : "false");
  return result;
}

void CobaltContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);
  communication->RunScriptsAtDocumentStart();
}

std::unique_ptr<::media::RendererFactory>
CobaltContentRendererClient::GetBaseRendererFactory(
    content::RenderFrame* /* render_frame */,
    ::media::MediaLog* media_log,
    ::media::DecoderFactory* /* decoder_factory */,
    base::RepeatingCallback<::media::GpuVideoAcceleratorFactories*()>
    /* get_gpu_factories_cb */) {
  // TODO(b/394368542): Add Content API to create StarboardRenderer.
  auto overlay_factory = std::make_unique<::media::VideoOverlayFactory>();
  base::TimeDelta audio_write_duration_local =
      base::Microseconds(kSbPlayerWriteDurationLocal);
  base::TimeDelta audio_write_duration_remote =
      base::Microseconds(kSbPlayerWriteDurationRemote);
  DCHECK(video_geometry_setter_service_);
  return std::make_unique<::media::StarboardRendererFactory>(
      media_log, std::move(overlay_factory),
      // TODO(b/383327725) - Cobalt: Inject these values from the web app.
      audio_write_duration_local, audio_write_duration_remote,
      video_geometry_setter_service_.get());
}

}  // namespace cobalt
