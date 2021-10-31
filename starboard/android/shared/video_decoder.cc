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

#include <jni.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <list>

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/decode_target_create.h"
#include "starboard/android/shared/decode_target_internal.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/video_render_algorithm.h"
#include "starboard/android/shared/window_internal.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/drm.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/string.h"
#include "starboard/thread.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

using ::starboard::shared::starboard::player::filter::VideoFrame;
using VideoRenderAlgorithmBase =
    ::starboard::shared::starboard::player::filter::VideoRenderAlgorithm;
using std::placeholders::_1;
using std::placeholders::_2;

class VideoFrameImpl : public VideoFrame {
 public:
  VideoFrameImpl(const DequeueOutputResult& dequeue_output_result,
                 MediaCodecBridge* media_codec_bridge)
      : VideoFrame(dequeue_output_result.flags & BUFFER_FLAG_END_OF_STREAM
                       ? kMediaTimeEndOfStream
                       : dequeue_output_result.presentation_time_microseconds),
        dequeue_output_result_(dequeue_output_result),
        media_codec_bridge_(media_codec_bridge),
        released_(false) {
    SB_DCHECK(media_codec_bridge_);
  }

  ~VideoFrameImpl() {
    if (!released_) {
      media_codec_bridge_->ReleaseOutputBuffer(dequeue_output_result_.index,
                                               false);
    }
  }

  void Draw(int64_t release_time_in_nanoseconds) {
    SB_DCHECK(!released_);
    SB_DCHECK(!is_end_of_stream());
    released_ = true;
    media_codec_bridge_->ReleaseOutputBufferAtTimestamp(
        dequeue_output_result_.index, release_time_in_nanoseconds);
  }

 private:
  DequeueOutputResult dequeue_output_result_;
  MediaCodecBridge* media_codec_bridge_;
  volatile bool released_;
};

const SbTime kInitialPrerollTimeout = 250 * kSbTimeMillisecond;
const SbTime kNeedMoreInputCheckIntervalInTunnelMode = 50 * kSbTimeMillisecond;

const int kInitialPrerollFrameCount = 8;
const int kNonInitialPrerollFrameCount = 1;

const int kSeekingPrerollPendingWorkSizeInTunnelMode =
    16 + kInitialPrerollFrameCount;
const int kMaxPendingWorkSize = 128;

const int kFpsGuesstimateRequiredInputBufferCount = 3;

// Convenience HDR mastering metadata.
const SbMediaMasteringMetadata kEmptyMasteringMetadata = {};

// Determine if two |SbMediaMasteringMetadata|s are equal.
bool Equal(const SbMediaMasteringMetadata& lhs,
           const SbMediaMasteringMetadata& rhs) {
  return memcmp(&lhs, &rhs, sizeof(SbMediaMasteringMetadata)) == 0;
}

// Determine if two |SbMediaColorMetadata|s are equal.
bool Equal(const SbMediaColorMetadata& lhs, const SbMediaColorMetadata& rhs) {
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
class VideoRenderAlgorithmTunneled : public VideoRenderAlgorithmBase {
 public:
  explicit VideoRenderAlgorithmTunneled(VideoFrameTracker* frame_tracker)
      : frame_tracker_(frame_tracker) {
    SB_DCHECK(frame_tracker_);
  }

  void Render(MediaTimeProvider* media_time_provider,
              std::list<scoped_refptr<VideoFrame>>* frames,
              VideoRendererSink::DrawFrameCB draw_frame_cb) override {}
  void Seek(SbTime seek_to_time) override {
    frame_tracker_->Seek(seek_to_time);
  }
  int GetDroppedFrames() override {
    return frame_tracker_->UpdateAndGetDroppedFrames();
  }

 private:
  VideoFrameTracker* frame_tracker_;
};

int VideoDecoder::number_of_hardware_decoders_ = 0;

class VideoDecoder::Sink : public VideoDecoder::VideoRendererSink {
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

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec,
                           SbDrmSystem drm_system,
                           SbPlayerOutputMode output_mode,
                           SbDecodeTargetGraphicsContextProvider*
                               decode_target_graphics_context_provider,
                           const char* max_video_capabilities,
                           int tunnel_mode_audio_session_id,
                           bool force_secure_pipeline_under_tunnel_mode,
                           bool force_big_endian_hdr_metadata,
                           std::string* error_message)
    : video_codec_(video_codec),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      tunnel_mode_audio_session_id_(tunnel_mode_audio_session_id),
      has_new_texture_available_(false),
      surface_condition_variable_(surface_destroy_mutex_),
      require_software_codec_(max_video_capabilities &&
                              strlen(max_video_capabilities) > 0),
      force_big_endian_hdr_metadata_(force_big_endian_hdr_metadata) {
  SB_DCHECK(error_message);

  if (tunnel_mode_audio_session_id != -1) {
    video_frame_tracker_.reset(new VideoFrameTracker(kMaxPendingWorkSize * 2));
  }
  if (force_secure_pipeline_under_tunnel_mode) {
    SB_DCHECK(tunnel_mode_audio_session_id != -1);
    SB_DCHECK(!drm_system_);
    drm_system_to_enforce_tunnel_mode_.reset(new DrmSystem(
        nullptr, StubDrmSessionUpdateRequestFunc, StubDrmSessionUpdatedFunc,
        StubDrmSessionKeyStatusesChangedFunc));
    drm_system_ = drm_system_to_enforce_tunnel_mode_.get();
  }

  if (require_software_codec_) {
    SB_DCHECK(output_mode_ == kSbPlayerOutputModeDecodeToTexture);
  }
  if (!require_software_codec_) {
    number_of_hardware_decoders_++;
  }

  if (video_codec_ != kSbMediaVideoCodecAv1) {
    if (!InitializeCodec(error_message)) {
      *error_message =
          "Failed to initialize video decoder with error: " + *error_message;
      SB_LOG(ERROR) << *error_message;
      TeardownCodec();
    }
  }
}

VideoDecoder::~VideoDecoder() {
  TeardownCodec();
  ClearVideoWindow();

  if (!require_software_codec_) {
    number_of_hardware_decoders_--;
  }
}

scoped_refptr<VideoDecoder::VideoRendererSink> VideoDecoder::GetSink() {
  if (sink_ == NULL) {
    sink_ = new Sink;
  }
  return sink_;
}

scoped_ptr<VideoDecoder::VideoRenderAlgorithm>
VideoDecoder::GetRenderAlgorithm() {
  if (tunnel_mode_audio_session_id_ == -1) {
    return scoped_ptr<VideoRenderAlgorithm>(
        new android::shared::VideoRenderAlgorithm(this));
  }
  return scoped_ptr<VideoRenderAlgorithm>(
      new VideoRenderAlgorithmTunneled(video_frame_tracker_.get()));
}

void VideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                              const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
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
      std::bind(&VideoDecoder::ReportError, this, _1, _2));
}

size_t VideoDecoder::GetPrerollFrameCount() const {
  // Tunnel mode uses its own preroll logic.
  if (tunnel_mode_audio_session_id_ != -1) {
    return 0;
  }
  if (input_buffer_written_ > 0 && first_buffer_timestamp_ != 0) {
    return kNonInitialPrerollFrameCount;
  }
  return kInitialPrerollFrameCount;
}

SbTime VideoDecoder::GetPrerollTimeout() const {
  if (input_buffer_written_ > 0 && first_buffer_timestamp_ != 0) {
    return kSbTimeMax;
  }
  return kInitialPrerollTimeout;
}

void VideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(input_buffer->sample_type() == kSbMediaTypeVideo);
  SB_DCHECK(decoder_status_cb_);

  if (input_buffer_written_ == 0) {
    SB_DCHECK(video_fps_ == 0);
    first_buffer_timestamp_ = input_buffer->timestamp();

    // If color metadata is present and is not an identity mapping, then
    // teardown the codec so it can be reinitalized with the new metadata.
    auto& color_metadata = input_buffer->video_sample_info().color_metadata;
    if (!IsIdentity(color_metadata)) {
      SB_DCHECK(!color_metadata_) << "Unexpected residual color metadata.";
      SB_LOG(INFO) << "Reinitializing codec with HDR color metadata.";
      TeardownCodec();
      color_metadata_ = color_metadata;
    }

    // Re-initialize the codec now if it was torn down either in |Reset| or
    // because we need to change the color metadata.
    if (video_codec_ != kSbMediaVideoCodecAv1 && media_decoder_ == NULL) {
      std::string error_message;
      if (!InitializeCodec(&error_message)) {
        error_message =
            "Failed to reinitialize codec with error: " + error_message;
        SB_LOG(ERROR) << error_message;
        TeardownCodec();
        ReportError(kSbPlayerErrorDecode, error_message);
        return;
      }
    }

    if (tunnel_mode_audio_session_id_ != -1) {
      Schedule(std::bind(&VideoDecoder::OnTunnelModePrerollTimeout, this),
               kInitialPrerollTimeout);
    }
  }

  ++input_buffer_written_;

  if (video_codec_ == kSbMediaVideoCodecAv1 && video_fps_ == 0) {
    SB_DCHECK(!media_decoder_);

    if (pending_input_buffers_.size() <
        kFpsGuesstimateRequiredInputBufferCount) {
      pending_input_buffers_.push_back(input_buffer);
      decoder_status_cb_(kNeedMoreInput, NULL);
      return;
    }
    std::string error_message;
    if (!InitializeCodec(&error_message)) {
      error_message =
          "Failed to reinitialize codec with error: " + error_message;
      SB_LOG(ERROR) << error_message;
      TeardownCodec();
      ReportError(kSbPlayerErrorDecode, error_message);
      return;
    }
  }

  WriteInputBufferInternal(input_buffer);
}

void VideoDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
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
    SB_DCHECK(pending_input_buffers_.size() == input_buffer_written_);

    std::string error_message;
    if (!InitializeCodec(&error_message)) {
      error_message =
          "Failed to reinitialize codec with error: " + error_message;
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

void VideoDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  TeardownCodec();
  CancelPendingJobs();

  tunnel_mode_prerolling_.store(true);
  tunnel_mode_frame_rendered_.store(false);
  input_buffer_written_ = 0;
  end_of_stream_written_ = false;
  video_fps_ = 0;
  pending_input_buffers_.clear();

  // TODO: We rely on VideoRenderAlgorithmTunneled::Seek() to be called inside
  //       VideoRenderer::Seek() after calling VideoDecoder::Reset() to update
  //       the seek status of |video_frame_tracker_|.  This is slightly flaky as
  //       it depends on the behavior of the video renderer.
}

bool VideoDecoder::InitializeCodec(std::string* error_message) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(error_message);

  if (video_codec_ == kSbMediaVideoCodecAv1) {
    SB_DCHECK(pending_input_buffers_.size() > 0);

    // Guesstimate the video fps.
    if (pending_input_buffers_.size() == 1) {
      video_fps_ = 30;
    } else {
      SbTime first_timestamp = pending_input_buffers_[0]->timestamp();
      SbTime second_timestamp = pending_input_buffers_[1]->timestamp();
      if (pending_input_buffers_.size() > 2) {
        second_timestamp =
            std::min(second_timestamp, pending_input_buffers_[2]->timestamp());
      }
      SbTime frame_duration = second_timestamp - first_timestamp;
      if (frame_duration > 0) {
        // To avoid problems caused by deviation of fps calculation, we use the
        // nearest multiple of 5 to check codec capability. So, the fps like 61,
        // 62 will be capped to 60, and 24 will be increased to 25.
        const double kFpsMinDifference = 5;
        video_fps_ =
            std::round(kSbTimeSecond / (second_timestamp - first_timestamp) /
                       kFpsMinDifference) *
            kFpsMinDifference;
      } else {
        video_fps_ = 30;
      }
    }
    SB_DCHECK(video_fps_ > 0);
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
      SbDecodeTarget decode_target =
          DecodeTargetCreate(decode_target_graphics_context_provider_,
                             kSbDecodeTargetFormat1PlaneRGBA, 0, 0);
      if (!SbDecodeTargetIsValid(decode_target)) {
        *error_message = "Could not acquire a decode target from provider.";
        SB_LOG(ERROR) << *error_message;
        return false;
      }
      j_output_surface = decode_target->data->surface;

      JniEnvExt* env = JniEnvExt::Get();
      env->CallVoidMethodOrAbort(decode_target->data->surface_texture,
                                 "setOnFrameAvailableListener", "(J)V", this);

      ScopedLock lock(decode_target_mutex_);
      decode_target_ = decode_target;
    } break;
    case kSbPlayerOutputModeInvalid: {
      SB_NOTREACHED();
    } break;
  }
  if (!j_output_surface) {
    *error_message = "Video surface does not exist.";
    SB_LOG(ERROR) << *error_message;
    return false;
  }

  int width, height;
  if (!GetVideoWindowSize(&width, &height)) {
    *error_message =
        "Can't initialize the codec since we don't have a video window.";
    SB_LOG(ERROR) << *error_message;
    return false;
  }

  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  SB_DCHECK(!drm_system_ || j_media_crypto);
  if (video_codec_ == kSbMediaVideoCodecAv1) {
    SB_DCHECK(video_fps_ > 0);
  } else {
    SB_DCHECK(video_fps_ == 0);
  }
  media_decoder_.reset(new MediaDecoder(
      this, video_codec_, width, height, video_fps_, j_output_surface,
      drm_system_, color_metadata_ ? &*color_metadata_ : nullptr,
      require_software_codec_,
      std::bind(&VideoDecoder::OnTunnelModeFrameRendered, this, _1),
      tunnel_mode_audio_session_id_, force_big_endian_hdr_metadata_,
      error_message));
  if (media_decoder_->is_valid()) {
    if (error_cb_) {
      media_decoder_->Initialize(
          std::bind(&VideoDecoder::ReportError, this, _1, _2));
    }
    media_decoder_->SetPlaybackRate(playback_rate_);

    if (video_codec_ == kSbMediaVideoCodecAv1) {
      SB_DCHECK(!pending_input_buffers_.empty());
    } else {
      SB_DCHECK(pending_input_buffers_.empty());
    }
    while (!pending_input_buffers_.empty()) {
      WriteInputBufferInternal(pending_input_buffers_[0]);
      pending_input_buffers_.pop_front();
    }
    return true;
  }
  media_decoder_.reset();
  return false;
}

void VideoDecoder::TeardownCodec() {
  SB_DCHECK(BelongsToCurrentThread());
  if (owns_video_surface_) {
    ReleaseVideoSurface();
    owns_video_surface_ = false;
  }
  media_decoder_.reset();
  color_metadata_ = starboard::nullopt;

  SbDecodeTarget decode_target_to_release = kSbDecodeTargetInvalid;
  {
    ScopedLock lock(decode_target_mutex_);
    if (SbDecodeTargetIsValid(decode_target_)) {
      // Remove OnFrameAvailableListener to make sure the callback
      // would not be called.
      JniEnvExt* env = JniEnvExt::Get();
      env->CallVoidMethodOrAbort(decode_target_->data->surface_texture,
                                 "removeOnFrameAvailableListener", "()V");

      decode_target_to_release = decode_target_;
      decode_target_ = kSbDecodeTargetInvalid;
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

void VideoDecoder::OnEndOfStreamWritten(MediaCodecBridge* media_codec_bridge) {
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

void VideoDecoder::WriteInputBufferInternal(
    const scoped_refptr<InputBuffer>& input_buffer) {
  // There's a race condition when suspending the app. If surface view is
  // destroyed before video decoder stopped, |media_decoder_| could be null
  // here. And error_cb_() could be handled asynchronously. It's possible
  // that WriteInputBuffer() is called again when the first WriteInputBuffer()
  // fails, in such case |media_decoder_| will be null.
  if (!media_decoder_) {
    SB_LOG(INFO) << "Trying to write input buffer when media_decoder_ is null.";
    return;
  }
  media_decoder_->WriteInputBuffer(input_buffer);
  if (media_decoder_->GetNumberOfPendingTasks() < kMaxPendingWorkSize) {
    decoder_status_cb_(kNeedMoreInput, NULL);
  } else if (tunnel_mode_audio_session_id_ != -1) {
    // In tunnel mode playback when need data is not signaled above, it is
    // possible that the VideoDecoder won't get a chance to send kNeedMoreInput
    // to the renderer again.  Schedule a task to check back.
    Schedule(std::bind(&VideoDecoder::OnTunnelModeCheckForNeedMoreInput, this),
             kNeedMoreInputCheckIntervalInTunnelMode);
  }

  if (tunnel_mode_audio_session_id_ != -1) {
    video_frame_tracker_->OnInputBuffer(input_buffer->timestamp());

    if (tunnel_mode_prerolling_.load()) {
      // TODO: Refine preroll logic in tunnel mode.
      bool enough_buffers_written_to_media_codec = false;
      if (first_buffer_timestamp_ == 0) {
        // Initial playback.
        enough_buffers_written_to_media_codec =
            (input_buffer_written_ - pending_input_buffers_.size() -
             media_decoder_->GetNumberOfPendingTasks()) >
            kInitialPrerollFrameCount;
      } else {
        // Seeking.  Note that this branch can be eliminated once seeking in
        // tunnel mode is always aligned to the next video key frame.
        enough_buffers_written_to_media_codec =
            (input_buffer_written_ - pending_input_buffers_.size() -
             media_decoder_->GetNumberOfPendingTasks()) >
                kSeekingPrerollPendingWorkSizeInTunnelMode &&
            input_buffer->timestamp() >= video_frame_tracker_->seek_to_time();
      }

      bool cache_full =
          media_decoder_->GetNumberOfPendingTasks() >= kMaxPendingWorkSize;
      bool prerolled = tunnel_mode_frame_rendered_.load() > 0 ||
                       enough_buffers_written_to_media_codec || cache_full;

      if (prerolled && tunnel_mode_prerolling_.exchange(false)) {
        SB_LOG(INFO)
            << "Tunnel mode preroll finished on enqueuing input buffer "
            << input_buffer->timestamp() << ", for seek time "
            << video_frame_tracker_->seek_to_time();
        decoder_status_cb_(
            kNeedMoreInput,
            new VideoFrame(video_frame_tracker_->seek_to_time()));
      }
    }
  }
}

void VideoDecoder::ProcessOutputBuffer(
    MediaCodecBridge* media_codec_bridge,
    const DequeueOutputResult& dequeue_output_result) {
  SB_DCHECK(decoder_status_cb_);
  SB_DCHECK(dequeue_output_result.index >= 0);

  bool is_end_of_stream =
      dequeue_output_result.flags & BUFFER_FLAG_END_OF_STREAM;
  decoder_status_cb_(
      is_end_of_stream ? kBufferFull : kNeedMoreInput,
      new VideoFrameImpl(dequeue_output_result, media_codec_bridge));
}

void VideoDecoder::RefreshOutputFormat(MediaCodecBridge* media_codec_bridge) {
  SB_DCHECK(media_codec_bridge);
  SB_DLOG(INFO) << "Output format changed, trying to dequeue again.";

  ScopedLock lock(decode_target_mutex_);
  // Record the latest width/height of the decoded input.
  SurfaceDimensions output_dimensions =
      media_codec_bridge->GetOutputDimensions();
  frame_width_ = output_dimensions.width;
  frame_height_ = output_dimensions.height;
}

bool VideoDecoder::Tick(MediaCodecBridge* media_codec_bridge) {
  // Tunnel mode renders frames in MediaCodec automatically and shouldn't reach
  // here.
  SB_DCHECK(tunnel_mode_audio_session_id_ == -1);
  return sink_->Render();
}

void VideoDecoder::OnFlushing() {
  decoder_status_cb_(kReleaseAllFrames, NULL);
}

namespace {
void updateTexImage(jobject surface_texture) {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(surface_texture, "updateTexImage", "()V");
}

void getTransformMatrix(jobject surface_texture, float* matrix4x4) {
  JniEnvExt* env = JniEnvExt::Get();

  jfloatArray java_array = env->NewFloatArray(16);
  SB_DCHECK(java_array);

  env->CallVoidMethodOrAbort(surface_texture, "getTransformMatrix", "([F)V",
                             java_array);

  jfloat* array_values = env->GetFloatArrayElements(java_array, 0);
  memcpy(matrix4x4, array_values, sizeof(float) * 16);

  env->DeleteLocalRef(java_array);
}

// Rounds the float to the nearest integer, and also does a DCHECK to make sure
// that the input float was already near an integer value.
int RoundToNearInteger(float x) {
  int rounded = static_cast<int>(x + 0.5f);
  return rounded;
}

// Converts a 4x4 matrix representing the texture coordinate transform into
// an equivalent rectangle representing the region within the texture where
// the pixel data is valid.  Note that the width and height of this region may
// be negative to indicate that that axis should be flipped.
void SetDecodeTargetContentRegionFromMatrix(
    SbDecodeTargetInfoContentRegion* content_region,
    int width,
    int height,
    const float* matrix4x4) {
  // Ensure that this matrix contains no rotations or shears.  In other words,
  // make sure that we can convert it to a decode target content region without
  // losing any information.
  SB_DCHECK(matrix4x4[1] == 0.0f);
  SB_DCHECK(matrix4x4[2] == 0.0f);
  SB_DCHECK(matrix4x4[3] == 0.0f);

  SB_DCHECK(matrix4x4[4] == 0.0f);
  SB_DCHECK(matrix4x4[6] == 0.0f);
  SB_DCHECK(matrix4x4[7] == 0.0f);

  SB_DCHECK(matrix4x4[8] == 0.0f);
  SB_DCHECK(matrix4x4[9] == 0.0f);
  SB_DCHECK(matrix4x4[10] == 1.0f);
  SB_DCHECK(matrix4x4[11] == 0.0f);

  SB_DCHECK(matrix4x4[14] == 0.0f);
  SB_DCHECK(matrix4x4[15] == 1.0f);

  float origin_x = matrix4x4[12];
  float origin_y = matrix4x4[13];

  float extent_x = matrix4x4[0] + matrix4x4[12];
  float extent_y = matrix4x4[5] + matrix4x4[13];

  SB_DCHECK(origin_y >= 0.0f);
  SB_DCHECK(origin_y <= 1.0f);
  SB_DCHECK(origin_x >= 0.0f);
  SB_DCHECK(origin_x <= 1.0f);
  SB_DCHECK(extent_x >= 0.0f);
  SB_DCHECK(extent_x <= 1.0f);
  SB_DCHECK(extent_y >= 0.0f);
  SB_DCHECK(extent_y <= 1.0f);

  // Flip the y-axis to match ContentRegion's coordinate system.
  origin_y = 1.0f - origin_y;
  extent_y = 1.0f - extent_y;

  content_region->left = origin_x * width;
  content_region->right = extent_x * width;

  // Note that in GL coordinates, the origin is the bottom and the extent
  // is the top.
  content_region->top = extent_y * height;
  content_region->bottom = origin_y * height;
}
}  // namespace

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget VideoDecoder::GetCurrentDecodeTarget() {
  SB_DCHECK(output_mode_ == kSbPlayerOutputModeDecodeToTexture);
  // We must take a lock here since this function can be called from a separate
  // thread.
  ScopedLock lock(decode_target_mutex_);
  if (SbDecodeTargetIsValid(decode_target_)) {
    bool has_new_texture = has_new_texture_available_.exchange(false);
    if (has_new_texture) {
      updateTexImage(decode_target_->data->surface_texture);

      decode_target_->data->info.planes[0].width = frame_width_;
      decode_target_->data->info.planes[0].height = frame_height_;
      decode_target_->data->info.width = frame_width_;
      decode_target_->data->info.height = frame_height_;

      float matrix4x4[16];
      getTransformMatrix(decode_target_->data->surface_texture, matrix4x4);
      SetDecodeTargetContentRegionFromMatrix(
          &decode_target_->data->info.planes[0].content_region, frame_width_,
          frame_height_, matrix4x4);

      if (!first_texture_received_) {
        first_texture_received_ = true;
      }
    }

    if (first_texture_received_) {
      SbDecodeTarget out_decode_target = new SbDecodeTargetPrivate;
      out_decode_target->data = decode_target_->data;
      return out_decode_target;
    }
  }
  return kSbDecodeTargetInvalid;
}

void VideoDecoder::SetPlaybackRate(double playback_rate) {
  playback_rate_ = playback_rate;
  if (media_decoder_) {
    media_decoder_->SetPlaybackRate(playback_rate);
  }
}

void VideoDecoder::OnNewTextureAvailable() {
  has_new_texture_available_.store(true);
}

void VideoDecoder::OnTunnelModeFrameRendered(SbTime frame_timestamp) {
  SB_DCHECK(tunnel_mode_audio_session_id_ != -1);

  tunnel_mode_frame_rendered_.store(true);
  video_frame_tracker_->OnFrameRendered(frame_timestamp);
}

void VideoDecoder::OnTunnelModePrerollTimeout() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(tunnel_mode_audio_session_id_ != -1);

  if (tunnel_mode_prerolling_.exchange(false)) {
    SB_LOG(INFO) << "Tunnel mode preroll finished due to timeout.";
    // TODO: Currently the decoder sends a dummy frame to the renderer to signal
    //       preroll finish.  We should investigate a better way for prerolling
    //       when the video is rendered directly by the decoder, maybe by always
    //       sending placeholder frames.
    decoder_status_cb_(kNeedMoreInput,
                       new VideoFrame(video_frame_tracker_->seek_to_time()));
  }
}

void VideoDecoder::OnTunnelModeCheckForNeedMoreInput() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(tunnel_mode_audio_session_id_ != -1);

  // There's a race condition when suspending the app. If surface view is
  // destroyed before this function is called, |media_decoder_| could be null
  // here, in such case |media_decoder_| will be null.
  if (!media_decoder_) {
    SB_LOG(INFO) << "Trying to call OnTunnelModeCheckForNeedMoreInput() when"
                 << " media_decoder_ is null.";
    return;
  }

  if (media_decoder_->GetNumberOfPendingTasks() < kMaxPendingWorkSize) {
    decoder_status_cb_(kNeedMoreInput, NULL);
    return;
  }

  Schedule(std::bind(&VideoDecoder::OnTunnelModeCheckForNeedMoreInput, this),
           kNeedMoreInputCheckIntervalInTunnelMode);
}

void VideoDecoder::OnSurfaceDestroyed() {
  if (!BelongsToCurrentThread()) {
    // Wait until codec is stopped.
    ScopedLock lock(surface_destroy_mutex_);
    Schedule(std::bind(&VideoDecoder::OnSurfaceDestroyed, this));
    surface_condition_variable_.WaitTimed(kSbTimeSecond);
    return;
  }
  // When this function is called, the decoder no longer owns the surface.
  owns_video_surface_ = false;
  TeardownCodec();
  ScopedLock lock(surface_destroy_mutex_);
  surface_condition_variable_.Signal();
}

void VideoDecoder::ReportError(SbPlayerError error,
                               const std::string& error_message) {
  SB_DCHECK(error_cb_);

  if (!error_cb_) {
    return;
  }

  error_cb_(kSbPlayerErrorDecode, error_message);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_VideoSurfaceTexture_nativeOnFrameAvailable(
    JNIEnv* env,
    jobject unused_this,
    jlong native_video_decoder) {
  using starboard::android::shared::VideoDecoder;

  VideoDecoder* video_decoder =
      reinterpret_cast<VideoDecoder*>(native_video_decoder);
  SB_DCHECK(video_decoder);
  video_decoder->OnNewTextureAvailable();
}
