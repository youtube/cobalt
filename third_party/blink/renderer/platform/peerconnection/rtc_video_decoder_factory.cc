// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_factory.h"

#include <array>
#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_util.h"
#include "media/base/platform_features.h"
#include "media/base/video_codecs.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_stream_adapter.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/webrtc/api/video_codecs/h264_profile_level_id.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"
#include "third_party/webrtc/media/base/codec.h"
#include "third_party/webrtc/media/base/media_constants.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

// The default fps and default size are used when querying gpu_factories_ to see
// if a codec profile is supported. 1280x720 at 30 fps corresponds to level 3.1
// for both VP9 and H264. This matches the maximum H264 profile level that is
// returned by the internal software decoder.
// TODO(crbug.com/1213437): Query gpu_factories_ or decoder_factory_ to
// determine the maximum resolution and frame rate.
const int kDefaultFps = 30;
const gfx::Size kDefaultSize(1280, 720);

struct CodecConfig {
  media::VideoCodec codec;
  media::VideoCodecProfile profile;
};

constexpr std::array<CodecConfig, 9> kCodecConfigs = {{
    {media::VideoCodec::kVP8, media::VP8PROFILE_ANY},
    {media::VideoCodec::kVP9, media::VP9PROFILE_PROFILE0},
    {media::VideoCodec::kVP9, media::VP9PROFILE_PROFILE1},
    {media::VideoCodec::kVP9, media::VP9PROFILE_PROFILE2},
    {media::VideoCodec::kH264, media::H264PROFILE_BASELINE},
    {media::VideoCodec::kH264, media::H264PROFILE_MAIN},
    {media::VideoCodec::kH264, media::H264PROFILE_HIGH},
    {media::VideoCodec::kH264, media::H264PROFILE_HIGH444PREDICTIVEPROFILE},
    {media::VideoCodec::kAV1, media::AV1PROFILE_PROFILE_MAIN},
}};

// Translate from media::VideoDecoderConfig to webrtc::SdpVideoFormat, or return
// nothing if the profile isn't supported.
absl::optional<webrtc::SdpVideoFormat> VdcToWebRtcFormat(
    const media::VideoDecoderConfig& config) {
  switch (config.codec()) {
    case media::VideoCodec::kAV1:
      return webrtc::SdpVideoFormat(cricket::kAv1CodecName);
    case media::VideoCodec::kVP8:
      return webrtc::SdpVideoFormat(cricket::kVp8CodecName);
    case media::VideoCodec::kVP9: {
      webrtc::VP9Profile vp9_profile;
      switch (config.profile()) {
        case media::VP9PROFILE_PROFILE0:
          vp9_profile = webrtc::VP9Profile::kProfile0;
          break;
        case media::VP9PROFILE_PROFILE1:
          vp9_profile = webrtc::VP9Profile::kProfile1;
          break;
        case media::VP9PROFILE_PROFILE2:
          vp9_profile = webrtc::VP9Profile::kProfile2;
          break;
        default:
          // Unsupported profile in WebRTC.
          return absl::nullopt;
      }
      return webrtc::SdpVideoFormat(
          cricket::kVp9CodecName, {{webrtc::kVP9FmtpProfileId,
                                    webrtc::VP9ProfileToString(vp9_profile)}});
    }
    case media::VideoCodec::kH264: {
      webrtc::H264Profile h264_profile;
      switch (config.profile()) {
        case media::H264PROFILE_BASELINE:
          h264_profile = webrtc::H264Profile::kProfileBaseline;
          break;
        case media::H264PROFILE_MAIN:
          h264_profile = webrtc::H264Profile::kProfileMain;
          break;
        case media::H264PROFILE_HIGH:
          h264_profile = webrtc::H264Profile::kProfileHigh;
          break;
        case media::H264PROFILE_HIGH444PREDICTIVEPROFILE:
          h264_profile = webrtc::H264Profile::kProfilePredictiveHigh444;
          break;
        default:
          // Unsupported H264 profile in WebRTC.
          return absl::nullopt;
      }

      const int width = config.visible_rect().width();
      const int height = config.visible_rect().height();

      const absl::optional<webrtc::H264Level> h264_level =
          webrtc::H264SupportedLevel(width * height, kDefaultFps);
      const webrtc::H264ProfileLevelId profile_level_id(
          h264_profile, h264_level.value_or(webrtc::H264Level::kLevel1));

      webrtc::SdpVideoFormat format(cricket::kH264CodecName);
      format.parameters = {
          {cricket::kH264FmtpProfileLevelId,
           *webrtc::H264ProfileLevelIdToString(profile_level_id)},
          {cricket::kH264FmtpLevelAsymmetryAllowed, "1"}};
      return format;
    }
    default:
      return absl::nullopt;
  }
}

// This extra indirection is needed so that we can delete the decoder on the
// correct thread.
class ScopedVideoDecoder : public webrtc::VideoDecoder {
 public:
  ScopedVideoDecoder(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      std::unique_ptr<webrtc::VideoDecoder> decoder)
      : task_runner_(task_runner), decoder_(std::move(decoder)) {}

  bool Configure(const Settings& settings) override {
    return decoder_->Configure(settings);
  }
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    return decoder_->RegisterDecodeCompleteCallback(callback);
  }
  int32_t Release() override { return decoder_->Release(); }
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    return decoder_->Decode(input_image, missing_frames, render_time_ms);
  }

  DecoderInfo GetDecoderInfo() const override {
    return decoder_->GetDecoderInfo();
  }

  // Runs on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  ~ScopedVideoDecoder() override {
    task_runner_->DeleteSoon(FROM_HERE, decoder_.release());
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<webrtc::VideoDecoder> decoder_;
};

}  // namespace

RTCVideoDecoderFactory::RTCVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    base::WeakPtr<media::DecoderFactory> decoder_factory,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const gfx::ColorSpace& render_color_space)
    : gpu_factories_(gpu_factories),
      decoder_factory_(decoder_factory),
      media_task_runner_(std::move(media_task_runner)),
      render_color_space_(render_color_space) {
  if (gpu_factories_) {
    gpu_codec_support_waiter_ =
        std::make_unique<GpuCodecSupportWaiter>(gpu_factories_);
  }
  DVLOG(2) << __func__;
}

void RTCVideoDecoderFactory::CheckAndWaitDecoderSupportStatusIfNeeded() const {
  if (!gpu_codec_support_waiter_)
    return;

  if (!gpu_codec_support_waiter_->IsDecoderSupportKnown()) {
    DLOG(WARNING) << "Decoder support is unknown. Timeout "
                  << gpu_codec_support_waiter_->wait_timeout_ms()
                         .value_or(base::TimeDelta())
                         .InMilliseconds()
                  << "ms. Decoders might not be available.";
  }
}

std::vector<webrtc::SdpVideoFormat>
RTCVideoDecoderFactory::GetSupportedFormats() const {
  CheckAndWaitDecoderSupportStatusIfNeeded();

  // For now, ignore `kUseDecoderStreamForWebRTC`, and advertise support only
  // for hardware-accelerated formats.  For some codecs, like AV1, which don't
  // have an equivalent in rtc, we might want to include them anyway.
  std::vector<webrtc::SdpVideoFormat> supported_formats;
  for (auto& codec_config : kCodecConfigs) {
    media::VideoDecoderConfig config(
        codec_config.codec, codec_config.profile,
        media::VideoDecoderConfig::AlphaMode::kIsOpaque,
        media::VideoColorSpace(), media::kNoTransformation, kDefaultSize,
        gfx::Rect(kDefaultSize), kDefaultSize, media::EmptyExtraData(),
        media::EncryptionScheme::kUnencrypted);
    config.set_is_rtc(true);
    absl::optional<webrtc::SdpVideoFormat> format;

    // The RTCVideoDecoderAdapter is for HW decoders only, so ignore it if there
    // are no gpu_factories_.
    if (gpu_factories_ &&
        gpu_factories_->IsDecoderConfigSupported(config) ==
            media::GpuVideoAcceleratorFactories::Supported::kTrue) {
      format = VdcToWebRtcFormat(config);
    }

    // TODO(https://crbug.com/1274904): Temporarily do not add configs from
    // `decoder_factory_`, so that supported configs are identical.
    // See:
    // https://chromium-review.googlesource.com/c/chromium/src/+/3305493
    // For the exact diff.

    if (format) {
      // For H.264 decoder, packetization-mode 0/1 should be both supported.
      media::VideoCodec codec = WebRtcToMediaVideoCodec(
          webrtc::PayloadStringToCodecType(format->name));
      if (codec == media::VideoCodec::kH264) {
        const std::array<std::string, 2> kH264PacketizationModes = {{"1", "0"}};
        for (const auto& mode : kH264PacketizationModes) {
          webrtc::SdpVideoFormat h264_format = *format;
          h264_format.parameters[cricket::kH264FmtpPacketizationMode] = mode;
          supported_formats.push_back(h264_format);
        }
      } else {
        supported_formats.push_back(*format);
      }
    }
  }

  // Due to https://crbug.com/345569, HW decoders do not distinguish between
  // Constrained Baseline(CBP) and Baseline(BP) profiles. Since CBP is a subset
  // of BP, we can report support for both. It is safe to do so when SW fallback
  // is available.
  // TODO(emircan): Remove this when the bug referred above is fixed.
  cricket::AddH264ConstrainedBaselineProfileToSupportedFormats(
      &supported_formats);
  return supported_formats;
}

webrtc::VideoDecoderFactory::CodecSupport
RTCVideoDecoderFactory::QueryCodecSupport(const webrtc::SdpVideoFormat& format,
                                          bool reference_scaling) const {
  CheckAndWaitDecoderSupportStatusIfNeeded();

  media::VideoCodec codec =
      WebRtcToMediaVideoCodec(webrtc::PayloadStringToCodecType(format.name));
  if (reference_scaling) {
    // Check that the configuration is valid (e.g., H264 doesn't support SVC at
    // all and VP8 doesn't support spatial layers).
    if (codec != media::VideoCodec::kVP9 && codec != media::VideoCodec::kAV1) {
      // Invalid reference_scaling, return unsupported.
      return {false, false};
    }
    // Most HW decoders cannot handle reference scaling/spatial layers, so
    // return false if the configuration requires reference scaling unless we
    // explicitly know that the HW decoder can handle this.
    if (codec == media::VideoCodec::kVP9 &&
        !media::IsVp9kSVCHWDecodingEnabled()) {
      return {false, false};
    }
  }

  media::VideoCodecProfile codec_profile =
      WebRtcVideoFormatToMediaVideoCodecProfile(format);
  media::VideoDecoderConfig config(
      codec, codec_profile, media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      media::VideoColorSpace(), media::kNoTransformation, kDefaultSize,
      gfx::Rect(kDefaultSize), kDefaultSize, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);

  webrtc::VideoDecoderFactory::CodecSupport codec_support;
  // Check gpu_factories for powerEfficient.
  if (gpu_factories_) {
    if (gpu_factories_->IsDecoderConfigSupported(config) ==
        media::GpuVideoAcceleratorFactories::Supported::kTrue) {
      codec_support.is_power_efficient = true;
    }
  }

  // The codec must be supported if it's power efficient.
  codec_support.is_supported = codec_support.is_power_efficient;

  // TODO(https://crbug.com/1274904): Temporarily do not check
  // `decoder_factory_` for additional support, to make codec
  // DecoderAdapter and DecoderStreamAdapter the same.
  // See:
  // https://chromium-review.googlesource.com/c/chromium/src/+/3305493
  // For the exact diff.
  // Please note that `decoder_factory_` may be a null pointer when this
  // function is called from the MediaCapabilities API, see
  // https://crbug.com/1349423.

  return codec_support;
}

RTCVideoDecoderFactory::~RTCVideoDecoderFactory() {
  DVLOG(2) << __func__;
}

std::unique_ptr<webrtc::VideoDecoder>
RTCVideoDecoderFactory::CreateVideoDecoder(
    const webrtc::SdpVideoFormat& format) {
  TRACE_EVENT0("webrtc", "RTCVideoDecoderFactory::CreateVideoDecoder");
  DVLOG(2) << __func__;
  CheckAndWaitDecoderSupportStatusIfNeeded();

  std::unique_ptr<webrtc::VideoDecoder> decoder;
  if (base::FeatureList::IsEnabled(media::kUseDecoderStreamForWebRTC)) {
    decoder = RTCVideoDecoderStreamAdapter::Create(
        gpu_factories_, decoder_factory_, media_task_runner_,
        render_color_space_, format);
  } else {
    decoder = RTCVideoDecoderAdapter::Create(gpu_factories_, format);
  }
  // ScopedVideoDecoder uses the task runner to make sure the decoder is
  // destructed on the correct thread.
  return decoder ? std::make_unique<ScopedVideoDecoder>(
                       base::SequencedTaskRunner::GetCurrentDefault(),
                       std::move(decoder))
                 : nullptr;
}

}  // namespace blink
