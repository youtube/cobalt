// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"

#include <memory>
#include <numeric>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/capture/capture_switches.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/h264_parser.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_gfx.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(ARCH_CPU_ARM_FAMILY)
bool IsRK3399Board() {
  const std::string board = base::SysInfo::GetLsbReleaseBoard();
  const char* kRK3399Boards[] = {
      "bob",
      "kevin",
      "rainier",
      "scarlet",
  };
  for (const char* b : kRK3399Boards) {
    if (board.find(b) == 0u) {  // if |board| starts with |b|.
      return true;
    }
  }
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(ARCH_CPU_ARM_FAMILY)

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsZeroCopyTabCaptureEnabled() {
  // If you change this function, please change the code of the same function
  // in
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_content.cc.
#if defined(ARCH_CPU_ARM_FAMILY)
  // The GL driver used on RK3399 has a problem to enable zero copy tab capture.
  // See b/267966835.
  // TODO(b/239503724): Remove this code when RK3399 reaches EOL.
  static bool kIsRK3399Board = IsRK3399Board();
  if (kIsRK3399Board) {
    return false;
  }
#endif  // defined(ARCH_CPU_ARM_FAMILY)
  return base::FeatureList::IsEnabled(blink::features::kZeroCopyTabCapture);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool IsNV12GpuMemoryBufferVideoFrame(const webrtc::VideoFrame& input_image) {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer =
      input_image.video_frame_buffer();
  if (video_frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative) {
    return false;
  }
  const scoped_refptr<media::VideoFrame> frame =
      static_cast<blink::WebRtcVideoFrameAdapter*>(video_frame_buffer.get())
          ->getMediaVideoFrame();
  CHECK(frame);
  return frame->format() == media::PIXEL_FORMAT_NV12 &&
         frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER;
}

class SignaledValue {
 public:
  SignaledValue() : event(nullptr), val(nullptr) {}
  SignaledValue(base::WaitableEvent* event, int32_t* val)
      : event(event), val(val) {
    DCHECK(event);
  }

  ~SignaledValue() {
    if (IsValid() && !event->IsSignaled()) {
      NOTREACHED() << "never signaled";
      event->Signal();
    }
  }

  // Move-only.
  SignaledValue(const SignaledValue&) = delete;
  SignaledValue& operator=(const SignaledValue&) = delete;
  SignaledValue(SignaledValue&& other) : event(other.event), val(other.val) {
    other.event = nullptr;
    other.val = nullptr;
  }
  SignaledValue& operator=(SignaledValue&& other) {
    event = other.event;
    val = other.val;
    other.event = nullptr;
    other.val = nullptr;
    return *this;
  }

  void Signal() {
    if (!IsValid())
      return;
    event->Signal();
    event = nullptr;
  }

  void Set(int32_t v) {
    if (!val)
      return;
    *val = v;
  }

  bool IsValid() { return event; }

 private:
  base::WaitableEvent* event;
  int32_t* val;
};

class ScopedSignaledValue {
 public:
  ScopedSignaledValue() = default;
  ScopedSignaledValue(base::WaitableEvent* event, int32_t* val)
      : sv(event, val) {}
  explicit ScopedSignaledValue(SignaledValue sv) : sv(std::move(sv)) {}

  ~ScopedSignaledValue() { sv.Signal(); }

  ScopedSignaledValue(const ScopedSignaledValue&) = delete;
  ScopedSignaledValue& operator=(const ScopedSignaledValue&) = delete;
  ScopedSignaledValue(ScopedSignaledValue&& other) : sv(std::move(other.sv)) {
    DCHECK(!other.sv.IsValid());
  }
  ScopedSignaledValue& operator=(ScopedSignaledValue&& other) {
    sv.Signal();
    sv = std::move(other.sv);
    DCHECK(!other.sv.IsValid());
    return *this;
  }

  // Set |v|, signal |sv|, and invalidate |sv|. If |sv| is already invalidated
  // at the call, this has no effect.
  void SetAndReset(int32_t v) {
    sv.Set(v);
    reset();
  }

  // Invalidate |sv|. The invalidated value will be set by move assignment
  // operator.
  void reset() { *this = ScopedSignaledValue(); }

 private:
  SignaledValue sv;
};

class EncodedDataWrapper : public webrtc::EncodedImageBufferInterface {
 public:
  EncodedDataWrapper(uint8_t* data,
                     size_t size,
                     base::OnceClosure reuse_buffer_callback)
      : data_(data),
        size_(size),
        reuse_buffer_callback_(std::move(reuse_buffer_callback)) {}
  ~EncodedDataWrapper() override {
    DCHECK(reuse_buffer_callback_);
    std::move(reuse_buffer_callback_).Run();
  }
  const uint8_t* data() const override { return data_; }
  uint8_t* data() override { return data_; }
  size_t size() const override { return size_; }

 private:
  uint8_t* const data_;
  const size_t size_;
  base::OnceClosure reuse_buffer_callback_;
};

struct FrameChunk {
  FrameChunk(const webrtc::VideoFrame& input_image, bool force_keyframe)
      : video_frame_buffer(input_image.video_frame_buffer()),
        timestamp(input_image.timestamp()),
        timestamp_us(input_image.timestamp_us()),
        render_time_ms(input_image.render_time_ms()),
        force_keyframe(force_keyframe) {
    DCHECK(video_frame_buffer);
  }

  const rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer;
  // TODO(b/241349739): timestamp and timestamp_us should be unified as one
  // base::TimeDelta.
  const uint32_t timestamp;
  const uint64_t timestamp_us;
  const int64_t render_time_ms;

  const bool force_keyframe;
};

bool ConvertKbpsToBps(uint32_t bitrate_kbps, uint32_t* bitrate_bps) {
  if (!base::IsValueInRangeForNumericType<uint32_t>(bitrate_kbps *
                                                    UINT64_C(1000))) {
    return false;
  }
  *bitrate_bps = bitrate_kbps * 1000;
  return true;
}
}  // namespace

namespace WTF {

template <>
struct CrossThreadCopier<webrtc::VideoEncoder::RateControlParameters>
    : public CrossThreadCopierPassThrough<
          webrtc::VideoEncoder::RateControlParameters> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<
    std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>>
    : public CrossThreadCopierPassThrough<
          std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<FrameChunk>
    : public CrossThreadCopierPassThrough<FrameChunk> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<media::VideoEncodeAccelerator::Config>
    : public CrossThreadCopierPassThrough<
          media::VideoEncodeAccelerator::Config> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<SignaledValue> {
  static SignaledValue Copy(SignaledValue sv) {
    return sv;  // this is a move in fact.
  }
};
}  // namespace WTF

namespace blink {

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "RTCVideoEncoderShutdownReason" in src/tools/metrics/histograms/enums.xml.
enum class RTCVideoEncoderShutdownReason {
  kSuccessfulRelease = 0,
  kInvalidArgument = 1,
  kIllegalState = 2,
  kPlatformFailure = 3,
  kMaxValue = kPlatformFailure,
};

static_assert(static_cast<int>(RTCVideoEncoderShutdownReason::kMaxValue) ==
                  media::VideoEncodeAccelerator::kErrorMax + 1,
              "RTCVideoEncoderShutdownReason should follow "
              "VideoEncodeAccelerator::Error (+1 for the success case)");

media::VideoEncodeAccelerator::Config::InterLayerPredMode
CopyFromWebRtcInterLayerPredMode(
    const webrtc::InterLayerPredMode inter_layer_pred) {
  switch (inter_layer_pred) {
    case webrtc::InterLayerPredMode::kOff:
      return media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOff;
    case webrtc::InterLayerPredMode::kOn:
      return media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOn;
    case webrtc::InterLayerPredMode::kOnKeyPic:
      return media::VideoEncodeAccelerator::Config::InterLayerPredMode::
          kOnKeyPic;
  }
}

// Create VEA::Config::SpatialLayer from |codec_settings|. If some config of
// |codec_settings| is not supported, returns false.
bool CreateSpatialLayersConfig(
    const webrtc::VideoCodec& codec_settings,
    std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>*
        spatial_layers,
    media::VideoEncodeAccelerator::Config::InterLayerPredMode*
        inter_layer_pred) {
  absl::optional<webrtc::ScalabilityMode> scalability_mode =
      codec_settings.GetScalabilityMode();

  if (codec_settings.codecType == webrtc::kVideoCodecVP9 &&
      codec_settings.VP9().numberOfSpatialLayers > 1 &&
      !RTCVideoEncoder::Vp9HwSupportForSpatialLayers()) {
    DVLOG(1)
        << "VP9 SVC not yet supported by HW codecs, falling back to software.";
    return false;
  }

  // We fill SpatialLayer only in temporal layer or spatial layer encoding.
  switch (codec_settings.codecType) {
    case webrtc::kVideoCodecH264:
      if (scalability_mode.has_value() &&
          *scalability_mode != webrtc::ScalabilityMode::kL1T1) {
        DVLOG(1)
            << "H264 temporal layers not yet supported by HW codecs, but use"
            << " HW codecs and leave the fallback decision to a webrtc client"
            << " by seeing metadata in webrtc::CodecSpecificInfo";

        return true;
      }
      break;
    case webrtc::kVideoCodecVP8: {
      int number_of_temporal_layers = 1;
      if (scalability_mode.has_value()) {
        switch (*scalability_mode) {
          case webrtc::ScalabilityMode::kL1T1:
            number_of_temporal_layers = 1;
            break;
          case webrtc::ScalabilityMode::kL1T2:
            number_of_temporal_layers = 2;
            break;
          case webrtc::ScalabilityMode::kL1T3:
            number_of_temporal_layers = 3;
            break;
          default:
            // Other modes not supported.
            return false;
        }
      }
      if (number_of_temporal_layers > 1) {
        if (codec_settings.mode == webrtc::VideoCodecMode::kScreensharing) {
          // This is a VP8 stream with screensharing using temporal layers for
          // temporal scalability. Since this implementation does not yet
          // implement temporal layers, fall back to software codec, if cfm and
          // board is known to have a CPU that can handle it.
          if (base::FeatureList::IsEnabled(
                  features::kWebRtcScreenshareSwEncoding)) {
            // TODO(sprang): Add support for temporal layers so we don't need
            // fallback. See eg http://crbug.com/702017
            DVLOG(1) << "Falling back to software encoder.";
            return false;
          }
        }
        // Though there is no SVC in VP8 spec. We allocate 1 element in
        // spatial_layers for temporal layer encoding.
        spatial_layers->resize(1u);
        auto& sl = (*spatial_layers)[0];
        sl.width = codec_settings.width;
        sl.height = codec_settings.height;
        if (!ConvertKbpsToBps(codec_settings.startBitrate, &sl.bitrate_bps))
          return false;
        sl.framerate = codec_settings.maxFramerate;
        sl.max_qp = base::saturated_cast<uint8_t>(codec_settings.qpMax);
        sl.num_of_temporal_layers =
            base::saturated_cast<uint8_t>(number_of_temporal_layers);
      }
      break;
    }
    case webrtc::kVideoCodecVP9:
      // Since one TL and one SL can be regarded as one simple stream,
      // SpatialLayer is not filled.
      if (codec_settings.VP9().numberOfTemporalLayers > 1 ||
          codec_settings.VP9().numberOfSpatialLayers > 1) {
        spatial_layers->clear();
        for (size_t i = 0; i < codec_settings.VP9().numberOfSpatialLayers;
             ++i) {
          const webrtc::SpatialLayer& rtc_sl = codec_settings.spatialLayers[i];
          // We ignore non active spatial layer and don't proceed further. There
          // must NOT be an active higher spatial layer than non active spatial
          // layer.
          if (!rtc_sl.active)
            break;
          spatial_layers->emplace_back();
          auto& sl = spatial_layers->back();
          sl.width = base::checked_cast<int32_t>(rtc_sl.width);
          sl.height = base::checked_cast<int32_t>(rtc_sl.height);
          if (!ConvertKbpsToBps(rtc_sl.targetBitrate, &sl.bitrate_bps))
            return false;
          sl.framerate = base::saturated_cast<int32_t>(rtc_sl.maxFramerate);
          sl.max_qp = base::saturated_cast<uint8_t>(rtc_sl.qpMax);
          sl.num_of_temporal_layers =
              base::saturated_cast<uint8_t>(rtc_sl.numberOfTemporalLayers);
        }

        if (spatial_layers->size() == 1 &&
            spatial_layers->at(0).num_of_temporal_layers == 1) {
          // Don't report spatial layers if only the base layer is active and we
          // have no temporar layers configured.
          spatial_layers->clear();
        } else {
          *inter_layer_pred = CopyFromWebRtcInterLayerPredMode(
              codec_settings.VP9().interLayerPred);
        }
      }
      break;
    default:
      break;
  }
  return true;
}

struct FrameInfo {
 public:
  FrameInfo(const base::TimeDelta& media_timestamp,
            int32_t rtp_timestamp,
            int64_t capture_time_ms,
            const std::vector<gfx::Size>& resolutions)
      : media_timestamp_(media_timestamp),
        rtp_timestamp_(rtp_timestamp),
        capture_time_ms_(capture_time_ms),
        resolutions_(resolutions) {}

  const base::TimeDelta media_timestamp_;
  const int32_t rtp_timestamp_;
  const int64_t capture_time_ms_;
  const std::vector<gfx::Size> resolutions_ ALLOW_DISCOURAGED_TYPE(
      "Matches media::Vp9Metadata::spatial_layer_resolutions etc");
  size_t produced_frames_ = 0;
};

webrtc::VideoCodecType ProfileToWebRtcVideoCodecType(
    media::VideoCodecProfile profile) {
  switch (media::VideoCodecProfileToVideoCodec(profile)) {
    case media::VideoCodec::kH264:
      return webrtc::kVideoCodecH264;
    case media::VideoCodec::kVP8:
      return webrtc::kVideoCodecVP8;
    case media::VideoCodec::kVP9:
      return webrtc::kVideoCodecVP9;
    case media::VideoCodec::kAV1:
      return webrtc::kVideoCodecAV1;
    default:
      NOTREACHED() << "Invalid profile " << GetProfileName(profile);
      return webrtc::kVideoCodecGeneric;
  }
}

void RecordInitEncodeUMA(int32_t init_retval,
                         media::VideoCodecProfile profile) {
  base::UmaHistogramBoolean("Media.RTCVideoEncoderInitEncodeSuccess",
                            init_retval == WEBRTC_VIDEO_CODEC_OK);
  if (init_retval != WEBRTC_VIDEO_CODEC_OK)
    return;
  UMA_HISTOGRAM_ENUMERATION("Media.RTCVideoEncoderProfile", profile,
                            media::VIDEO_CODEC_PROFILE_MAX + 1);
}

void RecordEncoderShutdownReasonUMA(RTCVideoEncoderShutdownReason reason,
                                    webrtc::VideoCodecType type) {
  switch (type) {
    case webrtc::VideoCodecType::kVideoCodecH264:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.H264",
                                    reason);
      break;
    case webrtc::VideoCodecType::kVideoCodecVP8:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.VP8",
                                    reason);
      break;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.VP9",
                                    reason);
      break;
    case webrtc::VideoCodecType::kVideoCodecAV1:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.AV1",
                                    reason);
      break;
    default:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.Other",
                                    reason);
  }
}

bool SupportGpuMemoryBufferEncoding() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kVideoCaptureUseGpuMemoryBuffer);
}

bool IsZeroCopyEnabled(webrtc::VideoContentType content_type) {
  if (content_type == webrtc::VideoContentType::SCREENSHARE) {
    // Zero copy screen capture.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // The zero-copy capture is available for all sources in ChromeOS
    // Ash-chrome.
    return IsZeroCopyTabCaptureEnabled();
#else
    // Currently, zero copy capture screenshare is available only for tabs.
    // Since it is impossible to determine the content source, tab, window or
    // monitor, we don't configure VideoEncodeAccelerator with NV12
    // GpuMemoryBuffer instead we configure I420 SHMEM as if it is not zero
    // copy, and we convert the NV12 GpuMemoryBuffer to I420 SHMEM in
    // RtcVideoEncoder::Impl::Encode().
    // TODO(b/267995715): Solve this problem by calling Initialize() in the
    // first frame.
    return false;
#endif
  }
  // Zero copy video capture from other sources (e.g. camera).
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableVideoCaptureUseGpuMemoryBuffer) &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kVideoCaptureUseGpuMemoryBuffer);
}

}  // namespace

namespace features {
// Initialize VideoEncodeAccelerator on the first encode.
BASE_FEATURE(kWebRtcInitializeOnFirstFrame,
             "WebRtcInitializeEncoderOnFirstFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Fallback from hardware encoder (if available) to software, for WebRTC
// screensharing that uses temporal scalability.
BASE_FEATURE(kWebRtcScreenshareSwEncoding,
             "WebRtcScreenshareSwEncoding",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

// This private class of RTCVideoEncoder does the actual work of communicating
// with a media::VideoEncodeAccelerator for handling video encoding. It can
// be created on any thread, but should subsequently be executed on
// |gpu_task_runner| including destructor.
//
// This class separates state related to the thread that RTCVideoEncoder
// operates on from the thread that |gpu_factories_| provides for accelerator
// operations (presently the media thread).
class RTCVideoEncoder::Impl : public media::VideoEncodeAccelerator::Client {
 public:
  using UpdateEncoderInfoCallback = base::RepeatingCallback<void(
      media::VideoEncoderInfo,
      std::vector<webrtc::VideoFrameBuffer::Type>)>;
  Impl(media::GpuVideoAcceleratorFactories* gpu_factories,
       webrtc::VideoCodecType video_codec_type,
       webrtc::VideoContentType video_content_type,
       UpdateEncoderInfoCallback update_encoder_info_callback,
       base::RepeatingClosure execute_software_fallback,
       base::WeakPtr<Impl>& weak_this_for_client);

  ~Impl() override;
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  // Create the VEA and call Initialize() on it.  Called once per instantiation,
  // and then the instance is bound forevermore to whichever thread made the
  // call.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |init_event| argument.
  void CreateAndInitializeVEA(
      const media::VideoEncodeAccelerator::Config& vea_config,
      SignaledValue init_event);

  // Enqueue a frame from WebRTC for encoding.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |encode_event| argument.
  void Enqueue(FrameChunk frame_chunk, SignaledValue encode_event);

  // Request encoding parameter change for the underlying encoder.
  void RequestEncodingParametersChange(
      const webrtc::VideoEncoder::RateControlParameters& parameters);

  void RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback);

  webrtc::VideoCodecType video_codec_type() const { return video_codec_type_; }

  // media::VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyErrorStatus(const media::EncoderStatus& status) override;
  void NotifyEncoderInfoChange(const media::VideoEncoderInfo& info) override;

 private:
  enum {
    kInputBufferExtraCount = 1,  // The number of input buffers allocated, more
                                 // than what is requested by
                                 // VEA::RequireBitstreamBuffers().
    kOutputBufferCount = 3,
  };

  // Perform encoding on an input frame from the input queue.
  void EncodeOneFrame(FrameChunk frame_chunk);

  // Perform encoding on an input frame from the input queue using VEA native
  // input mode.  The input frame must be backed with GpuMemoryBuffer buffers.
  void EncodeOneFrameWithNativeInput(FrameChunk frame_chunk);

  // Creates a GpuMemoryBuffer frame filled with black pixels. Returns true if
  // the frame is successfully created; false otherwise.
  bool CreateBlackGpuMemoryBufferFrame(const gfx::Size& natural_size);

  // Notify that an input frame is finished for encoding. |index| is the index
  // of the completed frame in |input_buffers_|.
  void InputBufferReleased(int index);

  // Checks if the frame size is different than hardware accelerator
  // requirements.
  bool RequiresSizeChange(const media::VideoFrame& frame) const;

  // Return an encoded output buffer to WebRTC.
  void ReturnEncodedImage(const webrtc::EncodedImage& image,
                          const webrtc::CodecSpecificInfo& info,
                          int32_t bitstream_buffer_id);

  // Records |failed_timestamp_match_| value after a session.
  void RecordTimestampMatchUMA() const;

  // Get a list of the spatial layer resolutions that are currently active,
  // meaning the are configured, have active=true and have non-zero bandwidth
  // allocated to them.
  // Returns an empty list is spatial layers are not used.
  std::vector<gfx::Size> ActiveSpatialResolutions() const;

  // Call VideoEncodeAccelerator::UseOutputBitstreamBuffer() for a buffer whose
  // id is |bitstream_buffer_id|.
  void UseOutputBitstreamBuffer(int32_t bitstream_buffer_id);

  // RTCVideoEncoder is given a buffer to be passed to WebRTC through the
  // RTCVideoEncoder::ReturnEncodedImage() function.  When that is complete,
  // the buffer is returned to Impl by its index using this function.
  void BitstreamBufferAvailable(int32_t bitstream_buffer_id);

  // This is attached to |gpu_task_runner_|, not the thread class is constructed
  // on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Factory for creating VEAs, shared memory buffers, etc.
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // webrtc::VideoEncoder expects InitEncode() and Encode() to be synchronous.
  // Do this by waiting on the |async_init_event_| when initialization
  // completes, on |async_encode_event_| when encoding completes and on both
  // when an error occurs.
  ScopedSignaledValue async_init_event_;
  ScopedSignaledValue async_encode_event_;

  // The underlying VEA to perform encoding on.
  std::unique_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Metadata for frames passed to Encode(), matched to encoded frames using
  // timestamps.
  WTF::Deque<FrameInfo> submitted_frames_;

  // Indicates that timestamp match failed and we should no longer attempt
  // matching.
  bool failed_timestamp_match_{false};

  // The pending frames to be encoded with the boolean representing whether the
  // frame must be encoded keyframe.
  WTF::Deque<FrameChunk> pending_frames_;

  // Frame sizes.
  gfx::Size input_frame_coded_size_;
  gfx::Size input_visible_size_;

  // Shared memory buffers for input/output with the VEA.
  Vector<std::unique_ptr<base::MappedReadOnlyRegion>> input_buffers_;

  Vector<std::pair<base::UnsafeSharedMemoryRegion,
                   base::WritableSharedMemoryMapping>>
      output_buffers_;

  // The number of frames that are sent to a hardware video encoder by Encode()
  // and the encoder holds them.
  size_t frames_in_encoder_count_ = 0;

  // Input buffers ready to be filled with input from Encode().  As a LIFO since
  // we don't care about ordering.
  Vector<int> input_buffers_free_;

  // The number of output buffers that have been sent to a hardware video
  // encoder by VideoEncodeAccelerator::UseOutputBitstreamBuffer() and the
  // encoder holds them.
  size_t output_buffers_in_encoder_count_{0};

  // The buffer ids that are not sent to a hardware video encoder and this holds
  // them. UseOutputBitstreamBuffer() is called for them on the next Encode().
  Vector<int32_t> pending_output_buffers_;

  // Whether to send the frames to VEA as native buffer. Native buffer allows
  // VEA to pass the buffer to the encoder directly without further processing.
  bool use_native_input_{false};

  // A black GpuMemoryBuffer frame used when the video track is disabled.
  scoped_refptr<media::VideoFrame> black_gmb_frame_;

  // The video codec type, as reported to WebRTC.
  const webrtc::VideoCodecType video_codec_type_;

  // The content type, as reported to WebRTC (screenshare vs realtime video).
  const webrtc::VideoContentType video_content_type_;

  // This has the same information as |encoder_info_.preferred_pixel_formats|
  // but can be used on |sequence_checker_| without acquiring the lock.
  absl::InlinedVector<webrtc::VideoFrameBuffer::Type,
                      webrtc::kMaxPreferredPixelFormats>
      preferred_pixel_formats_;

  UpdateEncoderInfoCallback update_encoder_info_callback_;

  // Calling this causes a software encoder fallback.
  base::RepeatingClosure execute_software_fallback_;

  // The reslutions of active spatial layer, only used when |Vp9Metadata| is
  // contained in |BitstreamBufferMetadata|. it will be updated when key frame
  // is produced.
  std::vector<gfx::Size> current_spatial_layer_resolutions_
      ALLOW_DISCOURAGED_TYPE(
          "Matches media::Vp9Metadata::spatial_layer_resolutions etc");

  // Index of the highest spatial layer with bandwidth allocated for it.
  size_t highest_active_spatial_index_{0};

  // We cannot immediately return error conditions to the WebRTC user of this
  // class, as there is no error callback in the webrtc::VideoEncoder interface.
  // Instead, we cache an error status here and return it the next time an
  // interface entry point is called.
  int32_t status_ GUARDED_BY_CONTEXT(sequence_checker_){
      WEBRTC_VIDEO_CODEC_UNINITIALIZED};

  // Protect |encoded_image_callback_|. |encoded_image_callback_| is read on
  // media thread and written in webrtc encoder thread.
  mutable base::Lock lock_;

  // webrtc::VideoEncoder encode complete callback.
  // TODO(b/257021675): Don't guard this by |lock_|
  webrtc::EncodedImageCallback* encoded_image_callback_ GUARDED_BY(lock_){
      nullptr};

  // They are bound to |gpu_task_runner_|, which is sequence checked by
  // |sequence_checker|.
  base::WeakPtr<Impl> weak_this_;
  base::WeakPtrFactory<Impl> weak_this_factory_{this};
};

RTCVideoEncoder::Impl::Impl(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    webrtc::VideoCodecType video_codec_type,
    webrtc::VideoContentType video_content_type,
    UpdateEncoderInfoCallback update_encoder_info_callback,
    base::RepeatingClosure execute_software_fallback,
    base::WeakPtr<Impl>& weak_this_for_client)
    : gpu_factories_(gpu_factories),
      video_codec_type_(video_codec_type),
      video_content_type_(video_content_type),
      update_encoder_info_callback_(std::move(update_encoder_info_callback)),
      execute_software_fallback_(std::move(execute_software_fallback)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  preferred_pixel_formats_ = {webrtc::VideoFrameBuffer::Type::kI420};
  weak_this_ = weak_this_factory_.GetWeakPtr();
  weak_this_for_client = weak_this_;
}

void RTCVideoEncoder::Impl::CreateAndInitializeVEA(
    const media::VideoEncodeAccelerator::Config& vea_config,
    SignaledValue init_event) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::CreateAndInitializeVEA");
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  status_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  async_init_event_ = ScopedSignaledValue(std::move(init_event));
  async_encode_event_.reset();

  video_encoder_ = gpu_factories_->CreateVideoEncodeAccelerator();
  if (!video_encoder_) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed to create VideoEncodeAccelerato"});
    return;
  }

  input_visible_size_ = vea_config.input_visible_size;
  // The valid config is NV12+kGpuMemoryBuffer and I420+kShmem.
  CHECK_EQ(
      vea_config.input_format == media::PIXEL_FORMAT_NV12,
      vea_config.storage_type ==
          media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer);
  if (vea_config.storage_type ==
      media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer) {
    use_native_input_ = true;
    preferred_pixel_formats_ = {webrtc::VideoFrameBuffer::Type::kNV12};
  }

  // When we don't have built in H264 software encoding, allow usage of any
  // software encoders provided by the platform.
#if !BUILDFLAG(ENABLE_OPENH264) && BUILDFLAG(RTC_USE_H264)
  if (profile >= media::H264PROFILE_MIN && profile <= media::H264PROFILE_MAX) {
    vea_config.required_encoder_type =
        media::VideoEncodeAccelerator::Config::EncoderType::kNoPreference;
  }
#endif
  if (!video_encoder_->Initialize(vea_config, this,
                                  std::make_unique<media::NullMediaLog>())) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed to initialize VideoEncodeAccelerator"});
    return;
  }

  current_spatial_layer_resolutions_.clear();
  for (const auto& layer : vea_config.spatial_layers) {
    current_spatial_layer_resolutions_.emplace_back(layer.width, layer.height);
  }
  highest_active_spatial_index_ = vea_config.spatial_layers.empty()
                                      ? 0u
                                      : vea_config.spatial_layers.size() - 1;

  // RequireBitstreamBuffers or NotifyError will be called and the waiter will
  // be signaled.
}

void RTCVideoEncoder::Impl::NotifyEncoderInfoChange(
    const media::VideoEncoderInfo& info) {
  update_encoder_info_callback_.Run(
      info,
      std::vector<webrtc::VideoFrameBuffer::Type>(
          preferred_pixel_formats_.begin(), preferred_pixel_formats_.end()));
}

void RTCVideoEncoder::Impl::Enqueue(FrameChunk frame_chunk,
                                    SignaledValue encode_event) {
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::Impl::Enqueue", "timestamp",
               frame_chunk.timestamp);
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status_ != WEBRTC_VIDEO_CODEC_OK) {
    encode_event.Set(status_);
    encode_event.Signal();
    return;
  }

  // If there are no free input and output buffers, drop the frame to avoid a
  // deadlock. If there is a free input buffer and |use_native_input_| is false,
  // EncodeOneFrame will run and unblock Encode(). If there are no free input
  // buffers but there is a free output buffer, InputBufferReleased() will be
  // called later to unblock Encode().
  //
  // The caller of Encode() holds a webrtc lock. The deadlock happens when:
  // (1) Encode() is waiting for the frame to be encoded in EncodeOneFrame().
  // (2) There are no free input buffers and they cannot be freed because
  //     the encoder has no output buffers.
  // (3) Output buffers cannot be freed because OnEncodedImage() is queued
  //     on libjingle worker thread to be run. But the worker thread is waiting
  //     for the same webrtc lock held by the caller of Encode().
  //
  // Dropping a frame is fine. The encoder has been filled with all input
  // buffers. Returning an error in Encode() is not fatal and WebRTC will just
  // continue. If this is a key frame, WebRTC will request a key frame again.
  // Besides, webrtc will drop a frame if Encode() blocks too long.
  if (!use_native_input_ && input_buffers_free_.empty() &&
      output_buffers_in_encoder_count_ == 0u) {
    DVLOG(2) << "Run out of input and output buffers. Drop the frame.";
    encode_event.Set(WEBRTC_VIDEO_CODEC_ERROR);
    encode_event.Signal();
    return;
  }

  async_encode_event_ = ScopedSignaledValue(std::move(encode_event));

  if (use_native_input_) {
    DCHECK(pending_frames_.empty());
    EncodeOneFrameWithNativeInput(std::move(frame_chunk));
    return;
  }

  pending_frames_.push_back(std::move(frame_chunk));
  // When |input_buffers_free_| is empty, EncodeOneFrame() for the frame in
  // |pending_frames_| will be invoked from InputBufferReleased().
  while (!pending_frames_.empty() && !input_buffers_free_.empty()) {
    auto chunk = std::move(pending_frames_.front());
    pending_frames_.pop_front();
    EncodeOneFrame(std::move(chunk));
  }
}

void RTCVideoEncoder::Impl::BitstreamBufferAvailable(
    int32_t bitstream_buffer_id) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::BitstreamBufferAvailable");
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there is no frame in a hardware video encoder,
  // UseOutputBitstreamBuffer() call for this buffer id is postponed in the next
  // Encode() call. This avoids unnecessary thread wake up in GPU process.
  if (frames_in_encoder_count_ == 0) {
    pending_output_buffers_.push_back(bitstream_buffer_id);
    return;
  }

  UseOutputBitstreamBuffer(bitstream_buffer_id);
}

void RTCVideoEncoder::Impl::UseOutputBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::UseOutputBitstreamBuffer");
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_encoder_) {
    video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        bitstream_buffer_id,
        output_buffers_[bitstream_buffer_id].first.Duplicate(),
        output_buffers_[bitstream_buffer_id].first.GetSize()));
    output_buffers_in_encoder_count_++;
  }
}

void RTCVideoEncoder::Impl::RequestEncodingParametersChange(
    const webrtc::VideoEncoder::RateControlParameters& parameters) {
  DVLOG(3) << __func__ << " bitrate=" << parameters.bitrate.ToString()
           << ", framerate=" << parameters.framerate_fps;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status_ != WEBRTC_VIDEO_CODEC_OK)
    return;

  // NotfiyError() has been called. Don't proceed the change request.
  if (!video_encoder_)
    return;

  // This is a workaround to zero being temporarily provided, as part of the
  // initial setup, by WebRTC.
  media::VideoBitrateAllocation allocation;
  if (parameters.bitrate.get_sum_bps() == 0u) {
    allocation.SetBitrate(0, 0, 1u);
  }
  uint32_t framerate =
      std::max(1u, static_cast<uint32_t>(parameters.framerate_fps + 0.5));

  highest_active_spatial_index_ = 0;
  for (size_t spatial_id = 0;
       spatial_id < media::VideoBitrateAllocation::kMaxSpatialLayers;
       ++spatial_id) {
    bool spatial_layer_active = false;
    for (size_t temporal_id = 0;
         temporal_id < media::VideoBitrateAllocation::kMaxTemporalLayers;
         ++temporal_id) {
      // TODO(sprang): Clean this up if/when webrtc struct moves to int.
      uint32_t temporal_layer_bitrate = base::checked_cast<int>(
          parameters.bitrate.GetBitrate(spatial_id, temporal_id));
      if (!allocation.SetBitrate(spatial_id, temporal_id,
                                 temporal_layer_bitrate)) {
        LOG(WARNING) << "Overflow in bitrate allocation: "
                     << parameters.bitrate.ToString();
        break;
      }
      if (temporal_layer_bitrate > 0)
        spatial_layer_active = true;
    }

    if (spatial_layer_active &&
        spatial_id < current_spatial_layer_resolutions_.size())
      highest_active_spatial_index_ = spatial_id;
  }
  DCHECK_EQ(allocation.GetSumBps(), parameters.bitrate.get_sum_bps());
  video_encoder_->RequestEncodingParametersChange(allocation, framerate);
}

void RTCVideoEncoder::Impl::RecordTimestampMatchUMA() const {
  base::UmaHistogramBoolean("Media.RTCVideoEncoderTimestampMatchSuccess",
                            !failed_timestamp_match_);
}

std::vector<gfx::Size> RTCVideoEncoder::Impl::ActiveSpatialResolutions() const {
  if (current_spatial_layer_resolutions_.empty())
    return {};
  DCHECK_LT(highest_active_spatial_index_,
            current_spatial_layer_resolutions_.size());
  return std::vector<gfx::Size>(current_spatial_layer_resolutions_.begin(),
                                current_spatial_layer_resolutions_.begin() +
                                    highest_active_spatial_index_ + 1);
}

void RTCVideoEncoder::Impl::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::RequireBitstreamBuffers");
  DVLOG(3) << __func__ << " input_count=" << input_count
           << ", input_coded_size=" << input_coded_size.ToString()
           << ", output_buffer_size=" << output_buffer_size;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto scoped_event = std::move(async_init_event_);
  if (!video_encoder_)
    return;

  input_frame_coded_size_ = input_coded_size;

  // |input_buffers_| is only needed in non import mode.
  if (!use_native_input_) {
    const wtf_size_t num_input_buffers = input_count + kInputBufferExtraCount;
    input_buffers_free_.resize(num_input_buffers);
    input_buffers_.resize(num_input_buffers);
    for (wtf_size_t i = 0; i < num_input_buffers; i++) {
      input_buffers_free_[i] = i;
      input_buffers_[i] = nullptr;
    }
  }

  for (int i = 0; i < kOutputBufferCount; ++i) {
    base::UnsafeSharedMemoryRegion region =
        gpu_factories_->CreateSharedMemoryRegion(output_buffer_size);
    base::WritableSharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid()) {
      NotifyErrorStatus({media::EncoderStatus::Codes::kSystemAPICallError,
                         "failed to create output buffer"});
      return;
    }
    output_buffers_.push_back(
        std::make_pair(std::move(region), std::move(mapping)));
  }

  // Immediately provide all output buffers to the VEA.
  for (wtf_size_t i = 0; i < output_buffers_.size(); ++i) {
    UseOutputBitstreamBuffer(i);
  }

  pending_output_buffers_.clear();
  pending_output_buffers_.reserve(output_buffers_.size());

  DCHECK_EQ(status_, WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  status_ = WEBRTC_VIDEO_CODEC_OK;

  scoped_event.SetAndReset(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::BitstreamBufferReady");
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << metadata.payload_size_bytes
           << ", key_frame=" << metadata.key_frame
           << ", timestamp ms=" << metadata.timestamp.InMilliseconds();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bitstream_buffer_id < 0 ||
      bitstream_buffer_id >= static_cast<int>(output_buffers_.size())) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kInvalidOutputBuffer,
                       "invalid bitstream_buffer_id: " +
                           base::NumberToString(bitstream_buffer_id)});
    return;
  }
  void* output_mapping_memory =
      output_buffers_[bitstream_buffer_id].second.memory();
  if (metadata.payload_size_bytes >
      output_buffers_[bitstream_buffer_id].second.size()) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kInvalidOutputBuffer,
                       "invalid payload_size: " +
                           base::NumberToString(metadata.payload_size_bytes)});
    return;
  }
  DCHECK_NE(output_buffers_in_encoder_count_, 0u);
  output_buffers_in_encoder_count_--;

  // Decrease |frames_in_encoder_count_| on the first frame.
  if (metadata.spatial_idx().value_or(0) == 0) {
    DCHECK_NE(0u, frames_in_encoder_count_);
    frames_in_encoder_count_--;
  }

  // Find RTP and capture timestamps by going through |pending_timestamps_|.
  // Derive it from current time otherwise.
  absl::optional<uint32_t> rtp_timestamp;
  absl::optional<int64_t> capture_timestamp_ms;
  absl::optional<std::vector<gfx::Size>> expected_resolutions;
  if (!failed_timestamp_match_) {
    // Pop timestamps until we have a match.
    while (!submitted_frames_.empty()) {
      auto& front_frame = submitted_frames_.front();
      const bool end_of_picture = metadata.end_of_picture();
      if (front_frame.media_timestamp_ == metadata.timestamp) {
        rtp_timestamp = front_frame.rtp_timestamp_;
        capture_timestamp_ms = front_frame.capture_time_ms_;
        expected_resolutions = front_frame.resolutions_;
        const size_t num_resolutions =
            std::max(front_frame.resolutions_.size(), size_t{1});
        ++front_frame.produced_frames_;

        if (front_frame.produced_frames_ == num_resolutions &&
            !end_of_picture) {
          // The top layer must always have the end-of-picture indicator.
          NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderFailedEncode,
                             "missing end-of-picture"});
          return;
        }
        if (end_of_picture) {
          // Remove pending timestamp at the top spatial layer in the case of
          // SVC encoding.
          if (!front_frame.resolutions_.empty() &&
              front_frame.produced_frames_ != front_frame.resolutions_.size()) {
            // At least one resolution was not produced.
            NotifyErrorStatus(
                {media::EncoderStatus::Codes::kEncoderFailedEncode,
                 "missing resolution"});
            return;
          }
          submitted_frames_.pop_front();
        }
        break;
      }
      // Timestamp does not match front of the pending frames list.
      if (end_of_picture)
        submitted_frames_.pop_front();
    }
    DCHECK(rtp_timestamp.has_value());
  }
  if (!rtp_timestamp.has_value() || !capture_timestamp_ms.has_value()) {
    failed_timestamp_match_ = true;
    submitted_frames_.clear();
    const int64_t current_time_ms =
        rtc::TimeMicros() / base::Time::kMicrosecondsPerMillisecond;
    // RTP timestamp can wrap around. Get the lower 32 bits.
    rtp_timestamp = static_cast<uint32_t>(current_time_ms * 90);
    capture_timestamp_ms = current_time_ms;
  }

  webrtc::EncodedImage image;
  image.SetEncodedData(rtc::make_ref_counted<EncodedDataWrapper>(
      static_cast<uint8_t*>(output_mapping_memory), metadata.payload_size_bytes,
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&RTCVideoEncoder::Impl::BitstreamBufferAvailable,
                         weak_this_, bitstream_buffer_id))));
  auto encoded_size = metadata.encoded_size.value_or(input_visible_size_);
  image._encodedWidth = encoded_size.width();
  image._encodedHeight = encoded_size.height();
  image.SetTimestamp(rtp_timestamp.value());
  image.capture_time_ms_ = capture_timestamp_ms.value();
  image._frameType =
      (metadata.key_frame ? webrtc::VideoFrameType::kVideoFrameKey
                          : webrtc::VideoFrameType::kVideoFrameDelta);
  image.content_type_ = video_content_type_;
  // Default invalid qp value is -1 in webrtc::EncodedImage and
  // media::BitstreamBufferMetadata, and libwebrtc would parse bitstream to get
  // the qp if |qp_| is less than zero.
  image.qp_ = metadata.qp;

  webrtc::CodecSpecificInfo info;
  info.codecType = video_codec_type_;
  switch (video_codec_type_) {
    case webrtc::kVideoCodecH264: {
      webrtc::CodecSpecificInfoH264& h264 = info.codecSpecific.H264;
      h264.packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
      h264.idr_frame = metadata.key_frame;
      if (metadata.h264) {
        h264.temporal_idx = metadata.h264->temporal_idx;
        h264.base_layer_sync = metadata.h264->layer_sync;
      } else {
        h264.temporal_idx = webrtc::kNoTemporalIdx;
        h264.base_layer_sync = false;
      }
    } break;
    case webrtc::kVideoCodecVP8:
      info.codecSpecific.VP8.keyIdx = -1;
      break;
    case webrtc::kVideoCodecVP9: {
      webrtc::CodecSpecificInfoVP9& vp9 = info.codecSpecific.VP9;
      if (metadata.vp9) {
        // Temporal and/or spatial layer stream.
        if (!metadata.vp9->spatial_layer_resolutions.empty() &&
            expected_resolutions != metadata.vp9->spatial_layer_resolutions) {
          NotifyErrorStatus(
              {media::EncoderStatus::Codes::kEncoderFailedEncode,
               "Encoded SVC resolution set does not match request"});
          return;
        }

        const uint8_t spatial_index = metadata.vp9->spatial_idx;
        if (spatial_index >= current_spatial_layer_resolutions_.size()) {
          NotifyErrorStatus({media::EncoderStatus::Codes::kInvalidOutputBuffer,
                             "invalid spatial index"});
          return;
        }
        image.SetSpatialIndex(spatial_index);
        image._encodedWidth =
            current_spatial_layer_resolutions_[spatial_index].width();
        image._encodedHeight =
            current_spatial_layer_resolutions_[spatial_index].height();

        vp9.first_frame_in_picture = spatial_index == 0;
        vp9.inter_pic_predicted = metadata.vp9->inter_pic_predicted;
        vp9.non_ref_for_inter_layer_pred =
            !metadata.vp9->referenced_by_upper_spatial_layers;
        vp9.temporal_idx = metadata.vp9->temporal_idx;
        vp9.temporal_up_switch = metadata.vp9->temporal_up_switch;
        vp9.inter_layer_predicted =
            metadata.vp9->reference_lower_spatial_layers;
        vp9.num_ref_pics = metadata.vp9->p_diffs.size();
        for (size_t i = 0; i < metadata.vp9->p_diffs.size(); ++i)
          vp9.p_diff[i] = metadata.vp9->p_diffs[i];
        vp9.ss_data_available = metadata.key_frame;
        vp9.first_active_layer = 0;
        vp9.num_spatial_layers = current_spatial_layer_resolutions_.size();
        if (vp9.ss_data_available) {
          vp9.spatial_layer_resolution_present = true;
          vp9.gof.num_frames_in_gof = 0;
          for (size_t i = 0; i < vp9.num_spatial_layers; ++i) {
            vp9.width[i] = current_spatial_layer_resolutions_[i].width();
            vp9.height[i] = current_spatial_layer_resolutions_[i].height();
          }
        }
        vp9.flexible_mode = true;
        vp9.gof_idx = 0;
        info.end_of_picture = metadata.vp9->end_of_picture;
      } else {
        // Simple stream, neither temporal nor spatial layer stream.
        vp9.flexible_mode = false;
        vp9.temporal_idx = webrtc::kNoTemporalIdx;
        vp9.temporal_up_switch = true;
        vp9.inter_layer_predicted = false;
        vp9.gof_idx = 0;
        vp9.num_spatial_layers = 1;
        vp9.first_frame_in_picture = true;
        vp9.spatial_layer_resolution_present = false;
        vp9.inter_pic_predicted = !metadata.key_frame;
        vp9.ss_data_available = metadata.key_frame;
        if (vp9.ss_data_available) {
          vp9.spatial_layer_resolution_present = true;
          vp9.width[0] = image._encodedWidth;
          vp9.height[0] = image._encodedHeight;
          vp9.gof.num_frames_in_gof = 1;
          vp9.gof.temporal_idx[0] = 0;
          vp9.gof.temporal_up_switch[0] = false;
          vp9.gof.num_ref_pics[0] = 1;
          vp9.gof.pid_diff[0][0] = 1;
        }
        info.end_of_picture = true;
      }
    } break;
    default:
      break;
  }

  base::AutoLock lock(lock_);
  if (!encoded_image_callback_)
    return;

  const auto result = encoded_image_callback_->OnEncodedImage(image, &info);
  if (result.error != webrtc::EncodedImageCallback::Result::OK) {
    DVLOG(2)
        << "ReturnEncodedImage(): webrtc::EncodedImageCallback::Result.error = "
        << result.error;
  }
}

void RTCVideoEncoder::Impl::NotifyErrorStatus(
    const media::EncoderStatus& status) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::NotifyErrorStatus");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!status.is_ok());
  LOG(ERROR) << "NotifyErrorStatus is called with code="
             << static_cast<int>(status.code())
             << ", message=" << status.message();
  // TODO(b/275663480): Deprecate RTCVideoEncoderShutdownReason
  // in favor of UKM.
  int32_t retval = WEBRTC_VIDEO_CODEC_ERROR;
  switch (media::ConvertStatusToVideoEncodeAcceleratorError(status)) {
    case media::VideoEncodeAccelerator::kInvalidArgumentError:
      retval = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
      RecordEncoderShutdownReasonUMA(
          RTCVideoEncoderShutdownReason::kInvalidArgument, video_codec_type_);
      break;
    case media::VideoEncodeAccelerator::kIllegalStateError:
      RecordEncoderShutdownReasonUMA(
          RTCVideoEncoderShutdownReason::kIllegalState, video_codec_type_);
      retval = WEBRTC_VIDEO_CODEC_ERROR;
      break;
    case media::VideoEncodeAccelerator::kPlatformFailureError:
      // Some platforms(i.e. Android) do not have SW H264 implementation so
      // check if it is available before asking for fallback.
      retval = video_codec_type_ != webrtc::kVideoCodecH264 ||
                       webrtc::H264Encoder::IsSupported()
                   ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                   : WEBRTC_VIDEO_CODEC_ERROR;
      RecordEncoderShutdownReasonUMA(
          RTCVideoEncoderShutdownReason::kPlatformFailure, video_codec_type_);
  }
  video_encoder_.reset();

  status_ = retval;

  async_init_event_.SetAndReset(retval);
  async_encode_event_.SetAndReset(retval);

  execute_software_fallback_.Run();
}

RTCVideoEncoder::Impl::~Impl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordTimestampMatchUMA();
  if (video_encoder_) {
    video_encoder_.reset();
    status_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    RecordEncoderShutdownReasonUMA(
        RTCVideoEncoderShutdownReason::kSuccessfulRelease, video_codec_type_);
  }

  async_init_event_.reset();
  async_encode_event_.reset();

  // weak_this_ must be invalidated in |gpu_task_runner_|.
  weak_this_factory_.InvalidateWeakPtrs();
}

void RTCVideoEncoder::Impl::EncodeOneFrame(FrameChunk frame_chunk) {
  DVLOG(3) << "Impl::EncodeOneFrame()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!input_buffers_free_.empty());
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::Impl::EncodeOneFrame", "timestamp",
               frame_chunk.timestamp);

  if (!video_encoder_) {
    async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }

  const base::TimeDelta timestamp =
      base::Microseconds(frame_chunk.timestamp_us);

  scoped_refptr<media::VideoFrame> frame;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
      frame_chunk.video_frame_buffer;

  // All non-native frames require a copy because we can't tell if non-copy
  // conditions are met.
  bool requires_copy_or_scale =
      frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative;
  if (!requires_copy_or_scale) {
    const WebRtcVideoFrameAdapter* frame_adapter =
        static_cast<WebRtcVideoFrameAdapter*>(frame_buffer.get());
    frame = frame_adapter->getMediaVideoFrame();
    frame->set_timestamp(timestamp);
    const media::VideoFrame::StorageType storage = frame->storage_type();
    const bool is_memory_based_frame =
        storage == media::VideoFrame::STORAGE_UNOWNED_MEMORY ||
        storage == media::VideoFrame::STORAGE_OWNED_MEMORY ||
        storage == media::VideoFrame::STORAGE_SHMEM;
    const bool is_gmb_frame =
        storage == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER;
    const bool is_right_format = frame->format() == media::PIXEL_FORMAT_I420 ||
                                 frame->format() == media::PIXEL_FORMAT_NV12;
    requires_copy_or_scale = !is_right_format || RequiresSizeChange(*frame) ||
                             !(is_memory_based_frame || is_gmb_frame);
  }

  if (requires_copy_or_scale) {
    TRACE_EVENT0("webrtc",
                 "RTCVideoEncoder::Impl::EncodeOneFrame::CopyOrScale");
    // Native buffer scaling is performed by WebRtcVideoFrameAdapter, which may
    // be more efficient in some cases. E.g. avoiding I420 conversion or scaling
    // from a middle layer instead of top layer.
    //
    // Native buffer scaling is only supported when `input_frame_coded_size_`
    // and `input_visible_size_` strides match. This ensures the strides of the
    // frame that we pass to the encoder fits the input requirements.
    bool native_buffer_scaling =
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
        frame_buffer->type() == webrtc::VideoFrameBuffer::Type::kNative &&
        input_frame_coded_size_ == input_visible_size_;
#else
        // TODO(https://crbug.com/1307206): Android (e.g. android-pie-arm64-rel)
        // and CrOS does not support the native buffer scaling path. Investigate
        // why and find a way to enable it, if possible.
        false;
#endif
    if (native_buffer_scaling) {
      DCHECK_EQ(frame_buffer->type(), webrtc::VideoFrameBuffer::Type::kNative);
      auto scaled_buffer = frame_buffer->Scale(input_visible_size_.width(),
                                               input_visible_size_.height());
      auto mapped_buffer =
          scaled_buffer->GetMappedFrameBuffer(preferred_pixel_formats_);
      if (!mapped_buffer) {
        NotifyErrorStatus({media::EncoderStatus::Codes::kSystemAPICallError,
                           "Failed to map a buffer"});
        return;
      }

      mapped_buffer = scaled_buffer->ToI420();
      if (!mapped_buffer) {
        NotifyErrorStatus({media::EncoderStatus::Codes::kFormatConversionError,
                           "Failed to convert a buffer to I420"});
        return;
      }
      DCHECK_NE(mapped_buffer->type(), webrtc::VideoFrameBuffer::Type::kNative);
      frame = ConvertFromMappedWebRtcVideoFrameBuffer(mapped_buffer, timestamp);
      if (!frame) {
        NotifyErrorStatus(
            {media::EncoderStatus::Codes::kFormatConversionError,
             "Failed to convert WebRTC mapped buffer to media::VideoFrame"});
        return;
      }
    } else {
      const int index = input_buffers_free_.back();
      if (!input_buffers_[index]) {
        const size_t input_frame_buffer_size =
            media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420,
                                              input_frame_coded_size_);
        input_buffers_[index] = std::make_unique<base::MappedReadOnlyRegion>(
            base::ReadOnlySharedMemoryRegion::Create(input_frame_buffer_size));
        if (!input_buffers_[index]) {
          NotifyErrorStatus({media::EncoderStatus::Codes::kSystemAPICallError,
                             "Failed to create input buffer"});
          return;
        }
      }

      auto& region = input_buffers_[index]->region;
      auto& mapping = input_buffers_[index]->mapping;
      frame = media::VideoFrame::WrapExternalData(
          media::PIXEL_FORMAT_I420, input_frame_coded_size_,
          gfx::Rect(input_visible_size_), input_visible_size_,
          static_cast<uint8_t*>(mapping.memory()), mapping.size(), timestamp);
      if (!frame) {
        NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderFailedEncode,
                           "Failed to create input buffer"});
        return;
      }

      // |frame| is STORAGE_UNOWNED_MEMORY at this point. Writing the data is
      // allowed.
      // Do a strided copy and scale (if necessary) the input frame to match
      // the input requirements for the encoder.
      // TODO(magjed): Downscale with an image pyramid instead.
      rtc::scoped_refptr<webrtc::I420BufferInterface> i420_buffer =
          frame_buffer->ToI420();
      if (libyuv::I420Scale(
              i420_buffer->DataY(), i420_buffer->StrideY(),
              i420_buffer->DataU(), i420_buffer->StrideU(),
              i420_buffer->DataV(), i420_buffer->StrideV(),
              i420_buffer->width(), i420_buffer->height(),
              frame->GetWritableVisibleData(media::VideoFrame::kYPlane),
              frame->stride(media::VideoFrame::kYPlane),
              frame->GetWritableVisibleData(media::VideoFrame::kUPlane),
              frame->stride(media::VideoFrame::kUPlane),
              frame->GetWritableVisibleData(media::VideoFrame::kVPlane),
              frame->stride(media::VideoFrame::kVPlane),
              frame->visible_rect().width(), frame->visible_rect().height(),
              libyuv::kFilterBox)) {
        NotifyErrorStatus({media::EncoderStatus::Codes::kFormatConversionError,
                           "Failed to copy buffer"});
        return;
      }

      // |frame| becomes STORAGE_SHMEM. Writing the buffer is not permitted
      // after here.
      frame->BackWithSharedMemory(&region);

      input_buffers_free_.pop_back();
      frame->AddDestructionObserver(
          base::BindPostTaskToCurrentDefault(WTF::BindOnce(
              &RTCVideoEncoder::Impl::InputBufferReleased, weak_this_, index)));
    }
  }

  if (!failed_timestamp_match_) {
    DCHECK(!base::Contains(submitted_frames_, timestamp,
                           &FrameInfo::media_timestamp_));
    submitted_frames_.emplace_back(timestamp, frame_chunk.timestamp,
                                   frame_chunk.render_time_ms,
                                   ActiveSpatialResolutions());
  }

  // Call UseOutputBitstreamBuffer() for pending output buffers.
  for (const auto& bitstream_buffer_id : pending_output_buffers_) {
    UseOutputBitstreamBuffer(bitstream_buffer_id);
  }
  pending_output_buffers_.clear();

  frames_in_encoder_count_++;
  video_encoder_->Encode(frame, frame_chunk.force_keyframe);
  async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::EncodeOneFrameWithNativeInput(
    FrameChunk frame_chunk) {
  DVLOG(3) << "Impl::EncodeOneFrameWithNativeInput()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(input_buffers_.empty() && input_buffers_free_.empty());
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::Impl::EncodeOneFrameWithNativeInput",
               "timestamp", frame_chunk.timestamp);

  if (!video_encoder_) {
    async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }

  scoped_refptr<media::VideoFrame> frame;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
      frame_chunk.video_frame_buffer;
  if (frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative) {
    // If we get a non-native frame it's because the video track is disabled and
    // WebRTC VideoBroadcaster replaces the camera frame with a black YUV frame.
    if (!black_gmb_frame_) {
      gfx::Size natural_size(frame_buffer->width(), frame_buffer->height());
      if (!CreateBlackGpuMemoryBufferFrame(natural_size)) {
        DVLOG(2) << "Failed to allocate native buffer for black frame";
        async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERROR);
        return;
      }
    }
    frame = media::VideoFrame::WrapVideoFrame(
        black_gmb_frame_, black_gmb_frame_->format(),
        black_gmb_frame_->visible_rect(), black_gmb_frame_->natural_size());
  } else {
    frame = static_cast<WebRtcVideoFrameAdapter*>(frame_buffer.get())
                ->getMediaVideoFrame();
  }
  frame->set_timestamp(base::Microseconds(frame_chunk.timestamp_us));

  if (frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kInvalidInputFrame,
                       "frame isn't GpuMemoryBuffer based VideoFrame"});
    return;
  }

  if (!failed_timestamp_match_) {
    DCHECK(!base::Contains(submitted_frames_, frame->timestamp(),
                           &FrameInfo::media_timestamp_));
    submitted_frames_.emplace_back(frame->timestamp(), frame_chunk.timestamp,
                                   frame_chunk.render_time_ms,
                                   ActiveSpatialResolutions());
  }

  // Call UseOutputBitstreamBuffer() for pending output buffers.
  for (const auto& bitstream_buffer_id : pending_output_buffers_) {
    UseOutputBitstreamBuffer(bitstream_buffer_id);
  }
  pending_output_buffers_.clear();

  frames_in_encoder_count_++;
  video_encoder_->Encode(frame, frame_chunk.force_keyframe);
  async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_OK);
}

bool RTCVideoEncoder::Impl::CreateBlackGpuMemoryBufferFrame(
    const gfx::Size& natural_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto gmb = gpu_factories_->CreateGpuMemoryBuffer(
      natural_size, gfx::BufferFormat::YUV_420_BIPLANAR,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);

  if (!gmb || !gmb->Map()) {
    black_gmb_frame_ = nullptr;
    return false;
  }
  // Fills the NV12 frame with YUV black (0x00, 0x80, 0x80).
  const auto gmb_size = gmb->GetSize();
  memset(static_cast<uint8_t*>(gmb->memory(0)), 0x0,
         gmb->stride(0) * gmb_size.height());
  memset(static_cast<uint8_t*>(gmb->memory(1)), 0x80,
         gmb->stride(1) * gmb_size.height() / 2);
  gmb->Unmap();

  gpu::MailboxHolder empty_mailboxes[media::VideoFrame::kMaxPlanes];
  black_gmb_frame_ = media::VideoFrame::WrapExternalGpuMemoryBuffer(
      gfx::Rect(gmb_size), natural_size, std::move(gmb), empty_mailboxes,
      base::NullCallback(), base::TimeDelta());
  return true;
}

void RTCVideoEncoder::Impl::InputBufferReleased(int index) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::InputBufferReleased");
  DVLOG(3) << "Impl::InputBufferReleased(): index=" << index;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!use_native_input_);

  // NotfiyError() has been called. Don't proceed the frame completion.
  if (!video_encoder_)
    return;

  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(input_buffers_.size()));
  input_buffers_free_.push_back(index);

  while (!pending_frames_.empty() && !input_buffers_free_.empty()) {
    auto chunk = std::move(pending_frames_.front());
    pending_frames_.pop_front();
    EncodeOneFrame(std::move(chunk));
  }
}

bool RTCVideoEncoder::Impl::RequiresSizeChange(
    const media::VideoFrame& frame) const {
  return (frame.coded_size() != input_frame_coded_size_ ||
          frame.visible_rect() != gfx::Rect(input_visible_size_));
}

void RTCVideoEncoder::Impl::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DVLOG(3) << __func__;
  base::AutoLock lock(lock_);
  encoded_image_callback_ = callback;
}

RTCVideoEncoder::RTCVideoEncoder(
    media::VideoCodecProfile profile,
    bool is_constrained_h264,
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : profile_(profile),
      is_constrained_h264_(is_constrained_h264),
      gpu_factories_(gpu_factories),
      gpu_task_runner_(gpu_factories->GetTaskRunner()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  DVLOG(1) << "RTCVideoEncoder(): profile=" << GetProfileName(profile);

  // The default values of EncoderInfo.
  encoder_info_.scaling_settings = webrtc::VideoEncoder::ScalingSettings::kOff;
  encoder_info_.requested_resolution_alignment = 1;
  encoder_info_.apply_alignment_to_all_simulcast_layers = false;
  encoder_info_.supports_native_handle = true;
  encoder_info_.implementation_name = "ExternalEncoder";
  encoder_info_.has_trusted_rate_controller = false;
  encoder_info_.is_hardware_accelerated = true;
  encoder_info_.is_qp_trusted = true;
  encoder_info_.fps_allocation[0] = {
      webrtc::VideoEncoder::EncoderInfo::kMaxFramerateFraction};
  DCHECK(encoder_info_.resolution_bitrate_limits.empty());
  encoder_info_.supports_simulcast = false;
  encoder_info_.preferred_pixel_formats = {
      webrtc::VideoFrameBuffer::Type::kI420};

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

RTCVideoEncoder::~RTCVideoEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  DVLOG(3) << __func__;

  // |weak_this_| must be invalidated on |webrtc_sequence_checker_|.
  weak_this_factory_.InvalidateWeakPtrs();

  Release();
  DCHECK(!impl_);
}

bool RTCVideoEncoder::IsCodecInitializationPending() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  return vea_config_.has_value();
}

int32_t RTCVideoEncoder::InitializeEncoder(
    const media::VideoEncodeAccelerator::Config& vea_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::InitEncode", "config",
               vea_config.AsHumanReadableString());
  DVLOG(1) << __func__ << ": config=" << vea_config.AsHumanReadableString();
  auto init_start = base::TimeTicks::Now();
  // This wait is necessary because this task is completed in GPU process
  // asynchronously but WebRTC API is synchronous.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent initialization_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t initialization_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::CreateAndInitializeVEA, weak_impl_,
          vea_config,
          SignaledValue(&initialization_waiter, &initialization_retval)));
  // webrtc::VideoEncoder expects this call to be synchronous.
  initialization_waiter.Wait();
  if (initialization_retval == WEBRTC_VIDEO_CODEC_OK) {
    UMA_HISTOGRAM_TIMES("WebRTC.RTCVideoEncoder.Initialize",
                        base::TimeTicks::Now() - init_start);
  }
  RecordInitEncodeUMA(initialization_retval, profile_);
  return initialization_retval;
}

int32_t RTCVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    const webrtc::VideoEncoder::Settings& settings) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::InitEncode");
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  DVLOG(1) << __func__ << " codecType=" << codec_settings->codecType
           << ", width=" << codec_settings->width
           << ", height=" << codec_settings->height
           << ", startBitrate=" << codec_settings->startBitrate;

  if (profile_ >= media::H264PROFILE_MIN &&
      profile_ <= media::H264PROFILE_MAX &&
      (codec_settings->width % 2 != 0 || codec_settings->height % 2 != 0)) {
    DLOG(ERROR)
        << "Input video size is " << codec_settings->width << "x"
        << codec_settings->height << ", "
        << "but hardware H.264 encoder only supports even sized frames.";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  if (impl_)
    Release();

  has_error_ = false;

  uint32_t bitrate_bps = 0;
  // Check for overflow converting bitrate (kilobits/sec) to bits/sec.
  if (!ConvertKbpsToBps(codec_settings->startBitrate, &bitrate_bps)) {
    DLOG(ERROR) << "Overflow converting bitrate from kbps to bps: bps="
                << codec_settings->startBitrate;
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>
      spatial_layers;
  auto inter_layer_pred =
      media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOff;
  if (!CreateSpatialLayersConfig(*codec_settings, &spatial_layers,
                                 &inter_layer_pred)) {
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  gfx::Size input_visible_size(codec_settings->width, codec_settings->height);
  // Check that |profile| supports |input_visible_size|.
  if (base::FeatureList::IsEnabled(features::kWebRtcUseMinMaxVEADimensions)) {
    const auto vea_supported_profiles =
        gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
            media::VideoEncodeAccelerator::SupportedProfiles());

    for (const auto& vea_profile : vea_supported_profiles) {
      if (vea_profile.profile == profile_ &&
          (input_visible_size.width() > vea_profile.max_resolution.width() ||
           input_visible_size.height() > vea_profile.max_resolution.height() ||
           input_visible_size.width() < vea_profile.min_resolution.width() ||
           input_visible_size.height() < vea_profile.min_resolution.height())) {
        DLOG(ERROR) << "Requested dimensions (" << input_visible_size.ToString()
                    << ") beyond accelerator limits ("
                    << vea_profile.min_resolution.ToString() << " - "
                    << vea_profile.max_resolution.ToString() << ")";
        return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
      }
    }
  }

  auto webrtc_content_type = webrtc::VideoContentType::UNSPECIFIED;
  auto vea_content_type =
      media::VideoEncodeAccelerator::Config::ContentType::kCamera;
  if (codec_settings->mode == webrtc::VideoCodecMode::kScreensharing) {
    webrtc_content_type = webrtc::VideoContentType::SCREENSHARE;
    vea_content_type =
        media::VideoEncodeAccelerator::Config::ContentType::kDisplay;
  }

  // base::Unretained(this) is safe because |impl_| is synchronously destroyed
  // in Release() so that |impl_| does not call UpdateEncoderInfo() after this
  // is destructed.
  Impl::UpdateEncoderInfoCallback update_encoder_info_callback =
      base::BindRepeating(&RTCVideoEncoder::UpdateEncoderInfo,
                          base::Unretained(this));
  base::RepeatingClosure execute_software_fallback =
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&RTCVideoEncoder::SetError, weak_this_));

  impl_ = std::make_unique<Impl>(
      gpu_factories_, ProfileToWebRtcVideoCodecType(profile_),
      webrtc_content_type, update_encoder_info_callback,
      execute_software_fallback, weak_impl_);

  media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420;
  auto storage_type =
      media::VideoEncodeAccelerator::Config::StorageType::kShmem;
  if (IsZeroCopyEnabled(webrtc_content_type)) {
    pixel_format = media::PIXEL_FORMAT_NV12;
    storage_type =
        media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
  }

  vea_config_ = media::VideoEncodeAccelerator::Config(
      pixel_format, input_visible_size, profile_,
      media::Bitrate::ConstantBitrate(bitrate_bps),
      /*initial_framerate=*/absl::nullopt,
      /*gop_length=*/absl::nullopt,
      /*h264_output_level=*/absl::nullopt, is_constrained_h264_, storage_type,
      vea_content_type, spatial_layers, inter_layer_pred);

  if (!base::FeatureList::IsEnabled(features::kWebRtcInitializeOnFirstFrame)) {
    int32_t initialization_ret = InitializeEncoder(*vea_config_);
    vea_config_.reset();
    if (initialization_ret != WEBRTC_VIDEO_CODEC_OK) {
      Release();
      CHECK(!impl_);
    }
    return initialization_ret;
  }

  // If we initialize the encoder on the first Encode(), then |encoder_info| is
  // not updated by the encoder implementation. But the RTCVideoEncoder client
  // gets |encoder_info_|. So we update |encoder_info_| from the configured
  // |spatial_layers| and |pixel_format| as if it is updated the encoder
  // implementation.
  PreInitializeEncoder(spatial_layers, pixel_format);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoder::Encode(
    const webrtc::VideoFrame& input_image,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::Encode", "timestamp",
               input_image.timestamp());
  DVLOG(3) << __func__;
  if (!impl_) {
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (has_error_)
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;

  if (IsCodecInitializationPending()) {
    if (IsNV12GpuMemoryBufferVideoFrame(input_image) &&
        SupportGpuMemoryBufferEncoding()) {
      vea_config_->input_format = media::PIXEL_FORMAT_NV12;
      vea_config_->storage_type =
          media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
    } else {
      vea_config_->input_format = media::PIXEL_FORMAT_I420;
      vea_config_->storage_type =
          media::VideoEncodeAccelerator::Config::StorageType::kShmem;
    }
    int32_t initialization_val = InitializeEncoder(*vea_config_);
    vea_config_.reset();
    if (initialization_val != WEBRTC_VIDEO_CODEC_OK) {
      SetError();
      Release();
      CHECK(!impl_);
      pending_rate_params_.reset();
      return initialization_val;
    }
    if (pending_rate_params_) {
      SetRates(*pending_rate_params_);
      pending_rate_params_.reset();
    }
  }

  const bool want_key_frame =
      frame_types && frame_types->size() &&
      frame_types->front() == webrtc::VideoFrameType::kVideoFrameKey;
  if (base::FeatureList::IsEnabled(features::kWebRtcEncoderAsyncEncode)) {
    PostCrossThreadTask(
        *gpu_task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(&RTCVideoEncoder::Impl::Enqueue, weak_impl_,
                            FrameChunk(input_image, want_key_frame),
                            SignaledValue()));
    return WEBRTC_VIDEO_CODEC_OK;
  } else {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    base::WaitableEvent encode_waiter(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    int32_t encode_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    PostCrossThreadTask(
        *gpu_task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(&RTCVideoEncoder::Impl::Enqueue, weak_impl_,
                            FrameChunk(input_image, want_key_frame),
                            SignaledValue(&encode_waiter, &encode_retval)));
    encode_waiter.Wait();
    DVLOG(3) << "Encode(): returning encode_retval=" << encode_retval;
    return encode_retval;
  }
}

int32_t RTCVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  DVLOG(3) << __func__;
  if (!impl_) {
    if (!callback)
      return WEBRTC_VIDEO_CODEC_OK;
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // TOD(b/257021675): RegisterEncodeCompleteCallback() should be called twice,
  // with a valid pointer after InitEncode() and with a nullptr after Release().
  // Setting callback in |impl_| should be done asynchronously by posting the
  // task to |media_task_runner_|.
  // However, RegisterEncodeCompleteCallback() are actually called multiple
  // times with valid pointers, this may be a bug. To workaround this problem,
  // a mutex is used so that it is guaranteed that the previous callback is not
  // executed after RegisterEncodeCompleteCallback().
  impl_->RegisterEncodeCompleteCallback(callback);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoder::Release() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  DVLOG(3) << __func__;
  if (!impl_)
    return WEBRTC_VIDEO_CODEC_OK;

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent release_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<Impl> impl, base::WaitableEvent* waiter) {
            impl.reset();
            waiter->Signal();
          },
          std::move(impl_), CrossThreadUnretained(&release_waiter)));

  release_waiter.Wait();

  // The object pointed by |weak_impl_| has been invalidated in Impl destructor.
  // Calling reset() is optional, but it's good to invalidate the value of
  // |weak_impl_| too
  weak_impl_.reset();

  return WEBRTC_VIDEO_CODEC_OK;
}

void RTCVideoEncoder::SetRates(
    const webrtc::VideoEncoder::RateControlParameters& parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  TRACE_EVENT1("webrtc", "SetRates", "parameters",
               parameters.bitrate.ToString());
  DVLOG(3) << __func__ << " new_bit_rate=" << parameters.bitrate.ToString()
           << ", frame_rate=" << parameters.framerate_fps;
  if (!impl_) {
    DVLOG(3) << "Encoder is not initialized";
    return;
  }

  if (has_error_)
    return;

  if (IsCodecInitializationPending()) {
    pending_rate_params_ = parameters;
    return;
  }
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::RequestEncodingParametersChange, weak_impl_,
          parameters));
  return;
}

webrtc::VideoEncoder::EncoderInfo RTCVideoEncoder::GetEncoderInfo() const {
  base::AutoLock auto_lock(lock_);
  return encoder_info_;
}

void RTCVideoEncoder::PreInitializeEncoder(
    const std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>&
        spatial_layers,
    media::VideoPixelFormat pixel_format) {
  size_t num_spatial_layers = 1;
  size_t num_temporal_layers = 1;
  if (!spatial_layers.empty()) {
    num_spatial_layers = spatial_layers.size();
    num_temporal_layers = spatial_layers[0].num_of_temporal_layers;
  }

  base::AutoLock auto_lock(lock_);
  for (size_t i = 0; i < num_spatial_layers; ++i) {
    switch (num_temporal_layers) {
      case 1:
        encoder_info_.fps_allocation[i] = {255};
        break;
      case 2:
        encoder_info_.fps_allocation[i] = {255 / 2, 255};
        break;
      case 3:
        encoder_info_.fps_allocation[i] = {255 / 4, 255 / 2, 255};
        break;
      default:
        NOTREACHED() << "Unexpected temporal layers: " << num_temporal_layers;
    }
  }
  auto preferred_pixel_format = pixel_format == media::PIXEL_FORMAT_I420
                                    ? webrtc::VideoFrameBuffer::Type::kI420
                                    : webrtc::VideoFrameBuffer::Type::kNV12;
  encoder_info_.preferred_pixel_formats = {preferred_pixel_format};
}

void RTCVideoEncoder::UpdateEncoderInfo(
    media::VideoEncoderInfo media_enc_info,
    std::vector<webrtc::VideoFrameBuffer::Type> preferred_pixel_formats) {
  // See b/261437029#comment7 why this needs to be done in |gpu_task_runner_|.
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(lock_);

  encoder_info_.implementation_name = media_enc_info.implementation_name;
  encoder_info_.supports_native_handle = media_enc_info.supports_native_handle;
  encoder_info_.has_trusted_rate_controller =
      media_enc_info.has_trusted_rate_controller;
  encoder_info_.is_hardware_accelerated =
      media_enc_info.is_hardware_accelerated;
  encoder_info_.supports_simulcast = media_enc_info.supports_simulcast;
  encoder_info_.is_qp_trusted = media_enc_info.reports_average_qp;
  encoder_info_.requested_resolution_alignment =
      media_enc_info.requested_resolution_alignment;
  encoder_info_.apply_alignment_to_all_simulcast_layers =
      media_enc_info.apply_alignment_to_all_simulcast_layers;
  static_assert(
      webrtc::kMaxSpatialLayers >= media::VideoEncoderInfo::kMaxSpatialLayers,
      "webrtc::kMaxSpatiallayers is less than "
      "media::VideoEncoderInfo::kMaxSpatialLayers");
  for (size_t i = 0; i < std::size(media_enc_info.fps_allocation); ++i) {
    if (media_enc_info.fps_allocation[i].empty())
      continue;
    encoder_info_.fps_allocation[i] =
        absl::InlinedVector<uint8_t, webrtc::kMaxTemporalStreams>(
            media_enc_info.fps_allocation[i].begin(),
            media_enc_info.fps_allocation[i].end());
  }
  for (const auto& limit : media_enc_info.resolution_bitrate_limits) {
    encoder_info_.resolution_bitrate_limits.emplace_back(
        limit.frame_size.GetArea(), limit.min_start_bitrate_bps,
        limit.min_bitrate_bps, limit.max_bitrate_bps);
  }
  encoder_info_.preferred_pixel_formats.assign(preferred_pixel_formats.begin(),
                                               preferred_pixel_formats.end());
}

void RTCVideoEncoder::SetError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  has_error_ = true;

  if (error_callback_for_testing_)
    std::move(error_callback_for_testing_).Run();
}

// static
bool RTCVideoEncoder::Vp9HwSupportForSpatialLayers() {
#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(media::kVaapiVp9kSVCHWEncoding);
#else
  // Spatial layers are not supported by hardware encoders.
  return false;
#endif
}

}  // namespace blink
