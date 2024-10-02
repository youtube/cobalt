// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include <memory>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/media_buildflags.h"
#include "media/muxers/webm_muxer.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/vpx_video_encoder.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/modules/mediarecorder/buildflags.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_encoder_wrapper.h"
#include "third_party/blink/renderer/modules/mediarecorder/vea_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/vpx_encoder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(RTC_USE_H264)
#include "media/video/openh264_video_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/h264_encoder.h"
#endif  // #if BUILDFLAG(RTC_USE_H264)

#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/video/av1_video_encoder.h"
#endif  // BUILDFLAG(ENABLE_LIBAOM)

using video_track_recorder::kVEAEncoderMinResolutionHeight;
using video_track_recorder::kVEAEncoderMinResolutionWidth;

namespace WTF {
template <>
struct CrossThreadCopier<std::vector<scoped_refptr<media::VideoFrame>>>
    : public CrossThreadCopierPassThrough<
          std::vector<scoped_refptr<media::VideoFrame>>> {
  STATIC_ONLY(CrossThreadCopier);
};
}  // namespace WTF

namespace blink {

// Helper class used to bless annotation of our calls to
// CreateOffscreenGraphicsContext3DProvider using ScopedAllowBaseSyncPrimitives.
class VideoTrackRecorderImplContextProvider {
 public:
  static std::unique_ptr<WebGraphicsContext3DProvider>
  CreateOffscreenGraphicsContext(Platform::ContextAttributes context_attributes,
                                 Platform::GraphicsInfo* gl_info,
                                 const KURL& url) {
    base::ScopedAllowBaseSyncPrimitives allow;
    return CreateOffscreenGraphicsContext3DProvider(context_attributes, gl_info,
                                                    url);
  }
};

using CodecId = VideoTrackRecorder::CodecId;

libyuv::RotationMode MediaVideoRotationToRotationMode(
    media::VideoRotation rotation) {
  switch (rotation) {
    case media::VIDEO_ROTATION_0:
      return libyuv::kRotate0;
    case media::VIDEO_ROTATION_90:
      return libyuv::kRotate90;
    case media::VIDEO_ROTATION_180:
      return libyuv::kRotate180;
    case media::VIDEO_ROTATION_270:
      return libyuv::kRotate270;
  }
  NOTREACHED() << rotation;
  return libyuv::kRotate0;
}

namespace {

static const struct {
  CodecId codec_id;
  media::VideoCodecProfile min_profile;
  media::VideoCodecProfile max_profile;
} kPreferredCodecIdAndVEAProfiles[] = {
    {CodecId::kVp8, media::VP8PROFILE_MIN, media::VP8PROFILE_MAX},
    {CodecId::kVp9, media::VP9PROFILE_MIN, media::VP9PROFILE_MAX},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {CodecId::kH264, media::H264PROFILE_MIN, media::H264PROFILE_MAX},
#endif
    {CodecId::kAv1, media::AV1PROFILE_MIN, media::AV1PROFILE_MAX},
};

static_assert(std::size(kPreferredCodecIdAndVEAProfiles) ==
                  static_cast<int>(CodecId::kLast),
              "|kPreferredCodecIdAndVEAProfiles| should consider all CodecIds");

// The maximum number of frames that we keep the reference alive for encode.
// This guarantees that there is limit on the number of frames in a FIFO queue
// that are being encoded and frames coming after this limit is reached are
// dropped.
// TODO(emircan): Make this a LIFO queue that has different sizes for each
// encoder implementation.
const int kMaxNumberOfFramesInEncode = 10;

void NotifyEncoderSupportKnown(base::OnceClosure callback) {
  if (!Platform::Current()) {
    DLOG(ERROR) << "Couldn't access the render thread";
    std::move(callback).Run();
    return;
  }

  media::GpuVideoAcceleratorFactories* const gpu_factories =
      Platform::Current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoEncodeAcceleratorEnabled()) {
    DLOG(ERROR) << "Couldn't initialize GpuVideoAcceleratorFactories";
    std::move(callback).Run();
    return;
  }

  gpu_factories->NotifyEncoderSupportKnown(std::move(callback));
}

// Obtains video encode accelerator's supported profiles.
media::VideoEncodeAccelerator::SupportedProfiles GetVEASupportedProfiles() {
  if (!Platform::Current()) {
    DLOG(ERROR) << "Couldn't access the render thread";
    return media::VideoEncodeAccelerator::SupportedProfiles();
  }

  media::GpuVideoAcceleratorFactories* const gpu_factories =
      Platform::Current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoEncodeAcceleratorEnabled()) {
    DLOG(ERROR) << "Couldn't initialize GpuVideoAcceleratorFactories";
    return media::VideoEncodeAccelerator::SupportedProfiles();
  }
  return gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
      media::VideoEncodeAccelerator::SupportedProfiles());
}

VideoTrackRecorderImpl::CodecEnumerator* GetCodecEnumerator() {
  static VideoTrackRecorderImpl::CodecEnumerator* enumerator =
      new VideoTrackRecorderImpl::CodecEnumerator(GetVEASupportedProfiles());
  return enumerator;
}

void UmaHistogramForCodec(bool uses_acceleration, CodecId codec_id) {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // (kMaxValue being the only exception, as it does not map to a logged value,
  // and should be renumbered as new values are inserted.)
  enum class VideoTrackRecorderCodecHistogram : uint8_t {
    kUnknown = 0,
    kVp8Sw = 1,
    kVp8Hw = 2,
    kVp9Sw = 3,
    kVp9Hw = 4,
    kH264Sw = 5,
    kH264Hw = 6,
    kAv1Sw = 7,
    kAv1Hw = 8,
    kMaxValue = kAv1Hw,
  };
  auto histogram = VideoTrackRecorderCodecHistogram::kUnknown;
  if (uses_acceleration) {
    switch (codec_id) {
      case CodecId::kVp8:
        histogram = VideoTrackRecorderCodecHistogram::kVp8Hw;
        break;
      case CodecId::kVp9:
        histogram = VideoTrackRecorderCodecHistogram::kVp9Hw;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case CodecId::kH264:
        histogram = VideoTrackRecorderCodecHistogram::kH264Hw;
        break;
#endif
      case CodecId::kAv1:
        histogram = VideoTrackRecorderCodecHistogram::kAv1Hw;
        break;
      case CodecId::kLast:
        break;
    }
  } else {
    switch (codec_id) {
      case CodecId::kVp8:
        histogram = VideoTrackRecorderCodecHistogram::kVp8Sw;
        break;
      case CodecId::kVp9:
        histogram = VideoTrackRecorderCodecHistogram::kVp9Sw;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case CodecId::kH264:
        histogram = VideoTrackRecorderCodecHistogram::kH264Sw;
        break;
#endif
      case CodecId::kAv1:
        histogram = VideoTrackRecorderCodecHistogram::kAv1Sw;
        break;
      case CodecId::kLast:
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Media.MediaRecorder.Codec", histogram);
}

bool MustUseVEA(CodecId codec_id) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && !BUILDFLAG(RTC_USE_H264)
  return codec_id == CodecId::kH264;
#else
  return false;
#endif
}

#if BUILDFLAG(ENABLE_LIBAOM)
MediaRecorderEncoderWrapper::CreateEncoderCB
GetCreateSoftwareVideoEncoderCallback(CodecId codec_id) {
  switch (codec_id) {
#if BUILDFLAG(RTC_USE_H264)
    case CodecId::kH264:
      return ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          []() -> std::unique_ptr<media::VideoEncoder> {
            return std::make_unique<media::OpenH264VideoEncoder>();
          }));
#endif  // BUILDFLAG(RTC_USE_H264)
    case CodecId::kVp8:
    case CodecId::kVp9:
      return ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          []() -> std::unique_ptr<media::VideoEncoder> {
            return std::make_unique<media::VpxVideoEncoder>();
          }));
#if BUILDFLAG(ENABLE_LIBAOM)
    case CodecId::kAv1:
      return ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          []() -> std::unique_ptr<media::VideoEncoder> {
            return std::make_unique<media::Av1VideoEncoder>();
          }));
#endif  // BUILDFLAG(ENABLE_LIBAOM)
    default:
      NOTREACHED() << "Unsupported codec=" << static_cast<int>(codec_id);
      return base::NullCallback();
  }
}
#endif  // BUILDFLAG(ENABLE_LIBAOM)
}  // anonymous namespace

VideoTrackRecorder::VideoTrackRecorder(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    base::OnceClosure on_track_source_ended_cb)
    : TrackRecorder(std::move(on_track_source_ended_cb)),
      main_thread_task_runner_(std::move(main_thread_task_runner)) {}

VideoTrackRecorderImpl::CodecProfile::CodecProfile(CodecId codec_id)
    : codec_id(codec_id) {}

VideoTrackRecorderImpl::CodecProfile::CodecProfile(
    CodecId codec_id,
    absl::optional<media::VideoCodecProfile> opt_profile,
    absl::optional<uint8_t> opt_level)
    : codec_id(codec_id), profile(opt_profile), level(opt_level) {}

VideoTrackRecorderImpl::CodecProfile::CodecProfile(
    CodecId codec_id,
    media::VideoCodecProfile profile,
    uint8_t level)
    : codec_id(codec_id), profile(profile), level(level) {}

VideoTrackRecorderImpl::CodecEnumerator::CodecEnumerator(
    const media::VideoEncodeAccelerator::SupportedProfiles&
        vea_supported_profiles) {
  for (const auto& supported_profile : vea_supported_profiles) {
    const media::VideoCodecProfile codec = supported_profile.profile;
    for (auto& codec_id_and_profile : kPreferredCodecIdAndVEAProfiles) {
      if (codec >= codec_id_and_profile.min_profile &&
          codec <= codec_id_and_profile.max_profile) {
        DVLOG(2) << "Accelerated codec found: " << media::GetProfileName(codec)
                 << ", min_resolution: "
                 << supported_profile.min_resolution.ToString()
                 << ", max_resolution: "
                 << supported_profile.max_resolution.ToString()
                 << ", max_framerate: "
                 << supported_profile.max_framerate_numerator << "/"
                 << supported_profile.max_framerate_denominator;
        auto iter = supported_profiles_.find(codec_id_and_profile.codec_id);
        if (iter == supported_profiles_.end()) {
          auto result = supported_profiles_.insert(
              codec_id_and_profile.codec_id,
              media::VideoEncodeAccelerator::SupportedProfiles());
          result.stored_value->value.push_back(supported_profile);
        } else {
          iter->value.push_back(supported_profile);
        }
        if (preferred_codec_id_ == CodecId::kLast)
          preferred_codec_id_ = codec_id_and_profile.codec_id;
      }
    }
  }
}

VideoTrackRecorderImpl::CodecEnumerator::~CodecEnumerator() = default;

std::pair<media::VideoCodecProfile, bool>
VideoTrackRecorderImpl::CodecEnumerator::FindSupportedVideoCodecProfile(
    CodecId codec,
    media::VideoCodecProfile profile) const {
  const auto profiles = supported_profiles_.find(codec);
  if (profiles == supported_profiles_.end()) {
    return {media::VIDEO_CODEC_PROFILE_UNKNOWN, false};
  }
  for (const auto& p : profiles->value) {
    if (p.profile == profile) {
      const bool vbr_support =
          p.rate_control_modes & media::VideoEncodeAccelerator::kVariableMode;
      return {profile, vbr_support};
    }
  }
  return {media::VIDEO_CODEC_PROFILE_UNKNOWN, false};
}

VideoTrackRecorderImpl::CodecId
VideoTrackRecorderImpl::CodecEnumerator::GetPreferredCodecId() const {
  if (preferred_codec_id_ == CodecId::kLast)
    return CodecId::kVp8;

  return preferred_codec_id_;
}

std::pair<media::VideoCodecProfile, bool>
VideoTrackRecorderImpl::CodecEnumerator::GetFirstSupportedVideoCodecProfile(
    CodecId codec) const {
  const auto profile = supported_profiles_.find(codec);
  if (profile == supported_profiles_.end())
    return {media::VIDEO_CODEC_PROFILE_UNKNOWN, false};

  const auto& supported_profile = profile->value.front();
  const bool vbr_support = supported_profile.rate_control_modes &
                           media::VideoEncodeAccelerator::kVariableMode;
  return {supported_profile.profile, vbr_support};
}

media::VideoEncodeAccelerator::SupportedProfiles
VideoTrackRecorderImpl::CodecEnumerator::GetSupportedProfiles(
    CodecId codec) const {
  const auto profile = supported_profiles_.find(codec);
  return profile == supported_profiles_.end()
             ? media::VideoEncodeAccelerator::SupportedProfiles()
             : profile->value;
}

VideoTrackRecorderImpl::Counter::Counter() : count_(0u) {}

VideoTrackRecorderImpl::Counter::~Counter() = default;

void VideoTrackRecorderImpl::Counter::IncreaseCount() {
  count_++;
}

void VideoTrackRecorderImpl::Counter::DecreaseCount() {
  count_--;
}

base::WeakPtr<VideoTrackRecorderImpl::Counter>
VideoTrackRecorderImpl::Counter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

VideoTrackRecorderImpl::Encoder::Encoder(
    scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
    const OnEncodedVideoCB& on_encoded_video_cb,
    uint32_t bits_per_second)
    : encoding_task_runner_(std::move(encoding_task_runner)),
      on_encoded_video_cb_(on_encoded_video_cb),
      bits_per_second_(bits_per_second),
      num_frames_in_encode_(
          std::make_unique<VideoTrackRecorderImpl::Counter>()) {
  CHECK(encoding_task_runner_);
  DCHECK(!on_encoded_video_cb_.is_null());
}

VideoTrackRecorderImpl::Encoder::~Encoder() = default;

void VideoTrackRecorderImpl::Encoder::Initialize() {}

void VideoTrackRecorderImpl::Encoder::StartFrameEncode(
    scoped_refptr<media::VideoFrame> video_frame,
    std::vector<scoped_refptr<media::VideoFrame>> /*scaled_video_frames*/,
    base::TimeTicks capture_timestamp) {
  DVLOG(3) << __func__;
  if (paused_)
    return;

  if (num_frames_in_encode_->count() > kMaxNumberOfFramesInEncode) {
    DLOG(WARNING) << "Too many frames are queued up. Dropping this one.";
    return;
  }

  scoped_refptr<media::VideoFrame> frame = video_frame;
  const bool is_format_supported =
      (video_frame->format() == media::PIXEL_FORMAT_NV12 &&
       video_frame->HasGpuMemoryBuffer()) ||
      (video_frame->IsMappable() &&
       (video_frame->format() == media::PIXEL_FORMAT_I420 ||
        video_frame->format() == media::PIXEL_FORMAT_I420A));
  if (!is_format_supported) {
    frame = MaybeProvideEncodableFrame(video_frame);
  } else if (!video_frame->HasGpuMemoryBuffer()) {
    // Drop alpha channel if the encoder does not support it yet.
    if (!CanEncodeAlphaChannel() &&
        video_frame->format() == media::PIXEL_FORMAT_I420A) {
      frame = media::WrapAsI420VideoFrame(video_frame);
    } else {
      frame = media::VideoFrame::WrapVideoFrame(
          video_frame, video_frame->format(), video_frame->visible_rect(),
          video_frame->natural_size());
    }
  }
  if (!frame) {
    // Explicit reasons for the frame drop are already logged.
    return;
  }
  frame->AddDestructionObserver(base::BindPostTask(
      encoding_task_runner_,
      WTF::BindOnce(&VideoTrackRecorderImpl::Counter::DecreaseCount,
                    num_frames_in_encode_->GetWeakPtr())));
  num_frames_in_encode_->IncreaseCount();
  EncodeFrame(std::move(frame), capture_timestamp);
}

scoped_refptr<media::VideoFrame>
VideoTrackRecorderImpl::Encoder::MaybeProvideEncodableFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  DVLOG(3) << __func__;
  scoped_refptr<media::VideoFrame> frame;
  const bool is_opaque = media::IsOpaque(video_frame->format());
  if (media::IsRGB(video_frame->format()) && video_frame->IsMappable()) {
    // It's a mapped RGB frame, no readback needed,
    // all we need is to convert RGB to I420
    auto visible_rect = video_frame->visible_rect();
    frame = frame_pool_.CreateFrame(
        is_opaque ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
        visible_rect.size(), visible_rect, visible_rect.size(),
        video_frame->timestamp());

    if (!frame ||
        !media::ConvertAndScaleFrame(*video_frame, *frame, resize_buffer_)
             .is_ok()) {
      // Send black frames (yuv = {0, 127, 127}).
      DLOG(ERROR) << "Can't convert RGB to I420";
      frame = media::VideoFrame::CreateColorFrame(
          video_frame->visible_rect().size(), 0u, 0x80, 0x80,
          video_frame->timestamp());
    }
    return frame;
  }

  // |encoder_thread_context_| is null if the GPU process has crashed or isn't
  // there
  if (!encoder_thread_context_) {
    // PaintCanvasVideoRenderer requires these settings to work.
    Platform::ContextAttributes attributes;
    attributes.enable_raster_interface = true;
    attributes.prefer_low_power_gpu = true;

    // TODO(crbug.com/1240756): This line can be removed once OOPR-Canvas has
    // shipped on all platforms
    attributes.support_grcontext = true;

    Platform::GraphicsInfo info;
    encoder_thread_context_ =
        VideoTrackRecorderImplContextProvider::CreateOffscreenGraphicsContext(
            attributes, &info, KURL("chrome://VideoTrackRecorderImpl"));

    if (encoder_thread_context_ &&
        !encoder_thread_context_->BindToCurrentSequence()) {
      encoder_thread_context_ = nullptr;
    }
  }

  if (!encoder_thread_context_) {
    // Send black frames (yuv = {0, 127, 127}).
    frame = media::VideoFrame::CreateColorFrame(
        video_frame->visible_rect().size(), 0u, 0x80, 0x80,
        video_frame->timestamp());
  } else {
    // Accelerated decoders produce ARGB/ABGR texture-backed frames (see
    // https://crbug.com/585242), fetch them using a PaintCanvasVideoRenderer.
    // Additionally, macOS accelerated decoders can produce XRGB content
    // and are treated the same way.
    //
    // This path is also used for less common formats like I422, I444, and
    // high bit depth pixel formats.

    const gfx::Size& old_visible_size = video_frame->visible_rect().size();
    gfx::Size new_visible_size = old_visible_size;

    media::VideoRotation video_rotation = media::VIDEO_ROTATION_0;
    if (video_frame->metadata().transformation)
      video_rotation = video_frame->metadata().transformation->rotation;

    if (video_rotation == media::VIDEO_ROTATION_90 ||
        video_rotation == media::VIDEO_ROTATION_270) {
      new_visible_size.SetSize(old_visible_size.height(),
                               old_visible_size.width());
    }

    frame = frame_pool_.CreateFrame(
        is_opaque ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
        new_visible_size, gfx::Rect(new_visible_size), new_visible_size,
        video_frame->timestamp());

    const SkImageInfo info = SkImageInfo::MakeN32(
        frame->visible_rect().width(), frame->visible_rect().height(),
        is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType);

    // Create |surface_| if it doesn't exist or incoming resolution has changed.
    if (!canvas_ || canvas_->imageInfo().width() != info.width() ||
        canvas_->imageInfo().height() != info.height()) {
      bitmap_.allocPixels(info);
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(bitmap_);
    }
    if (!video_renderer_)
      video_renderer_ = std::make_unique<media::PaintCanvasVideoRenderer>();

    encoder_thread_context_->CopyVideoFrame(video_renderer_.get(),
                                            video_frame.get(), canvas_.get());

    SkPixmap pixmap;
    if (!bitmap_.peekPixels(&pixmap)) {
      DLOG(ERROR) << "Error trying to map PaintSurface's pixels";
      return nullptr;
    }

#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
    const uint32_t source_pixel_format = libyuv::FOURCC_ABGR;
#else
    const uint32_t source_pixel_format = libyuv::FOURCC_ARGB;
#endif
    if (libyuv::ConvertToI420(
            static_cast<uint8_t*>(pixmap.writable_addr()),
            pixmap.computeByteSize(),
            frame->GetWritableVisibleData(media::VideoFrame::kYPlane),
            frame->stride(media::VideoFrame::kYPlane),
            frame->GetWritableVisibleData(media::VideoFrame::kUPlane),
            frame->stride(media::VideoFrame::kUPlane),
            frame->GetWritableVisibleData(media::VideoFrame::kVPlane),
            frame->stride(media::VideoFrame::kVPlane), 0 /* crop_x */,
            0 /* crop_y */, pixmap.width(), pixmap.height(),
            old_visible_size.width(), old_visible_size.height(),
            MediaVideoRotationToRotationMode(video_rotation),
            source_pixel_format) != 0) {
      DLOG(ERROR) << "Error converting frame to I420";
      return nullptr;
    }
    if (!is_opaque) {
      // Alpha has the same alignment for both ABGR and ARGB.
      libyuv::ARGBExtractAlpha(
          static_cast<uint8_t*>(pixmap.writable_addr()),
          static_cast<int>(pixmap.rowBytes()) /* stride */,
          frame->GetWritableVisibleData(media::VideoFrame::kAPlane),
          frame->stride(media::VideoFrame::kAPlane), pixmap.width(),
          pixmap.height());
    }
  }
  return frame;
}

void VideoTrackRecorderImpl::Encoder::SetPaused(bool paused) {
  paused_ = paused;
}

bool VideoTrackRecorderImpl::Encoder::CanEncodeAlphaChannel() const {
  return false;
}

scoped_refptr<media::VideoFrame>
VideoTrackRecorderImpl::Encoder::ConvertToI420ForSoftwareEncoder(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_EQ(frame->format(), media::VideoPixelFormat::PIXEL_FORMAT_NV12);

  if (frame->GetGpuMemoryBuffer())
    frame = media::ConvertToMemoryMappedFrame(frame);
  if (!frame)
    return nullptr;

  scoped_refptr<media::VideoFrame> i420_frame = frame_pool_.CreateFrame(
      media::VideoPixelFormat::PIXEL_FORMAT_I420, frame->coded_size(),
      frame->visible_rect(), frame->natural_size(), frame->timestamp());
  auto ret = libyuv::NV12ToI420(
      frame->data(0), frame->stride(0), frame->data(1), frame->stride(1),
      i420_frame->writable_data(media::VideoFrame::kYPlane),
      i420_frame->stride(media::VideoFrame::kYPlane),
      i420_frame->writable_data(media::VideoFrame::kUPlane),
      i420_frame->stride(media::VideoFrame::kUPlane),
      i420_frame->writable_data(media::VideoFrame::kVPlane),
      i420_frame->stride(media::VideoFrame::kVPlane),
      frame->coded_size().width(), frame->coded_size().height());
  if (ret)
    return frame;
  return i420_frame;
}

// static
VideoTrackRecorderImpl::CodecId VideoTrackRecorderImpl::GetPreferredCodecId() {
  return GetCodecEnumerator()->GetPreferredCodecId();
}

// static
bool VideoTrackRecorderImpl::CanUseAcceleratedEncoder(CodecId codec,
                                                      size_t width,
                                                      size_t height,
                                                      double framerate) {
  if (!MustUseVEA(codec)) {
    if (width < kVEAEncoderMinResolutionWidth) {
      return false;
    }
    if (height < kVEAEncoderMinResolutionHeight) {
      return false;
    }
  }

  const auto profiles = GetCodecEnumerator()->GetSupportedProfiles(codec);
  if (profiles.empty())
    return false;

  for (const auto& profile : profiles) {
    if (profile.profile == media::VIDEO_CODEC_PROFILE_UNKNOWN) {
      return false;
    }

    const gfx::Size& min_resolution = profile.min_resolution;
    DCHECK_GE(min_resolution.width(), 0);
    const size_t min_width = static_cast<size_t>(min_resolution.width());
    DCHECK_GE(min_resolution.height(), 0);
    const size_t min_height = static_cast<size_t>(min_resolution.height());

    const gfx::Size& max_resolution = profile.max_resolution;
    DCHECK_GE(max_resolution.width(), 0);
    const size_t max_width = static_cast<size_t>(max_resolution.width());
    DCHECK_GE(max_resolution.height(), 0);
    const size_t max_height = static_cast<size_t>(max_resolution.height());

    const bool width_within_range = max_width >= width && width >= min_width;
    const bool height_within_range =
        max_height >= height && height >= min_height;

    const bool valid_framerate =
        framerate * profile.max_framerate_denominator <=
        profile.max_framerate_numerator;

    if (width_within_range && height_within_range && valid_framerate) {
      return true;
    }
  }
  return false;
}

VideoTrackRecorderImpl::VideoTrackRecorderImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    CodecProfile codec_profile,
    MediaStreamComponent* track,
    OnEncodedVideoCB on_encoded_video_cb,
    base::OnceClosure on_track_source_ended_cb,
    base::OnceClosure on_error_cb,
    uint32_t bits_per_second)
    : VideoTrackRecorder(std::move(main_thread_task_runner),
                         std::move(on_track_source_ended_cb)),
      track_(track),
      on_error_cb_(std::move(on_error_cb)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(track_);
  DCHECK(track_->GetSourceType() == MediaStreamSource::kTypeVideo);
  initialize_encoder_cb_ = WTF::BindRepeating(
      &VideoTrackRecorderImpl::InitializeEncoder, weak_factory_.GetWeakPtr(),
      codec_profile, std::move(on_encoded_video_cb), bits_per_second);
  // InitializeEncoder() will be called on Render Main thread.
  ConnectToTrack(
      base::BindPostTask(main_thread_task_runner_,
                         WTF::BindRepeating(initialize_encoder_cb_,
                                            true /* allow_vea_encoder */)));
}

VideoTrackRecorderImpl::~VideoTrackRecorderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DisconnectFromTrack();
}

void VideoTrackRecorderImpl::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (encoder_)
    encoder_.AsyncCall(&Encoder::SetPaused).WithArgs(true);
  else
    should_pause_encoder_on_initialization_ = true;
}

void VideoTrackRecorderImpl::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (encoder_)
    encoder_.AsyncCall(&Encoder::SetPaused).WithArgs(false);
  else
    should_pause_encoder_on_initialization_ = false;
}

void VideoTrackRecorderImpl::OnVideoFrameForTesting(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks timestamp) {
  DVLOG(3) << __func__;

  if (!encoder_) {
    DCHECK(!initialize_encoder_cb_.is_null());
    initialize_encoder_cb_.Run(/*allow_vea_encoder=*/true, frame, {},
                               timestamp);
  }
  encoder_.AsyncCall(&Encoder::StartFrameEncode)
      .WithArgs(std::move(frame),
                std::vector<scoped_refptr<media::VideoFrame>>(), timestamp);
}

void VideoTrackRecorderImpl::InitializeEncoder(
    CodecProfile codec_profile,
    const OnEncodedVideoCB& on_encoded_video_cb,
    uint32_t bits_per_second,
    bool allow_vea_encoder,
    scoped_refptr<media::VideoFrame> video_frame,
    std::vector<scoped_refptr<media::VideoFrame>> /*scaled_video_frames*/,
    base::TimeTicks capture_time) {
  DVLOG(3) << __func__ << video_frame->visible_rect().size().ToString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // Scaled video frames are currently ignored.
  auto on_encoder_support_known_cb = WTF::BindOnce(
      &VideoTrackRecorderImpl::InitializeEncoderOnEncoderSupportKnown,
      weak_factory_.GetWeakPtr(), codec_profile, on_encoded_video_cb,
      bits_per_second, allow_vea_encoder, std::move(video_frame), capture_time);

  if (!allow_vea_encoder) {
    // If HW encoding is not being used, no need to wait for encoder
    // enumeration.
    std::move(on_encoder_support_known_cb).Run();
    return;
  }

  // Delay initializing the encoder until HW support is known, so that
  // CanUseAcceleratedEncoder() can give a reliable and consistent answer.
  NotifyEncoderSupportKnown(std::move(on_encoder_support_known_cb));
}

void VideoTrackRecorderImpl::InitializeEncoderOnEncoderSupportKnown(
    CodecProfile codec_profile,
    const OnEncodedVideoCB& on_encoded_video_cb,
    uint32_t bits_per_second,
    bool allow_vea_encoder,
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks capture_time) {
  DVLOG(3) << __func__ << frame->AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  const gfx::Size& input_size = frame->visible_rect().size();
  const bool can_use_vea = CanUseAcceleratedEncoder(
      codec_profile.codec_id, input_size.width(), input_size.height());

#if BUILDFLAG(USE_PROPRIETARY_CODECS) && !BUILDFLAG(RTC_USE_H264)
  if (MustUseVEA(codec_profile.codec_id) &&
      (!allow_vea_encoder || !can_use_vea)) {
    // This should only happen if the H264 isn't supported by the VEA or an
    // an error was thrown while using the VEA for encoding.
    DLOG(ERROR) << "Can't use VEA, but must be able to use VEA...";
    if (on_error_cb_) {
      std::move(on_error_cb_).Run();
    }
    return;
  }
#endif

  // Avoid reinitializing |encoder_| when there are multiple frames sent to the
  // sink to initialize, https://crbug.com/698441.
  if (encoder_)
    return;

  DisconnectFromTrack();

  std::unique_ptr<Encoder> encoder;
  base::WeakPtr<Encoder> weak_encoder;
  scoped_refptr<base::SequencedTaskRunner> encoding_task_runner;
  if (allow_vea_encoder && can_use_vea) {
    // TODO(b/227350897): remove once codec histogram is verified working
    UMA_HISTOGRAM_BOOLEAN("Media.MediaRecorder.VEAUsed", true);
    UmaHistogramForCodec(true, codec_profile.codec_id);
    encoding_task_runner =
        Platform::Current()->GetGpuFactories()->GetTaskRunner();

    const auto [vea_profile, vbr_supported] =
        codec_profile.profile
            ? GetCodecEnumerator()->FindSupportedVideoCodecProfile(
                  codec_profile.codec_id, *codec_profile.profile)
            : GetCodecEnumerator()->GetFirstSupportedVideoCodecProfile(
                  codec_profile.codec_id);

    bool use_import_mode =
        frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER;
    // VBR encoding is preferred.
    media::Bitrate::Mode bitrate_mode = vbr_supported
                                            ? media::Bitrate::Mode::kVariable
                                            : media::Bitrate::Mode::kConstant;

    auto vea_encoder = std::make_unique<VEAEncoder>(
        encoding_task_runner, on_encoded_video_cb,
        base::BindPostTask(
            main_thread_task_runner_,
            WTF::BindRepeating(&VideoTrackRecorderImpl::OnHardwareEncoderError,
                               weak_factory_.GetWeakPtr())),
        bitrate_mode, bits_per_second, vea_profile, codec_profile.level,
        input_size, use_import_mode);
    weak_encoder = vea_encoder->GetWeakPtr();
    encoder = std::move(vea_encoder);
  } else {
    // TODO(b/227350897): remove once codec histogram is verified working
    UMA_HISTOGRAM_BOOLEAN("Media.MediaRecorder.VEAUsed", false);
    UmaHistogramForCodec(false, codec_profile.codec_id);
    encoding_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
    switch (codec_profile.codec_id) {
#if BUILDFLAG(RTC_USE_H264)
      case CodecId::kH264: {
        auto h264_encoder = std::make_unique<H264Encoder>(
            encoding_task_runner, on_encoded_video_cb, codec_profile,
            bits_per_second);
        weak_encoder = h264_encoder->GetWeakPtr();
        encoder = std::move(h264_encoder);
      } break;
#endif
      case CodecId::kVp8:
      case CodecId::kVp9: {
        auto vpx_encoder = std::make_unique<VpxEncoder>(
            encoding_task_runner, codec_profile.codec_id == CodecId::kVp9,
            on_encoded_video_cb, bits_per_second);
        weak_encoder = vpx_encoder->GetWeakPtr();
        encoder = std::move(vpx_encoder);
      } break;
#if BUILDFLAG(ENABLE_LIBAOM)
      case CodecId::kAv1: {
        CHECK(on_error_cb_);
        // TODO(crbug.com/1424974): Use MediaRecorderEncoderWrapper for other
        // codecs.
        auto video_encoder = std::make_unique<MediaRecorderEncoderWrapper>(
            encoding_task_runner,
            codec_profile.profile.value_or(media::AV1PROFILE_PROFILE_MAIN),
            bits_per_second,
            GetCreateSoftwareVideoEncoderCallback(codec_profile.codec_id),
            on_encoded_video_cb, std::move(on_error_cb_));
        weak_encoder = video_encoder->GetWeakPtr();
        encoder = std::move(video_encoder);
      } break;
#endif  // BUILDFLAG(ENABLE_LIBAOM)
      default:
        NOTREACHED() << "Unsupported codec "
                     << static_cast<int>(codec_profile.codec_id);
    }
  }

  encoder_.emplace(encoding_task_runner, std::move(encoder));
  encoder_.AsyncCall(&Encoder::Initialize);
  if (should_pause_encoder_on_initialization_)
    encoder_.AsyncCall(&Encoder::SetPaused).WithArgs(true);

  // Encoder::StartFrameEncode() will be called on the encoding sequence.
  ConnectToTrack(base::BindPostTask(
      encoding_task_runner,
      ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          &Encoder::StartFrameEncode, weak_encoder))));
}

void VideoTrackRecorderImpl::OnHardwareEncoderError() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // InitializeEncoder() will be called to reinitialize encoder on Render Main
  // thread.
  DisconnectFromTrack();
  encoder_.Reset();
  ConnectToTrack(base::BindPostTask(
      main_thread_task_runner_,
      WTF::BindRepeating(initialize_encoder_cb_, false /*allow_vea_encoder*/)));
}

void VideoTrackRecorderImpl::ConnectToTrack(
    const VideoCaptureDeliverFrameCB& callback) {
  track_->AddSink(this, callback, MediaStreamVideoSink::IsSecure::kNo,
                  MediaStreamVideoSink::UsesAlpha::kDefault);
}

void VideoTrackRecorderImpl::DisconnectFromTrack() {
  auto* video_track =
      static_cast<MediaStreamVideoTrack*>(track_->GetPlatformTrack());
  video_track->RemoveSink(this);
}

VideoTrackRecorderPassthrough::VideoTrackRecorderPassthrough(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    MediaStreamComponent* track,
    OnEncodedVideoCB on_encoded_video_cb,
    base::OnceClosure on_track_source_ended_cb)
    : VideoTrackRecorder(std::move(main_thread_task_runner),
                         std::move(on_track_source_ended_cb)),
      track_(track),
      state_(KeyFrameState::kWaitingForKeyFrame),
      callback_(std::move(on_encoded_video_cb)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // HandleEncodedVideoFrame() will be called on Render Main thread.
  // Note: Adding an encoded sink internally generates a new key frame
  // request, no need to RequestRefreshFrame().
  ConnectEncodedToTrack(
      WebMediaStreamTrack(track_),
      base::BindPostTask(
          main_thread_task_runner_,
          WTF::BindRepeating(
              &VideoTrackRecorderPassthrough::HandleEncodedVideoFrame,
              weak_factory_.GetWeakPtr())));
}

VideoTrackRecorderPassthrough::~VideoTrackRecorderPassthrough() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DisconnectFromTrack();
}

void VideoTrackRecorderPassthrough::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  state_ = KeyFrameState::kPaused;
}

void VideoTrackRecorderPassthrough::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  state_ = KeyFrameState::kWaitingForKeyFrame;
  RequestRefreshFrame();
}

void VideoTrackRecorderPassthrough::OnEncodedVideoFrameForTesting(
    scoped_refptr<EncodedVideoFrame> frame,
    base::TimeTicks capture_time) {
  HandleEncodedVideoFrame(frame, capture_time);
}

void VideoTrackRecorderPassthrough::RequestRefreshFrame() {
  auto* video_track =
      static_cast<MediaStreamVideoTrack*>(track_->GetPlatformTrack());
  DCHECK(video_track->source());
  video_track->source()->RequestRefreshFrame();
}

void VideoTrackRecorderPassthrough::DisconnectFromTrack() {
  // TODO(crbug.com/704136) : Remove this method when moving
  // MediaStreamVideoTrack to Oilpan's heap.
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DisconnectEncodedFromTrack();
}

void VideoTrackRecorderPassthrough::HandleEncodedVideoFrame(
    scoped_refptr<EncodedVideoFrame> encoded_frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (state_ == KeyFrameState::kPaused)
    return;
  if (state_ == KeyFrameState::kWaitingForKeyFrame &&
      !encoded_frame->IsKeyFrame()) {
    // Don't RequestRefreshFrame() here - we already did this implicitly when
    // Creating/Starting or explicitly when Resuming this object.
    return;
  }
  state_ = KeyFrameState::kKeyFrameReceivedOK;

  absl::optional<gfx::ColorSpace> color_space;
  if (encoded_frame->ColorSpace())
    color_space = encoded_frame->ColorSpace()->ToGfxColorSpace();
  auto span = encoded_frame->Data();
  const char* span_begin = reinterpret_cast<const char*>(span.data());
  std::string data(span_begin, span_begin + span.size());
  media::Muxer::VideoParameters params(encoded_frame->Resolution(),
                                       /*frame_rate=*/0.0f,
                                       /*codec=*/encoded_frame->Codec(),
                                       color_space);
  callback_.Run(params, std::move(data), {}, estimated_capture_time,
                encoded_frame->IsKeyFrame());
}

}  // namespace blink
