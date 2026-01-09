// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/android/shared/video_decoder.h"

#include <android/api-level.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <list>

#include "base/android/jni_android.h"
#include "build/build_config.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/video_render_algorithm.h"
#include "starboard/android/shared/video_surface_texture_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/player.h"
#include "starboard/common/size.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/drm.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/thread.h"

namespace starboard {

namespace {

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using std::placeholders::_1;
using std::placeholders::_2;

bool IsSoftwareDecodeRequired(const std::string& max_video_capabilities) {
  if (max_video_capabilities.empty()) {
    SB_LOG(INFO)
        << "Use hardware decoder as `max_video_capabilities` is empty.";
    return false;
  }

  // `max_video_capabilities` is in the form of mime type attributes, like
  // "width=1920; height=1080; ...".  Prepend valid mime type/subtype and codecs
  // so it can be parsed by MimeType.
  MimeType mime_type("video/mp4; codecs=\"vp9\"; " + max_video_capabilities);
  if (!mime_type.is_valid()) {
    SB_LOG(INFO) << "Use hardware decoder as `max_video_capabilities` ("
                 << max_video_capabilities << ") is invalid.";
    return false;
  }

  std::string software_decoder_expectation =
      mime_type.GetParamStringValue("softwaredecoder", "");
  if (software_decoder_expectation == "required" ||
      software_decoder_expectation == "preferred") {
    SB_LOG(INFO) << "Use software decoder as `softwaredecoder` is set to \""
                 << software_decoder_expectation << "\".";
    return true;
  } else if (software_decoder_expectation == "disallowed" ||
             software_decoder_expectation == "unpreferred") {
    SB_LOG(INFO) << "Use hardware decoder as `softwaredecoder` is set to \""
                 << software_decoder_expectation << "\".";
    return false;
  }

  bool is_low_resolution = mime_type.GetParamIntValue("width", 1920) <= 432 &&
                           mime_type.GetParamIntValue("height", 1080) <= 240;
  bool is_low_fps = mime_type.GetParamIntValue("framerate", 30) <= 15;

  if (is_low_resolution && is_low_fps) {
    // Workaround to be compatible with existing backend implementation.
    SB_LOG(INFO) << "Use software decoder as `max_video_capabilities` ("
                 << max_video_capabilities
                 << ") indicates a low resolution and low fps playback.";
    return true;
  }

  SB_LOG(INFO)
      << "Use hardware decoder as `max_video_capabilities` is set to \""
      << max_video_capabilities << "\".";
  return false;
}

std::optional<Size> ParseMaxResolution(
    const std::string& max_video_capabilities,
    const Size& frame_size) {
  SB_DCHECK_GT(frame_size.width, 0);
  SB_DCHECK_GT(frame_size.height, 0);

  if (max_video_capabilities.empty()) {
    SB_LOG(INFO)
        << "Didn't parse max resolutions as `max_video_capabilities` is empty.";
    return std::nullopt;
  }

  SB_LOG(INFO) << "Try to parse max resolutions from `max_video_capabilities` ("
               << max_video_capabilities << ").";

  // `max_video_capabilities` is in the form of mime type attributes, like
  // "width=1920; height=1080; ...".  Prepend valid mime type/subtype and codecs
  // so it can be parsed by MimeType.
  MimeType mime_type("video/mp4; codecs=\"vp9\"; " + max_video_capabilities);
  if (!mime_type.is_valid()) {
    SB_LOG(WARNING) << "Failed to parse max resolutions as "
                       "`max_video_capabilities` is invalid.";
    return std::nullopt;
  }

  int width = mime_type.GetParamIntValue("width", -1);
  int height = mime_type.GetParamIntValue("height", -1);
  if (width <= 0 && height <= 0) {
    SB_LOG(WARNING) << "Failed to parse max resolutions as either width or "
                       "height isn't set.";
    return std::nullopt;
  }
  if (width != -1 && height != -1) {
    const Size max_size = {width, height};
    SB_LOG(INFO) << "Parsed max resolution=" << max_size;
    return max_size;
  }

  if (frame_size.width <= 0 || frame_size.height <= 0) {
    // We DCHECK() above, but just be safe.
    SB_LOG(WARNING)
        << "Failed to parse max resolutions due to invalid frame resolutions ("
        << frame_size << ").";
    return std::nullopt;
  }

  if (width > 0) {
    const Size max_size = {width, width * frame_size.height / frame_size.width};
    SB_LOG(INFO) << "Inferred max size=" << max_size
                 << ", frame resolution=" << frame_size;
    return max_size;
  }

  if (height > 0) {
    const Size max_size = {height * frame_size.width / frame_size.height,
                           height};
    SB_LOG(INFO) << "Inferred max size=" << max_size
                 << ", frame resolution=" << frame_size;
  }
  return std::nullopt;
}

class VideoFrameImpl : public VideoFrame {
 public:
  typedef std::function<void()> VideoFrameReleaseCallback;

  VideoFrameImpl(const DequeueOutputResult& dequeue_output_result,
                 MediaCodecBridge* media_codec_bridge,
                 const VideoFrameReleaseCallback& release_callback)
      : VideoFrame(dequeue_output_result.flags & BUFFER_FLAG_END_OF_STREAM
                       ? kMediaTimeEndOfStream
                       : dequeue_output_result.presentation_time_microseconds),
        dequeue_output_result_(dequeue_output_result),
        media_codec_bridge_(media_codec_bridge),
        released_(false),
        release_callback_(release_callback) {
    SB_DCHECK(media_codec_bridge_);
    SB_DCHECK(release_callback_);
  }

  ~VideoFrameImpl() {
    if (!released_) {
      media_codec_bridge_->ReleaseOutputBuffer(dequeue_output_result_.index,
                                               false);
      if (!is_end_of_stream()) {
        release_callback_();
      }
    }
  }

  void Draw(int64_t release_time_in_nanoseconds) {
    SB_DCHECK(!released_);
    SB_DCHECK(!is_end_of_stream());
    released_ = true;
    media_codec_bridge_->ReleaseOutputBufferAtTimestamp(
        dequeue_output_result_.index, release_time_in_nanoseconds);
    release_callback_();
  }

 private:
  DequeueOutputResult dequeue_output_result_;
  MediaCodecBridge* media_codec_bridge_;
  volatile bool released_;
  const VideoFrameReleaseCallback release_callback_;
};

const int64_t kInitialPrerollTimeout = 250'000;                  // 250ms
const int64_t kNeedMoreInputCheckIntervalInTunnelMode = 50'000;  // 50ms

const int kInitialPrerollFrameCount = 8;
const int kNonInitialPrerollFrameCount = 1;

const int kSeekingPrerollPendingWorkSizeInTunnelMode =
    16 + kInitialPrerollFrameCount;
const int kMaxPendingInputsSize = 128;

const int kFpsGuesstimateRequiredInputBufferCount = 3;

// Convenience HDR mastering metadata.
const SbMediaMasteringMetadata kEmptyMasteringMetadata = {};

// Determine if two |SbMediaMasteringMetadata|s are equal.
bool Equal(const SbMediaMasteringMetadata& lhs,
           const SbMediaMasteringMetadata& rhs) {
  return memcmp(&lhs, &rhs, sizeof(SbMediaMasteringMetadata)) == 0;
}

// TODO: For whatever reason, Cobalt will always pass us this us for
// color metadata, regardless of whether HDR is on or not.  Find out if this
// is intentional or not.  It would make more sense if it were NULL.
// Determine if |color_metadata| is "empty", or "null".
bool IsIdentity(const SbMediaColorMetadata& color_metadata) {
  return color_metadata.primaries == kSbMediaPrimaryIdBt709 &&
         color_metadata.transfer == kSbMediaTransferIdBt709 &&
         color_metadata.matrix == kSbMediaMatrixIdBt709 &&
         color_metadata.range == kSbMediaRangeIdLimited &&
         Equal(color_metadata.mastering_metadata, kEmptyMasteringMetadata);
}

void StubDrmSessionUpdateRequestFunc(SbDrmSystem drm_system,
                                     void* context,
                                     int ticket,
                                     SbDrmStatus status,
                                     SbDrmSessionRequestType type,
                                     const char* error_message,
                                     const void* session_id,
                                     int session_id_size,
                                     const void* content,
                                     int content_size,
                                     const char* url) {}

void StubDrmSessionUpdatedFunc(SbDrmSystem drm_system,
                               void* context,
                               int ticket,
                               SbDrmStatus status,
                               const char* error_message,
                               const void* session_id,
                               int session_id_size) {}

void StubDrmSessionKeyStatusesChangedFunc(SbDrmSystem drm_system,
                                          void* context,
                                          const void* session_id,
                                          int session_id_size,
                                          int number_of_keys,
                                          const SbDrmKeyId* key_ids,
                                          const SbDrmKeyStatus* key_statuses) {}

}  // namespace

// TODO: Merge this with VideoFrameTracker, maybe?
class VideoRenderAlgorithmTunneled : public VideoRenderAlgorithm {
 public:
  explicit VideoRenderAlgorithmTunneled(VideoFrameTracker* frame_tracker)
      : frame_tracker_(frame_tracker) {
    SB_DCHECK(frame_tracker_);
  }

  void Render(MediaTimeProvider* media_time_provider,
              std::list<scoped_refptr<VideoFrame>>* frames,
              VideoRendererSink::DrawFrameCB draw_frame_cb) override {
    // Clear output frames.
    while (!frames->empty() && !frames->front()->is_end_of_stream()) {
      frames->pop_front();
    }
  }
  void Seek(int64_t seek_to_time) override {
    frame_tracker_->Seek(seek_to_time);
  }
  int GetDroppedFrames() override {
    return frame_tracker_->UpdateAndGetDroppedFrames();
  }

 private:
  VideoFrameTracker* frame_tracker_;
};

class MediaCodecVideoDecoder::Sink : public VideoRendererSink {
 public:
  bool Render() {
    SB_DCHECK(render_cb_);

    rendered_ = false;
    render_cb_(std::bind(&Sink::DrawFrame, this, _1, _2));

    return rendered_;
  }

 private:
  void SetRenderCB(RenderCB render_cb) override {
    SB_DCHECK(!render_cb_);
    SB_DCHECK(render_cb);

    render_cb_ = render_cb;
  }

  void SetBounds(int z_index, int x, int y, int width, int height) override {}

  DrawFrameStatus DrawFrame(const scoped_refptr<VideoFrame>& frame,
                            int64_t release_time_in_nanoseconds) {
    rendered_ = true;
    static_cast<VideoFrameImpl*>(frame.get())
        ->Draw(release_time_in_nanoseconds);

    return kReleased;
  }

  RenderCB render_cb_;
  bool rendered_;
};

MediaCodecVideoDecoder::MediaCodecVideoDecoder(
    const VideoStreamInfo& video_stream_info,
    SbDrmSystem drm_system,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider,
    const std::string& max_video_capabilities,
    int tunnel_mode_audio_session_id,
    bool force_secure_pipeline_under_tunnel_mode,
    bool force_reset_surface,
    bool force_reset_surface_under_tunnel_mode,
    bool force_big_endian_hdr_metadata,
    int max_video_input_size,
    bool enable_flush_during_seek,
    int64_t reset_delay_usec,
    int64_t flush_delay_usec,
    std::string* error_message)
    : video_codec_(video_stream_info.codec),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      max_video_capabilities_(max_video_capabilities),
      require_software_codec_(IsSoftwareDecodeRequired(max_video_capabilities)),
      force_big_endian_hdr_metadata_(force_big_endian_hdr_metadata),
      tunnel_mode_audio_session_id_(tunnel_mode_audio_session_id),
      max_video_input_size_(max_video_input_size),
      enable_flush_during_seek_(enable_flush_during_seek),
      reset_delay_usec_(android_get_device_api_level() < 34 ? reset_delay_usec
                                                            : 0),
      flush_delay_usec_(android_get_device_api_level() < 34 ? flush_delay_usec
                                                            : 0),
      force_reset_surface_(force_reset_surface),
      force_reset_surface_under_tunnel_mode_(
          force_reset_surface_under_tunnel_mode),
      is_video_frame_tracker_enabled_(IsFrameRenderedCallbackEnabled() ||
                                      tunnel_mode_audio_session_id != -1),
      has_new_texture_available_(false),
      number_of_preroll_frames_(kInitialPrerollFrameCount),
      bridge_(output_mode_ == kSbPlayerOutputModeDecodeToTexture
                  ? std::make_unique<VideoSurfaceTextureBridge>(this)
                  : nullptr) {
  SB_CHECK(error_message);

  if (force_secure_pipeline_under_tunnel_mode) {
    SB_DCHECK_NE(tunnel_mode_audio_session_id_, -1);
    SB_DCHECK(!drm_system_);
    drm_system_to_enforce_tunnel_mode_ = std::make_unique<DrmSystem>(
        "com.youtube.widevine.l3", nullptr, StubDrmSessionUpdateRequestFunc,
        StubDrmSessionUpdatedFunc, StubDrmSessionKeyStatusesChangedFunc);
    drm_system_ = drm_system_to_enforce_tunnel_mode_.get();
  }

  if (is_video_frame_tracker_enabled_) {
    video_frame_tracker_ =
        std::make_unique<VideoFrameTracker>(kMaxPendingInputsSize * 2);
  }

  if (require_software_codec_) {
    SB_DCHECK_EQ(output_mode_, kSbPlayerOutputModeDecodeToTexture);
  }

  if (video_codec_ != kSbMediaVideoCodecAv1) {
    auto result = InitializeCodec(video_stream_info);
    if (!result) {
      *error_message =
          "Failed to initialize video decoder with error: " + result.error();
      SB_LOG(ERROR) << *error_message;
      TeardownCodec();
    }
  }

  SB_LOG(INFO) << "Created VideoDecoder for codec "
               << GetMediaVideoCodecName(video_codec_) << ", with output mode "
               << GetPlayerOutputModeName(output_mode_)
               << ", max video capabilities \"" << max_video_capabilities_
               << "\", and tunnel mode audio session id "
               << tunnel_mode_audio_session_id_;
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  TeardownCodec();
  if (tunnel_mode_audio_session_id_ != -1) {
    ClearVideoWindow(force_reset_surface_under_tunnel_mode_);
  } else {
    ClearVideoWindow(force_reset_surface_);
  }
}

scoped_refptr<VideoRendererSink> MediaCodecVideoDecoder::GetSink() {
  if (sink_ == nullptr) {
    sink_ = make_scoped_refptr<Sink>();
  }
  return sink_;
}

std::unique_ptr<VideoRenderAlgorithm>
MediaCodecVideoDecoder::GetRenderAlgorithm() {
  if (tunnel_mode_audio_session_id_ == -1) {
    return std::make_unique<VideoRenderAlgorithmAndroid>(
        this, video_frame_tracker_.get());
  }
  return std::make_unique<VideoRenderAlgorithmTunneled>(
      video_frame_tracker_.get());
}

void MediaCodecVideoDecoder::Initialize(
    const DecoderStatusCB& decoder_status_cb,
    const ErrorCB& error_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;

  // There's a race condition when suspending the app. If surface view is
  // destroyed before this function is called, |media_decoder_| could be null
  // here.
  if (!media_decoder_) {
    SB_LOG(INFO) << "Trying to call Initialize() when media_decoder_ is null.";
    return;
  }
  media_decoder_->Initialize(
      std::bind(&MediaCodecVideoDecoder::ReportError, this, _1, _2));
}

size_t MediaCodecVideoDecoder::GetPrerollFrameCount() const {
  // Tunnel mode uses its own preroll logic.
  if (tunnel_mode_audio_session_id_ != -1) {
    return 0;
  }
  if (input_buffer_written_ > 0 && first_buffer_timestamp_ != 0) {
    return kNonInitialPrerollFrameCount;
  }
  return number_of_preroll_frames_;
}

int64_t MediaCodecVideoDecoder::GetPrerollTimeout() const {
  if (input_buffer_written_ > 0 && first_buffer_timestamp_ != 0) {
    return std::numeric_limits<int64_t>::max();
  }
  return kInitialPrerollTimeout;
}

void MediaCodecVideoDecoder::WriteInputBuffers(
    const InputBuffers& input_buffers) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(!input_buffers.empty());
  SB_DCHECK_EQ(input_buffers.front()->sample_type(), kSbMediaTypeVideo);
  SB_DCHECK(decoder_status_cb_);

  if (input_buffer_written_ == 0) {
    SB_DCHECK_EQ(video_fps_, 0);
    first_buffer_timestamp_ = input_buffers.front()->timestamp();

    // If color metadata is present and is not an identity mapping, then
    // teardown the codec so it can be reinitalized with the new metadata.
    const auto& color_metadata =
        input_buffers.front()->video_stream_info().color_metadata;
    if (!IsIdentity(color_metadata)) {
      SB_DCHECK(!color_metadata_) << "Unexpected residual color metadata.";
      SB_LOG(INFO) << "Reinitializing codec with HDR color metadata.";
      TeardownCodec();
      color_metadata_ = color_metadata;
    }

    // Re-initialize the codec now if it was torn down either in |Reset| or
    // because we need to change the color metadata.
    if (video_codec_ != kSbMediaVideoCodecAv1 && media_decoder_ == NULL) {
      auto result = InitializeCodec(input_buffers.front()->video_stream_info());
      if (!result) {
        std::string error_message =
            "Failed to reinitialize codec with error: " + result.error();
        SB_LOG(ERROR) << error_message;
        TeardownCodec();
        ReportError(kSbPlayerErrorDecode, error_message);
        return;
      }
    }

    if (tunnel_mode_audio_session_id_ != -1) {
      Schedule(
          std::bind(&MediaCodecVideoDecoder::OnTunnelModePrerollTimeout, this),
          kInitialPrerollTimeout);
    }
  }

  input_buffer_written_ += input_buffers.size();

  if (video_codec_ == kSbMediaVideoCodecAv1 && video_fps_ == 0) {
    SB_DCHECK(!media_decoder_);

    pending_input_buffers_.insert(pending_input_buffers_.end(),
                                  input_buffers.begin(), input_buffers.end());
    if (pending_input_buffers_.size() <
        kFpsGuesstimateRequiredInputBufferCount) {
      decoder_status_cb_(kNeedMoreInput, NULL);
      return;
    }
    auto result =
        InitializeCodec(pending_input_buffers_.front()->video_stream_info());
    if (!result) {
      std::string error_message =
          "Failed to reinitialize codec with error: " + result.error();
      SB_LOG(ERROR) << error_message;
      TeardownCodec();
      ReportError(kSbPlayerErrorDecode, error_message);
      return;
    }
    return;
  }

  WriteInputBuffersInternal(input_buffers);
}

void MediaCodecVideoDecoder::WriteEndOfStream() {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb_);

  if (end_of_stream_written_) {
    SB_LOG(WARNING) << "WriteEndOfStream() is called more than once.";
    return;
  }
  end_of_stream_written_ = true;

  if (input_buffer_written_ == 0) {
    // In this case, |media_decoder_|'s decoder thread is not initialized,
    // return EOS frame directly.
    first_buffer_timestamp_ = 0;
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
    return;
  }

  if (video_codec_ == kSbMediaVideoCodecAv1 && video_fps_ == 0) {
    SB_DCHECK(!media_decoder_);
    SB_DCHECK_EQ(pending_input_buffers_.size(),
                 static_cast<size_t>(input_buffer_written_));

    auto result =
        InitializeCodec(pending_input_buffers_.front()->video_stream_info());
    if (!result) {
      std::string error_message =
          "Failed to reinitialize codec with error: " + result.error();
      SB_LOG(ERROR) << error_message;
      TeardownCodec();
      ReportError(kSbPlayerErrorDecode, error_message);
      return;
    }
  }

  // There's a race condition when suspending the app. If surface view is
  // destroyed before video decoder stopped, |media_decoder_| could be null
  // here. And error_cb_() could be handled asynchronously. It's possible
  // that WriteEndOfStream() is called immediately after the first
  // WriteInputBuffer() fails, in such case |media_decoder_| will be null.
  if (!media_decoder_) {
    SB_LOG(INFO)
        << "Trying to write end of stream when media_decoder_ is null.";
    return;
  }

  media_decoder_->WriteEndOfStream();
}

void MediaCodecVideoDecoder::Reset() {
  SB_CHECK(BelongsToCurrentThread());

  // If fail to flush |media_decoder_| or |media_decoder_| is null, then
  // re-create |media_decoder_|. If the codec is kSbMediaVideoCodecAv1,
  // set video_fps_ to 0 will call InitializeCodec(),
  // which we do not need if flush the codec.
  if (!enable_flush_during_seek_ || !media_decoder_ ||
      !media_decoder_->Flush()) {
    TeardownCodec();
    if (reset_delay_usec_ > 0) {
      usleep(reset_delay_usec_);
    }

    input_buffer_written_ = 0;
    video_fps_ = 0;
  }
  CancelPendingJobs();

  // TODO(b/291959069): After flush |media_decoder_|, the output buffers
  // may have invalid frames. Reset |output_format_| to null here to skip max
  // output buffers check.
  decoded_output_frames_ = 0;
  output_format_ = std::nullopt;

  tunnel_mode_prerolling_.store(true);
  tunnel_mode_frame_rendered_.store(false);
  end_of_stream_written_ = false;
  pending_input_buffers_.clear();

  // TODO: We rely on VideoRenderAlgorithmTunneled::Seek() to be called inside
  //       VideoRenderer::Seek() after calling MediaCodecVideoDecoder::Reset()
  //       to update the seek status of |video_frame_tracker_|.  This is
  //       slightly flaky as it depends on the behavior of the video renderer.
}

Result<void> MediaCodecVideoDecoder::InitializeCodec(
    const VideoStreamInfo& video_stream_info) {
  SB_CHECK(BelongsToCurrentThread());

  if (video_stream_info.codec == kSbMediaVideoCodecAv1) {
    SB_DCHECK_GT(pending_input_buffers_.size(), 0u);

    // Guesstimate the video fps.
    if (pending_input_buffers_.size() == 1) {
      video_fps_ = 30;
    } else {
      int64_t first_timestamp = pending_input_buffers_[0]->timestamp();
      int64_t second_timestamp = pending_input_buffers_[1]->timestamp();
      if (pending_input_buffers_.size() > 2) {
        second_timestamp =
            std::min(second_timestamp, pending_input_buffers_[2]->timestamp());
      }
      int64_t frame_duration = second_timestamp - first_timestamp;
      if (frame_duration > 0) {
        // To avoid problems caused by deviation of fps calculation, we use the
        // nearest multiple of 5 to check codec capability. So, the fps like 61,
        // 62 will be capped to 60, and 24 will be increased to 25.
        const double kFpsMinDifference = 5;
        video_fps_ =
            std::round(1'000'000LL / (second_timestamp - first_timestamp) /
                       kFpsMinDifference) *
            kFpsMinDifference;
      } else {
        video_fps_ = 30;
      }
    }
    SB_DCHECK_GT(video_fps_, 0);
  }

  // Setup the output surface object.  If we are in punch-out mode, target
  // the passed in Android video surface.  If we are in decode-to-texture
  // mode, create a surface from a new texture target and use that as the
  // output surface.
  jobject j_output_surface = NULL;
  switch (output_mode_) {
    case kSbPlayerOutputModePunchOut: {
      j_output_surface = AcquireVideoSurface();
      if (j_output_surface) {
        owns_video_surface_ = true;
      }
    } break;
    case kSbPlayerOutputModeDecodeToTexture: {
      // A width and height of (0, 0) is provided here because Android doesn't
      // actually allocate any memory into the texture at this time.  That is
      // done behind the scenes, the acquired texture is not actually backed
      // by texture data until updateTexImage() is called on it.
      if (!decode_target_graphics_context_provider_) {
        return Failure("Invalid decode target graphics context provider.");
      }
      DecodeTarget* decode_target =
          new DecodeTarget(decode_target_graphics_context_provider_);
      if (!SbDecodeTargetIsValid(decode_target)) {
        return Failure("Could not acquire a decode target from provider.");
      }
      j_output_surface = decode_target->surface();

      JNIEnv* env = AttachCurrentThread();
      ScopedJavaLocalRef<jobject> surface_texture(
          env, decode_target->surface_texture());
      bridge_->SetOnFrameAvailableListener(
          env, JavaParamRef<jobject>(env, surface_texture.obj()));

      std::lock_guard lock(decode_target_mutex_);
      decode_target_ = decode_target;
    } break;
    case kSbPlayerOutputModeInvalid: {
      SB_NOTREACHED();
    } break;
  }
  if (!j_output_surface) {
    return Failure("Video surface does not exist.");
  }

  if (video_stream_info.codec == kSbMediaVideoCodecAv1) {
    SB_DCHECK_GT(video_fps_, 0);
  } else {
    SB_DCHECK_EQ(video_fps_, 0);
  }

  // TODO(b/281431214): Evaluate if we should also parse the fps from
  //                    `max_video_capabilities_` and pass to MediaCodecDecoder
  //                    ctor.
  std::optional<Size> max_frame_size =
      ParseMaxResolution(max_video_capabilities_, video_stream_info.frame_size);

  std::string error_message;
  media_decoder_ = std::make_unique<MediaCodecDecoder>(
      /*host=*/this, video_stream_info.codec, video_stream_info.frame_size,
      max_frame_size, video_fps_, j_output_surface, drm_system_,
      color_metadata_ ? &*color_metadata_ : nullptr, require_software_codec_,
      std::bind(&MediaCodecVideoDecoder::OnFrameRendered, this, _1),
      std::bind(&MediaCodecVideoDecoder::OnFirstTunnelFrameReady, this),
      tunnel_mode_audio_session_id_, force_big_endian_hdr_metadata_,
      max_video_input_size_, flush_delay_usec_, &error_message);
  if (media_decoder_->is_valid()) {
    if (error_cb_) {
      media_decoder_->Initialize(
          std::bind(&MediaCodecVideoDecoder::ReportError, this, _1, _2));
    }
    media_decoder_->SetPlaybackRate(playback_rate_);

    if (video_stream_info.codec == kSbMediaVideoCodecAv1) {
      SB_DCHECK(!pending_input_buffers_.empty());
    } else {
      SB_DCHECK(pending_input_buffers_.empty());
    }
    if (!pending_input_buffers_.empty()) {
      WriteInputBuffersInternal(pending_input_buffers_);
      pending_input_buffers_.clear();
    }
    return Success();
  }
  media_decoder_.reset();
  return Failure("Media Decoder is not valid: " + error_message);
}

void MediaCodecVideoDecoder::TeardownCodec() {
  SB_CHECK(BelongsToCurrentThread());
  if (owns_video_surface_) {
    ReleaseVideoSurface();
    owns_video_surface_ = false;
  }
  media_decoder_.reset();
  color_metadata_ = std::nullopt;

  SbDecodeTarget decode_target_to_release = kSbDecodeTargetInvalid;
  {
    std::lock_guard lock(decode_target_mutex_);
    if (decode_target_ != nullptr) {
      // Remove OnFrameAvailableListener to make sure the callback
      // would not be called.
      JNIEnv* env = AttachCurrentThread();
      ScopedJavaLocalRef<jobject> surface_texture(
          env, decode_target_->surface_texture());
      bridge_->RemoveOnFrameAvailableListener(
          env, JavaParamRef<jobject>(env, surface_texture.obj()));

      decode_target_to_release = decode_target_;
      decode_target_ = nullptr;
      first_texture_received_ = false;
      has_new_texture_available_.store(false);
    } else {
      // If |decode_target_| is not created, |first_texture_received_| and
      // |has_new_texture_available_| should always be false.
      SB_DCHECK(!first_texture_received_);
      SB_DCHECK(!has_new_texture_available_.load());
    }
  }
  // Release SbDecodeTarget on renderer thread. As |decode_target_mutex_| may
  // be required in renderer thread, SbDecodeTargetReleaseInGlesContext() must
  // be called when |decode_target_mutex_| is not locked, or we may get
  // deadlock.
  if (SbDecodeTargetIsValid(decode_target_to_release)) {
    SbDecodeTargetReleaseInGlesContext(decode_target_graphics_context_provider_,
                                       decode_target_to_release);
  }
}

void MediaCodecVideoDecoder::OnEndOfStreamWritten(
    MediaCodecBridge* media_codec_bridge) {
  if (tunnel_mode_audio_session_id_ == -1) {
    return;
  }

  SB_DCHECK(decoder_status_cb_);

  tunnel_mode_prerolling_.store(false);

  // TODO: Refactor the VideoDecoder and the VideoRendererImpl to improve the
  //       handling of preroll and EOS for pure punchout decoders.
  decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
  sink_->Render();
}

void MediaCodecVideoDecoder::WriteInputBuffersInternal(
    const InputBuffers& input_buffers) {
  SB_DCHECK(!input_buffers.empty());

  // There's a race condition when suspending the app. If surface view is
  // destroyed before video decoder stopped, |media_decoder_| could be null
  // here. And error_cb_() could be handled asynchronously. It's possible
  // that WriteInputBuffer() is called again when the first WriteInputBuffer()
  // fails, in such case |media_decoder_| will be null.
  if (!media_decoder_) {
    SB_LOG(INFO) << "Trying to write input buffer when media_decoder_ is null.";
    return;
  }

  if (is_video_frame_tracker_enabled_) {
    SB_DCHECK(video_frame_tracker_);
    for (const auto& input_buffer : input_buffers) {
      video_frame_tracker_->OnInputBuffer(input_buffer->timestamp());
    }
  }

  media_decoder_->WriteInputBuffers(input_buffers);
  if (media_decoder_->GetNumberOfPendingInputs() < kMaxPendingInputsSize) {
    decoder_status_cb_(kNeedMoreInput, NULL);
  } else if (tunnel_mode_audio_session_id_ != -1) {
    // In tunnel mode playback when need data is not signaled above, it is
    // possible that the VideoDecoder won't get a chance to send kNeedMoreInput
    // to the renderer again.  Schedule a task to check back.
    Schedule(
        std::bind(&MediaCodecVideoDecoder::OnTunnelModeCheckForNeedMoreInput,
                  this),
        kNeedMoreInputCheckIntervalInTunnelMode);
  }

  if (tunnel_mode_audio_session_id_ != -1) {
    int64_t max_timestamp = input_buffers[0]->timestamp();
    for (const auto& input_buffer : input_buffers) {
      max_timestamp = std::max(max_timestamp, input_buffer->timestamp());
    }

    if (tunnel_mode_prerolling_.load()) {
      // TODO: Refine preroll logic in tunnel mode.
      bool enough_buffers_written_to_media_codec = false;
      if (first_buffer_timestamp_ == 0) {
        // Initial playback.
        enough_buffers_written_to_media_codec =
            (input_buffer_written_ -
             media_decoder_->GetNumberOfPendingInputs()) >
            kInitialPrerollFrameCount;
      } else {
        // Seeking.  Note that this branch can be eliminated once seeking in
        // tunnel mode is always aligned to the next video key frame.
        enough_buffers_written_to_media_codec =
            (input_buffer_written_ -
             media_decoder_->GetNumberOfPendingInputs()) >
                kSeekingPrerollPendingWorkSizeInTunnelMode &&
            max_timestamp >= video_frame_tracker_->seek_to_time();
      }

      bool cache_full =
          media_decoder_->GetNumberOfPendingInputs() >= kMaxPendingInputsSize;
      bool prerolled = tunnel_mode_frame_rendered_.load() > 0 ||
                       enough_buffers_written_to_media_codec || cache_full;

      if (prerolled) {
        TryToSignalPrerollForTunnelMode();
      }
    }
  }
}

void MediaCodecVideoDecoder::ProcessOutputBuffer(
    MediaCodecBridge* media_codec_bridge,
    const DequeueOutputResult& dequeue_output_result) {
  SB_DCHECK(decoder_status_cb_);
  SB_DCHECK_GE(dequeue_output_result.index, 0);

  bool is_end_of_stream =
      dequeue_output_result.flags & BUFFER_FLAG_END_OF_STREAM;
  if (!is_end_of_stream) {
    ++decoded_output_frames_;
    if (output_format_) {
      ++buffered_output_frames_;
      // We have to wait until we feed enough inputs to the decoder and receive
      // enough outputs before update |max_buffered_output_frames_|. Otherwise,
      // |max_buffered_output_frames_| may be updated to a very small number
      // when we receive the first few outputs.
      if (decoded_output_frames_ > kInitialPrerollFrameCount &&
          buffered_output_frames_ > max_buffered_output_frames_) {
        max_buffered_output_frames_ = buffered_output_frames_;
        MaxMediaCodecOutputBuffersLookupTable::GetInstance()
            ->UpdateMaxOutputBuffers(output_format_.value(),
                                     max_buffered_output_frames_);
      }
    }
  }
  decoder_status_cb_(
      is_end_of_stream ? kBufferFull : kNeedMoreInput,
      new VideoFrameImpl(
          dequeue_output_result, media_codec_bridge,
          std::bind(&MediaCodecVideoDecoder::OnVideoFrameRelease, this)));
}

void MediaCodecVideoDecoder::RefreshOutputFormat(
    MediaCodecBridge* media_codec_bridge) {
  SB_DCHECK(media_codec_bridge);
  SB_DLOG(INFO) << "Output format changed, trying to dequeue again.";

  std::lock_guard lock(decode_target_mutex_);
  std::optional<FrameSize> output_size = media_codec_bridge->GetOutputSize();
  if (!output_size) {
    SB_LOG(WARNING)
        << "GetOutputSize() returned null. Defaulting to an empty FrameSize.";
    // We default to an empty FrameSize instead of propagating a failure to
    // ensure system robustness. The calling code historically does not expect
    // this call to fail and is not equipped to handle a null optional.
    output_size = FrameSize();
  }

  // Record the latest dimensions of the decoded input.
  frame_sizes_.push_back(*output_size);

  if (tunnel_mode_audio_session_id_ != -1) {
    return;
  }
  if (first_output_format_changed_) {
    // After resolution changes, the output buffers may have frames of different
    // resolutions. In that case, it's hard to determine the max supported
    // output buffers. So, we reset |output_format_| to null here to skip max
    // output buffers check.
    output_format_ = std::nullopt;
    return;
  }
  output_format_ =
      VideoOutputFormat(video_codec_, frame_sizes_.back().display_size,
                        color_metadata_.has_value());
  first_output_format_changed_ = true;
  auto max_output_buffers =
      MaxMediaCodecOutputBuffersLookupTable::GetInstance()
          ->GetMaxOutputVideoBuffers(output_format_.value());
  if (max_output_buffers > 0 &&
      max_output_buffers < kInitialPrerollFrameCount) {
    number_of_preroll_frames_ = max_output_buffers;
  }
}

bool MediaCodecVideoDecoder::Tick(MediaCodecBridge* media_codec_bridge) {
  // Tunnel mode renders frames in MediaCodec automatically and shouldn't reach
  // here.
  SB_DCHECK_EQ(tunnel_mode_audio_session_id_, -1);
  return sink_->Render();
}

void MediaCodecVideoDecoder::OnFlushing() {
  decoder_status_cb_(kReleaseAllFrames, NULL);
}

bool MediaCodecVideoDecoder::IsBufferDecodeOnly(
    const scoped_refptr<InputBuffer>& input_buffer) {
  if (!is_video_frame_tracker_enabled_) {
    return false;
  }

  SB_CHECK(video_frame_tracker_);
  return input_buffer->timestamp() < video_frame_tracker_->seek_to_time();
}

namespace {

void updateTexImage(jobject surface_texture) {
  JNIEnv* env = AttachCurrentThread();

  VideoSurfaceTextureBridge::UpdateTexImage(
      env, JavaParamRef<jobject>(env, surface_texture));
}

void getTransformMatrix(jobject surface_texture, float* matrix4x4) {
  JNIEnv* env = AttachCurrentThread();

  jfloatArray java_array = env->NewFloatArray(16);
  SB_CHECK(java_array);

  VideoSurfaceTextureBridge::GetTransformMatrix(
      env, JavaParamRef<jobject>(env, surface_texture),
      JavaParamRef<jfloatArray>(env, java_array));

  jfloat* array_values = env->GetFloatArrayElements(java_array, 0);
  memcpy(matrix4x4, array_values, sizeof(float) * 16);

  env->DeleteLocalRef(java_array);
}

// Converts a 4x4 matrix representing the texture coordinate transform into
// an equivalent rectangle representing the region within the texture where
// the pixel data is valid.  Note that the width and height of this region may
// be negative to indicate that that axis should be flipped.
SbDecodeTargetInfoContentRegion GetDecodeTargetContentRegionFromMatrix(
    const Size& size,
    const float* matrix4x4) {
  // Ensure that this matrix contains no rotations or shears.  In other words,
  // make sure that we can convert it to a decode target content region without
  // losing any information.
  SB_DCHECK_EQ(matrix4x4[1], 0.0f);
  SB_DCHECK_EQ(matrix4x4[2], 0.0f);
  SB_DCHECK_EQ(matrix4x4[3], 0.0f);

  SB_DCHECK_EQ(matrix4x4[4], 0.0f);
  SB_DCHECK_EQ(matrix4x4[6], 0.0f);
  SB_DCHECK_EQ(matrix4x4[7], 0.0f);

  SB_DCHECK_EQ(matrix4x4[8], 0.0f);
  SB_DCHECK_EQ(matrix4x4[9], 0.0f);
  SB_DCHECK_EQ(matrix4x4[10], 1.0f);
  SB_DCHECK_EQ(matrix4x4[11], 0.0f);

  SB_DCHECK_EQ(matrix4x4[14], 0.0f);
  SB_DCHECK_EQ(matrix4x4[15], 1.0f);

  float origin_x = matrix4x4[12];
  float origin_y = matrix4x4[13];

  float extent_x = matrix4x4[0] + matrix4x4[12];
  float extent_y = matrix4x4[5] + matrix4x4[13];

  SB_DCHECK_GE(origin_y, 0.0f);
  SB_DCHECK_LE(origin_y, 1.0f);
  SB_DCHECK_GE(origin_x, 0.0f);
  SB_DCHECK_LE(origin_x, 1.0f);
  SB_DCHECK_GE(extent_x, 0.0f);
  SB_DCHECK_LE(extent_x, 1.0f);
  SB_DCHECK_GE(extent_y, 0.0f);
  SB_DCHECK_LE(extent_y, 1.0f);

  // Flip the y-axis to match ContentRegion's coordinate system.
  origin_y = 1.0f - origin_y;
  extent_y = 1.0f - extent_y;

  SbDecodeTargetInfoContentRegion content_region;

  content_region.left = origin_x * size.width;
  content_region.right = extent_x * size.width;

  // Note that in GL coordinates, the origin is the bottom and the extent
  // is the top.
  content_region.top = extent_y * size.height;
  content_region.bottom = origin_y * size.height;

  return content_region;
}

}  // namespace

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget MediaCodecVideoDecoder::GetCurrentDecodeTarget() {
  SB_DCHECK_EQ(output_mode_, kSbPlayerOutputModeDecodeToTexture);
  // We must take a lock here since this function can be called from a separate
  // thread.
  std::lock_guard lock(decode_target_mutex_);
  if (decode_target_ != nullptr) {
    bool has_new_texture = has_new_texture_available_.exchange(false);
    if (has_new_texture) {
      updateTexImage(decode_target_->surface_texture());
      UpdateDecodeTargetSizeAndContentRegion_Locked();

      if (!first_texture_received_) {
        first_texture_received_ = true;
      }
    }

    if (first_texture_received_) {
      decode_target_->AddRef();
      return decode_target_;
    }
  }
  return kSbDecodeTargetInvalid;
}

void MediaCodecVideoDecoder::UpdateDecodeTargetSizeAndContentRegion_Locked() {
  SB_DCHECK(!frame_sizes_.empty());

  while (!frame_sizes_.empty()) {
    const auto& frame_size = frame_sizes_.front();
    if (frame_size.has_crop_values) {
      decode_target_->set_dimension(frame_size.display_size);

      float matrix4x4[16];
      getTransformMatrix(decode_target_->surface_texture(), matrix4x4);

      auto content_region = GetDecodeTargetContentRegionFromMatrix(
          frame_size.display_size, matrix4x4);
      decode_target_->set_content_region(content_region);

      // Now we have two crop rectangles, one from the MediaFormat, one from the
      // transform of the surface texture.  Their sizes should match.
      // Note that we cannot compare individual corners directly, as the values
      // retrieving from the surface texture can be flipped.
      int content_region_width =
          std::abs(content_region.left - content_region.right) + 1;
      int content_region_height =
          std::abs(content_region.bottom - content_region.top) + 1;
      // Using 2 as epsilon, as the texture may get clipped by one pixel from
      // each side.
      const auto display_size = frame_size.display_size;
      bool are_crop_values_matching =
          std::abs(content_region_width - display_size.width) <= 2 &&
          std::abs(content_region_height - display_size.height) <= 2;
      if (are_crop_values_matching) {
        return;
      }

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
      // If we failed to find any matching clip regions, the crop values
      // returned from the platform may be inconsistent.
      // Crash in non-gold mode, and fallback to the old logic in gold mode to
      // avoid terminating the app in production.
      SB_LOG_IF(WARNING, frame_sizes_.size() <= 1)
          << frame_size << " - (" << content_region.left << ", "
          << content_region.top << ", " << content_region.right << ", "
          << content_region.bottom << ")";
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    } else {
      SB_LOG(WARNING) << "Crop values not set.";
    }

    if (frame_sizes_.size() == 1) {
      SB_LOG(WARNING) << "Setting content region frame width/height failed,"
                      << " fallback to the legacy logic.";
      break;
    }

    frame_sizes_.erase(frame_sizes_.begin());
  }

  SB_DCHECK(!frame_sizes_.empty());
  if (frame_sizes_.empty()) {
    // This should never happen.  Appending a default value so it aligns to the
    // legacy behavior, where a single value (instead of an std::vector<>) is
    // used.
    frame_sizes_.resize(1);
  }

  // The legacy logic works when the crop rectangle has the same aspect ratio as
  // the video texture, which is true for most of the playbacks.
  // Leaving the legacy logic in place in case the new logic above doesn't work
  // on some devices, so at least the majority of playbacks still work.
  decode_target_->set_dimension(frame_sizes_.back().display_size);

  float matrix4x4[16];
  getTransformMatrix(decode_target_->surface_texture(), matrix4x4);

  decode_target_->set_content_region(GetDecodeTargetContentRegionFromMatrix(
      frame_sizes_.back().display_size, matrix4x4));
}

void MediaCodecVideoDecoder::SetPlaybackRate(double playback_rate) {
  playback_rate_ = playback_rate;
  if (media_decoder_) {
    media_decoder_->SetPlaybackRate(playback_rate);
  }
}

void MediaCodecVideoDecoder::OnFrameAvailable() {
  has_new_texture_available_.store(true);
}

void MediaCodecVideoDecoder::TryToSignalPrerollForTunnelMode() {
  if (tunnel_mode_prerolling_.exchange(false)) {
    SB_LOG(ERROR) << "Tunnel mode preroll finished.";
    // TODO: Currently the decoder sends a dummy frame to the renderer to signal
    //       preroll finish.  We should investigate a better way for prerolling
    //       when the video is rendered directly by the decoder, maybe by always
    //       sending placeholder frames.
    decoder_status_cb_(kNeedMoreInput,
                       new VideoFrame(video_frame_tracker_->seek_to_time()));
  }
}

bool MediaCodecVideoDecoder::IsFrameRenderedCallbackEnabled() {
  return MediaCodecBridge::IsFrameRenderedCallbackEnabled() == JNI_TRUE;
}

void MediaCodecVideoDecoder::OnFrameRendered(int64_t frame_timestamp) {
  SB_DCHECK(is_video_frame_tracker_enabled_);
  SB_DCHECK(video_frame_tracker_);

  if (tunnel_mode_audio_session_id_ != -1) {
    tunnel_mode_frame_rendered_.store(true);
  }
  video_frame_tracker_->OnFrameRendered(frame_timestamp);
}

void MediaCodecVideoDecoder::OnFirstTunnelFrameReady() {
  SB_DCHECK_NE(tunnel_mode_audio_session_id_, -1);

  TryToSignalPrerollForTunnelMode();
}

void MediaCodecVideoDecoder::OnTunnelModePrerollTimeout() {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK_NE(tunnel_mode_audio_session_id_, -1);

  TryToSignalPrerollForTunnelMode();
}

void MediaCodecVideoDecoder::OnTunnelModeCheckForNeedMoreInput() {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK_NE(tunnel_mode_audio_session_id_, -1);

  // There's a race condition when suspending the app. If surface view is
  // destroyed before this function is called, |media_decoder_| could be null
  // here, in such case |media_decoder_| will be null.
  if (!media_decoder_) {
    SB_LOG(INFO) << "Trying to call OnTunnelModeCheckForNeedMoreInput() when"
                 << " media_decoder_ is null.";
    return;
  }

  if (media_decoder_->GetNumberOfPendingInputs() < kMaxPendingInputsSize) {
    decoder_status_cb_(kNeedMoreInput, NULL);
    return;
  }

  Schedule(std::bind(&MediaCodecVideoDecoder::OnTunnelModeCheckForNeedMoreInput,
                     this),
           kNeedMoreInputCheckIntervalInTunnelMode);
}

void MediaCodecVideoDecoder::OnVideoFrameRelease() {
  if (output_format_) {
    --buffered_output_frames_;
    SB_DCHECK_GE(buffered_output_frames_, 0);
  }
}

void MediaCodecVideoDecoder::OnSurfaceDestroyed() {
  if (!BelongsToCurrentThread()) {
    // Wait until codec is stopped.
    std::unique_lock lock(surface_destroy_mutex_);
    surface_destroyed_ = false;
    Schedule(std::bind(&MediaCodecVideoDecoder::OnSurfaceDestroyed, this));
    surface_condition_variable_.wait_for(lock,
                                         std::chrono::microseconds(1'000'000),
                                         [this] { return surface_destroyed_; });
    return;
  }
  // When this function is called, the decoder no longer owns the surface.
  owns_video_surface_ = false;
  TeardownCodec();
  {
    std::lock_guard lock(surface_destroy_mutex_);
    surface_destroyed_ = true;
  }
  surface_condition_variable_.notify_one();
}

void MediaCodecVideoDecoder::ReportError(SbPlayerError error,
                                         const std::string& error_message) {
  SB_DCHECK(error_cb_);

  if (!error_cb_) {
    return;
  }

  error_cb_(kSbPlayerErrorDecode, error_message);
}

}  // namespace starboard
