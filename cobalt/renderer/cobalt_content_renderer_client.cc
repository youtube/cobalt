// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/renderer/cobalt_content_renderer_client.h"

#include <string>

#include "components/cdm/renderer/widevine_key_system_info.h"
#include "starboard/media.h"

namespace cobalt {

namespace {

// TODO(b/376542844): Eliminate the usage of hardcoded MIME string once we
// support to query codec capabilities with configs. The profile information
// gets lost with hardcoded MIME string. This can sometimes cause issues. For
// example, vp9 profile 2 indicates hdr support, so an implementation accepts
// "codecs=vp9" may reject "codecs=vp9.2".
std::string GetMimeFromVideoType(const media::VideoType& type) {
  // The MIME string is for very basic video codec supportability check.
  switch (type.codec) {
    case media::VideoCodec::kH264:
      return "video/mp4; codecs=\"avc1.4d4015\"";
    case media::VideoCodec::kVP9:
      return "video/webm; codecs=\"vp9\"";
    case media::VideoCodec::kAV1:
      return "video/mp4; codecs=\"av01.0.08M.08\"";
    default:
      return "";
  }
}

// TODO(b/376542844): Eliminate the usage of hardcoded MIME string once we
// support to query codec capabilities with configs.
std::string GetMimeFromAudioType(const media::AudioType& type) {
  // The MIME string is for very basic audio codec supportability check.
  switch (type.codec) {
    case media::AudioCodec::kAAC:
      return "audio/mp4; codecs=\"mp4a.40.2\"";
    case media::AudioCodec::kAC3:
      return "audio/mp4; codecs=\"ac-3\"";
    case media::AudioCodec::kEAC3:
      return "audio/mp4; codecs=\"ec-3\"";
    case media::AudioCodec::kOpus:
      return "audio/webm; codecs=\"opus\"";
    // TODO(b/375232937): Support IAMF
    default:
      return "";
  }
}

media::SupportedCodecs GetStarboardEmeSupportedCodecs() {
  media::SupportedCodecs codecs =
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

CobaltContentRendererClient::CobaltContentRendererClient() {}
CobaltContentRendererClient::~CobaltContentRendererClient() {}

void CobaltContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new js_injection::JsCommunication(render_frame);
}

#if BUILDFLAG(IS_ANDROID)
void AddStarboardCmaKeySystems(::media::KeySystemInfos* key_system_infos) {
  media::SupportedCodecs codecs = GetStarboardEmeSupportedCodecs();

  using Robustness = cdm::WidevineKeySystemInfo::Robustness;

  const base::flat_set<media::EncryptionScheme> kEncryptionSchemes = {
      media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs};

  const base::flat_set<media::CdmSessionType> kSessionTypes = {
      media::CdmSessionType::kTemporary};

  key_system_infos->emplace_back(new cdm::WidevineKeySystemInfo(
      codecs,                        // Regular codecs.
      kEncryptionSchemes,            // Encryption schemes.
      kSessionTypes,                 // Session types.
      codecs,                        // Hardware secure codecs.
      kEncryptionSchemes,            // Hardware secure encryption schemes.
      kSessionTypes,                 // Hardware secure session types.
      Robustness::HW_SECURE_CRYPTO,  // Max audio robustness.
      Robustness::HW_SECURE_ALL,     // Max video robustness.
      media::EmeFeatureSupport::ALWAYS_ENABLED,    // Persistent state.
      media::EmeFeatureSupport::ALWAYS_ENABLED));  // Distinctive identifier.
}
#endif

void CobaltContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  media::KeySystemInfos key_systems;
#if BUILDFLAG(IS_ANDROID)
  AddStarboardCmaKeySystems(&key_systems);
  std::move(cb).Run(std::move(key_systems));
#endif
}

bool CobaltContentRendererClient::IsSupportedAudioType(
    const media::AudioType& type) {
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
    const media::VideoType& type) {
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

}  // namespace cobalt
