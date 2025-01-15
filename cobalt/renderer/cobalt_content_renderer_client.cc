// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/renderer/cobalt_content_renderer_client.h"

#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "cobalt/common/shell_switches.h"
#include "components/cdm/renderer/external_clear_key_key_system_info.h"
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/pseudonymization_util.h"
#include "content/public/common/web_identity.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/test/test_service.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/policy/sandbox.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/web/modules/credentialmanagement/throttle_helper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/v8.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include "components/cdm/renderer/widevine_key_system_info.h"
#include "starboard/media.h"
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "ppapi/shared_impl/ppapi_switches.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "base/feature_list.h"
#include "media/base/media_switches.h"
#endif

namespace cobalt {

namespace {

class CobaltContentRendererUrlLoaderThrottleProvider
    : public blink::URLLoaderThrottleProvider {
 public:
  std::unique_ptr<URLLoaderThrottleProvider> Clone() override {
    return std::make_unique<CobaltContentRendererUrlLoaderThrottleProvider>();
  }

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request) override {
    blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    // Workers can call us on a background thread. We don't care about such
    // requests because we purposefully only look at resources from frames
    // that the user can interact with.`
    content::RenderFrame* frame =
        content::RenderThread::IsMainThread()
            ? content::RenderFrame::FromRoutingID(render_frame_id)
            : nullptr;
    if (frame) {
      auto throttle = content::MaybeCreateIdentityUrlLoaderThrottle(
          base::BindRepeating(blink::SetIdpSigninStatus, frame->GetWebFrame()));
      if (throttle) {
        throttles.push_back(std::move(throttle));
      }
    }

    return throttles;
  }

  void SetOnline(bool is_online) override {}
};

}  // namespace

CobaltContentRendererClient::CobaltContentRendererClient() {}
CobaltContentRendererClient::~CobaltContentRendererClient() {}

void CobaltContentRendererClient::RenderThreadStarted() {
  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();
}

void CobaltContentRendererClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  binders->Add<web_cache::mojom::WebCache>(
      base::BindRepeating(&web_cache::WebCacheImpl::BindReceiver,
                          base::Unretained(web_cache_impl_.get())),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void CobaltContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {}

void CobaltContentRendererClient::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  if (error_html && error_html->empty()) {
    *error_html =
        "<head><title>Error</title></head><body>Could not load the requested "
        "resource.<br/>Error code: " +
        base::NumberToString(error.reason()) +
        (error.reason() < 0 ? " (" + net::ErrorToString(error.reason()) + ")"
                            : "") +
        "</body>";
  }
}

void CobaltContentRendererClient::PrepareErrorPageForHttpStatusError(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    int http_status,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  if (error_html) {
    *error_html =
        "<head><title>Error</title></head><body>Server returned HTTP status " +
        base::NumberToString(http_status) + "</body>";
  }
}

void CobaltContentRendererClient::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    blink::WebTestingSupport::InjectInternalsObject(context);
  }
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
CobaltContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<CobaltContentRendererUrlLoaderThrottleProvider>();
}

#if BUILDFLAG(USE_STARBOARD_MEDIA)
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

#else  // BUILDFLAG(USE_STARBOARD_MEDIA)

#if BUILDFLAG(ENABLE_MOJO_CDM)
void CobaltContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  media::KeySystemInfos key_systems;
  if (base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting)) {
    key_systems.push_back(
        std::make_unique<cdm::ExternalClearKeyKeySystemInfo>());
  }
  std::move(cb).Run(std::move(key_systems));
}
#endif

#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

#if BUILDFLAG(USE_STARBOARD_MEDIA)
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
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

std::unique_ptr<blink::WebPrescientNetworking>
CobaltContentRendererClient::CreatePrescientNetworking(
    content::RenderFrame* render_frame) {
  return std::make_unique<network_hints::WebPrescientNetworkingImpl>(
      render_frame);
}

}  // namespace cobalt
