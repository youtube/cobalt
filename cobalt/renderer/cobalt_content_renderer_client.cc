// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/renderer/cobalt_content_renderer_client.h"

#include <string>
#include <variant>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "cobalt/renderer/cobalt_render_frame_observer.h"
#include "components/cdm/renderer/widevine_key_system_info.h"
#include "components/js_injection/renderer/js_communication.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/clients/starboard/starboard_renderer_client_factory.h"
#include "media/starboard/bind_host_receiver_callback.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cobalt {

namespace {

const char kH5vccSettingsKeyMediaEnableAllocateOnDemand[] =
    "Media.EnableAllocateOnDemand";
const char kH5vccSettingsKeyMediaEnableFlushDuringSeek[] =
    "Media.EnableFlushDuringSeek";
const char kH5vccSettingsKeyMediaEnableResetAudioDecoder[] =
    "Media.EnableResetAudioDecoder";
const char kH5vccSettingsKeyMediaVideoBufferSizeClampMb[] =
    "Media.VideoBufferSizeClampMb";

// Map that stores all current bindings of H5vcc settings to media switches.
// If a setting has a corresponding switch, we will enable the switch with the
// corresponding value.
const base::flat_map<std::string, const char*> kH5vccSettingToSwitchMap = {
    {kH5vccSettingsKeyMediaVideoBufferSizeClampMb,
     switches::kMSEVideoBufferSizeLimitClampMb},
};

struct ParsedH5vccSettings {
  bool enable_flush_during_seek = false;
  bool enable_reset_audio_decoder = false;
};

using H5vccSettingValue = std::variant<std::string, int64_t>;

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

void BindHostReceiverWithValuation(mojo::GenericPendingReceiver receiver) {
  content::RenderThread::Get()->BindHostReceiver(std::move(receiver));
}

// TODO: b/460292554 - This code is a tentative solution, and will be replaced
// once base::Feature is fully supported.
//
// Append the h5vcc setting to the corresponding media switch, if such mapping
// exists. H5vcc settings are either pass their value to a media switch for code
// in /media to use, or are given to Starboard Renderer for direct usage.
bool AppendSettingToSwitch(const std::string& setting_name,
                           const H5vccSettingValue& setting_value) {
  auto it = kH5vccSettingToSwitchMap.find(setting_name);
  if (it == kH5vccSettingToSwitchMap.end()) {
    return false;
  }
  std::string switch_name = it->second;
  std::string setting_str;
  if (auto* val = std::get_if<std::string>(&setting_value)) {
    setting_str = *val;
  } else if (auto* val = std::get_if<int64_t>(&setting_value)) {
    setting_str = base::NumberToString(*val);
  } else {
    LOG(WARNING) << "Attempted to apply switch " << switch_name
                 << " but the setting value was not an integer or string.";
    return false;
  }
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(switch_name,
                                                            setting_str);
  LOG(INFO) << "Applied command line switch: " << switch_name << " = "
            << setting_str;
  return true;
}

std::map<std::string, H5vccSettingValue> ParseH5vccSettings(
    cobalt::mojom::SettingsPtr settings) {
  std::map<std::string, H5vccSettingValue> h5vcc_settings;
  for (auto& [key, value] : settings->settings) {
    if (value->is_string_value()) {
      h5vcc_settings.emplace(key, std::move(value->get_string_value()));
    } else if (value->is_int_value()) {
      h5vcc_settings.emplace(key, value->get_int_value());
    } else {
      NOTREACHED();
    }
  }
  return h5vcc_settings;
}

template <typename T>
const T* GetSettingValue(
    const std::map<std::string, H5vccSettingValue>& settings,
    const std::string& key) {
  auto it = settings.find(key);
  if (it == settings.end()) {
    return nullptr;
  }
  return std::get_if<T>(&it->second);
}

ParsedH5vccSettings ProcessH5vccSettings(
    const std::map<std::string, H5vccSettingValue>& settings) {
  ParsedH5vccSettings parsed;
  if (auto* val = GetSettingValue<int64_t>(
          settings, kH5vccSettingsKeyMediaEnableAllocateOnDemand)) {
    bool enable_allocate_on_demand = *val != 0;
    media::DecoderBuffer::EnableAllocateOnDemand(enable_allocate_on_demand);
  }
  if (auto* val = GetSettingValue<int64_t>(
          settings, kH5vccSettingsKeyMediaEnableFlushDuringSeek)) {
    parsed.enable_flush_during_seek = *val != 0;
  }
  if (auto* val = GetSettingValue<int64_t>(
          settings, kH5vccSettingsKeyMediaEnableResetAudioDecoder)) {
    parsed.enable_reset_audio_decoder = *val != 0;
  }

  for (const auto& [setting_name, setting_value] : settings) {
    AppendSettingToSwitch(setting_name, setting_value);
  }
  return parsed;
}

}  // namespace

static_assert(std::is_same<::media::BindHostReceiverCallback,
                           base::RepeatingCallback<
                               decltype(BindHostReceiverWithValuation)>>::value,
              "These two types must be the same");

CobaltContentRendererClient::CobaltContentRendererClient() {
  DETACH_FROM_THREAD(thread_checker_);
}

CobaltContentRendererClient::~CobaltContentRendererClient() = default;

void CobaltContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  new js_injection::JsCommunication(render_frame);
  new CobaltRenderFrameObserver(render_frame);

  if (!h5vcc_settings_remote_.is_bound()) {
    content::RenderThread::Get()->BindHostReceiver(
        h5vcc_settings_remote_.BindNewPipeAndPassReceiver());
  }

  if (render_frame->GetWebView()) {
    viewport_size_ =
        gfx::ToCeiledSize(render_frame->GetWebView()->VisualViewportSize());
  } else {
    LOG(WARNING) << "RenderFrameCreated is called with no webview.";
  }
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ::media::KeySystemInfos key_systems;
#if BUILDFLAG(IS_ANDROID)
  AddStarboardCmaKeySystems(&key_systems);
  std::move(cb).Run(std::move(key_systems));
#endif
}

bool CobaltContentRendererClient::IsSupportedAudioType(
    const ::media::AudioType& type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);
  communication->RunScriptsAtDocumentStart();
}

void CobaltContentRendererClient::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  BindHostReceiverWithValuation(std::move(receiver));
}

void CobaltContentRendererClient::GetStarboardRendererFactoryTraits(
    media::RendererFactoryTraits* renderer_factory_traits) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(b/383327725) - Cobalt: Inject these values from the web app.
  renderer_factory_traits->audio_write_duration_local =
      base::Microseconds(kSbPlayerWriteDurationLocal);
  renderer_factory_traits->audio_write_duration_remote =
      base::Microseconds(kSbPlayerWriteDurationRemote);

  if (!h5vcc_settings_remote_.is_bound()) {
    content::RenderThread::Get()->BindHostReceiver(
        h5vcc_settings_remote_.BindNewPipeAndPassReceiver());
  }

  cobalt::mojom::SettingsPtr settings;
  ParsedH5vccSettings parsed;
  if (h5vcc_settings_remote_->GetSettings(&settings) && settings) {
    auto h5vcc_settings = ParseH5vccSettings(std::move(settings));
    ProcessH5vccSettings(h5vcc_settings);
  }
  // TODO: b/474454335 - Remove once experiments are done.
  renderer_factory_traits->enable_flush_during_seek =
      parsed.enable_flush_during_seek;
  renderer_factory_traits->enable_reset_audio_decoder =
      parsed.enable_reset_audio_decoder;

  renderer_factory_traits->viewport_size = viewport_size_;
  // TODO(b/405424096) - Cobalt: Move VideoGeometrySetterService to Gpu thread.
  renderer_factory_traits->bind_host_receiver_callback =
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&CobaltContentRendererClient::BindHostReceiver,
                              weak_factory_.GetWeakPtr()));
}

void CobaltContentRendererClient::PostSandboxInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Register the current thread (which is the InProcessRendererThread in
  // single- process mode) for hang watching. Store the ScopedClosureRunner to
  // keep the registration active until this client object is destroyed.
  if (base::HangWatcher::IsEnabled() && base::HangWatcher::GetInstance()) {
    // Use kRendererThread as the type for this in-process renderer thread.
    unregister_thread_closure = base::HangWatcher::RegisterThread(
        base::HangWatcher::ThreadType::kRendererThread);
  }
}

}  // namespace cobalt
