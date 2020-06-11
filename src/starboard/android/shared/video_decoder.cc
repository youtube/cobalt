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

#include <cmath>
#include <functional>
#include <list>

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/decode_target_create.h"
#include "starboard/android/shared/decode_target_internal.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_agency.h"
#include "starboard/android/shared/media_common.h"
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
const SbTime kPrerollTimeoutRetryInterval = 25 * kSbTimeMillisecond;

const int kInitialPrerollFrameCount = 8;
const int kNonInitialPrerollFrameCount = 1;

const int kInitialPrerollPendingWorkSizeInTunneldMode =
    16 + kInitialPrerollFrameCount;
const int kMaxPendingWorkSize = 128;

// Convenience HDR mastering metadata.
const SbMediaMasteringMetadata kEmptyMasteringMetadata = {};

// Determine if two |SbMediaMasteringMetadata|s are equal.
bool Equal(const SbMediaMasteringMetadata& lhs,
           const SbMediaMasteringMetadata& rhs) {
  return SbMemoryCompare(&lhs, &rhs, sizeof(SbMediaMasteringMetadata)) == 0;
}

// Determine if two |SbMediaColorMetadata|s are equal.
bool Equal(const SbMediaColorMetadata& lhs, const SbMediaColorMetadata& rhs) {
  return SbMemoryCompare(&lhs, &rhs, sizeof(SbMediaMasteringMetadata)) == 0;
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

}  // namespace

class VideoFrameTracker : public RefCountedThreadSafe<VideoFrameTracker> {
 public:
  VideoFrameTracker(int tunneling_audio_session_id,
                    const VideoFramesDroppedCB& tunneling_frames_dropped_cb)
      : tunneling_audio_session_id_(tunneling_audio_session_id),
        frames_dropped_cb_(tunneling_frames_dropped_cb),
        seek_to_time_(-1),
        dropped_frames_(0) {}

  ~VideoFrameTracker() {}

  void TrackFrame(SbTime media_time_us, SbTime track_time_us) {
    starboard::ScopedLock lock(mutex_);
    if (frame_queue_.empty()) {
      frame_queue_.emplace_back(media_time_us, track_time_us);
      return;
    }
    // sort by media_time_us. Because if there is B frames, the
    // media_time_us is not monotonic.
    for (std::list<TrackedFrameInfo>::iterator it = frame_queue_.end();
         it != frame_queue_.begin();) {
      it--;
      if (it->media_time_us_ < media_time_us) {
        frame_queue_.emplace(++it, media_time_us, track_time_us);
        return;
      } else if (it->media_time_us_ == media_time_us) {
        SB_LOG(WARNING) << "feed video ES with same time stamp"
                        << media_time_us;
        return;
      }
    }
    frame_queue_.emplace_front(media_time_us, track_time_us);
  }

  // |media_time_us| should >= |seek_to_time_| because platform would drop
  // video frames whose timestamp is less than |seek_to_time|
  void OnFrameRendered(SbTime media_time_us, SbTime render_time_us) {
    starboard::ScopedLock lock(mutex_);
    std::list<TrackedFrameInfo>::iterator it = frame_queue_.begin();
    while (it != frame_queue_.end()) {
      if (it->media_time_us_ > media_time_us) {
        break;
      }

      if (it->media_time_us_ < seek_to_time_) {
        it = frame_queue_.erase(it);
      } else if (it->media_time_us_ < media_time_us) {
        SB_LOG(ERROR) << "Video Frame Drop:" << it->media_time_us_
                      << " current media time:" << media_time_us
                      << " not rendered frames:" << frame_queue_.size();
        dropped_frames_++;
        if (dropped_frames_) {
          frames_dropped_cb_(dropped_frames_);
        }
        it = frame_queue_.erase(it);
      } else if (it->media_time_us_ == media_time_us) {
        it = frame_queue_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void Reset() {
    starboard::ScopedLock lock(mutex_);
    frame_queue_.clear();
    seek_to_time_ = -1;
    is_prerolling_ = false;
    media_codec_input_buffer_available_count = 0;
  }

  void SetSeekTime(SbTime seek_time_us) {
    starboard::ScopedLock lock(mutex_);
    seek_to_time_ = seek_time_us;
  }

  void SetPrerolling() {
    starboard::ScopedLock lock(mutex_);
    is_prerolling_ = true;
  }

  bool IsPrerolling() {
    starboard::ScopedLock lock(mutex_);
    return is_prerolling_;
  }

  void SetDecoderStatusCb(
      const VideoDecoder::DecoderStatusCB& decoder_status_cb) {
    starboard::ScopedLock lock(mutex_);
    decoder_status_cb_ = decoder_status_cb;
  }

  void OnMediaCodecInputBufferAvailable() {
    starboard::ScopedLock lock(mutex_);
    media_codec_input_buffer_available_count++;
  }

  int GetMediaCodecInputBufferAvailableCount() {
    starboard::ScopedLock lock(mutex_);
    return media_codec_input_buffer_available_count;
  }

  void SignalPrerollDoneIfNecessary(bool force = false) {
    starboard::ScopedLock lock(mutex_);
    if (!is_prerolling_) {
      return;
    }

    if (force) {
      decoder_status_cb_(VideoDecoder::kNeedMoreInput, new VideoFrame(0));
      is_prerolling_ = false;
      SB_LOG(INFO) << "force video preroll done first frame ts:"
                   << " seek time:" << seek_to_time_
                   << " frame fed count:" << frame_queue_.size()
                   << " media codec input buffer avaiable count:"
                   << media_codec_input_buffer_available_count;
      return;
    }

    if (media_codec_input_buffer_available_count > kInitialPrerollFrameCount &&
        ((seek_to_time_ != -1 &&
          frame_queue_.size() > kInitialPrerollPendingWorkSizeInTunneldMode &&
          frame_queue_.back().media_time_us_ >= seek_to_time_) ||
         frame_queue_.size() >= kMaxPendingWorkSize)) {
      SB_LOG(INFO) << "video preroll done first frame ts:"
                   << frame_queue_.front().media_time_us_
                   << " seek time:" << seek_to_time_
                   << " frame fed count:" << frame_queue_.size()
                   << " media codec input buffer avaiable count:"
                   << media_codec_input_buffer_available_count;
      decoder_status_cb_(VideoDecoder::kNeedMoreInput, new VideoFrame(0));
      is_prerolling_ = false;
    }
  }

 private:
  struct TrackedFrameInfo {
    TrackedFrameInfo(SbTime media_time_us, SbTime track_time_us) {
      media_time_us_ = media_time_us;
      track_time_us_ = track_time_us;
    }
    SbTime media_time_us_;
    SbTime track_time_us_;
  };

  std::list<TrackedFrameInfo> frame_queue_;
  int tunneling_audio_session_id_;
  SbTime seek_to_time_;
  starboard::Mutex mutex_;
  int dropped_frames_ = 0;
  bool is_prerolling_ = false;
  VideoFramesDroppedCB frames_dropped_cb_ = nullptr;
  int media_codec_input_buffer_available_count = 0;
  VideoDecoder::DecoderStatusCB decoder_status_cb_;
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

  void SetBounds(int z_index, int x, int y, int width, int height) override {
  }

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

VideoDecoder::VideoDecoder(
    SbMediaVideoCodec video_codec,
    SbDrmSystem drm_system,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider,
    const char* max_video_capabilities,
    int tunneling_audio_session_id,
    const VideoFramesDroppedCB& tunneling_frames_dropped_cb,
    std::string* error_message)
    : video_codec_(video_codec),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      tunneling_audio_session_id_(tunneling_audio_session_id),
      has_new_texture_available_(false),
      surface_condition_variable_(surface_destroy_mutex_),
      require_software_codec_(max_video_capabilities &&
                              SbStringGetLength(max_video_capabilities) > 0),
      input_end_of_stream_(false) {
  SB_DCHECK(error_message);

  video_frame_tracker_ = new VideoFrameTracker(tunneling_audio_session_id_,
                                               tunneling_frames_dropped_cb);
  MediaAgency::GetInstance()->RegisterPlayerClient(
      tunneling_audio_session_id_, this,
      std::bind(&VideoDecoder::OnPlaybackStatus, this, _1, _2));

  if (require_software_codec_) {
    SB_DCHECK(output_mode_ == kSbPlayerOutputModeDecodeToTexture);
  }
  if (!require_software_codec_) {
    number_of_hardware_decoders_++;
  }

  if (!InitializeCodec(error_message)) {
    *error_message =
        "Failed to initialize video decoder with error: " + *error_message;
    SB_LOG(ERROR) << *error_message;
    TeardownCodec();
  }
}

VideoDecoder::~VideoDecoder() {
  MediaAgency::GetInstance()->UnregisterPlayerClient(
      tunneling_audio_session_id_);
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

void VideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                              const ErrorCB& error_cb) {
  SB_DCHECK(media_decoder_);

  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);
  {
    starboard::ScopedLock lock(decoder_status_cb_mutex_);
    decoder_status_cb_ = decoder_status_cb;
    video_frame_tracker_->SetDecoderStatusCb(decoder_status_cb);
  }
  error_cb_ = error_cb;

  media_decoder_->Initialize(
      error_cb_, std::bind(&VideoDecoder::onFrameRendered, this, _1, _2));
}

void VideoDecoder::OnPlaybackStatus(int action, SbTime seek_to_time_us) {
  // OnPlaybackStatus() will not happen after |VideoDecoder| destruction.
  // Because UnregisterPlayerClient() is prior to |VideoDecoder| destruction
  if (action & kSbPlaybackStatusUpdateSeekTime) {
    video_frame_tracker_->SetSeekTime(seek_to_time_us);
    if (tunneling_audio_session_id_ != -1) {
      video_frame_tracker_->SetPrerolling();
      video_frame_tracker_->SignalPrerollDoneIfNecessary();
      if (video_frame_tracker_->IsPrerolling()) {
        Schedule(std::bind(&VideoDecoder::OnPrerollTimeout, this),
                 kInitialPrerollTimeout);
      }
    }
  }
}

void VideoDecoder::onFrameRendered(int64_t presentation_time_us,
                                   int64_t render_at_system_time_ns) {
  video_frame_tracker_->OnFrameRendered(presentation_time_us,
                                        render_at_system_time_ns);
}

size_t VideoDecoder::GetPrerollFrameCount() const {
  // Tunneled mode use another way for preroll other than decoded
  // frame count
  if (tunneling_audio_session_id_ != -1) {
    return 0;
  }
  if (first_buffer_received_ && first_buffer_timestamp_ != 0) {
    return kNonInitialPrerollFrameCount;
  }
  return kInitialPrerollFrameCount;
}

SbTime VideoDecoder::GetPrerollTimeout() const {
  if (first_buffer_received_ && first_buffer_timestamp_ != 0) {
    return kSbTimeMax;
  }
  return kInitialPrerollTimeout;
}

// only for tunneled mode
void VideoDecoder::OnPrerollTimeout() {
  if (video_frame_tracker_->IsPrerolling()) {
    if (video_frame_tracker_->GetMediaCodecInputBufferAvailableCount() > 0) {
      // trigger seek preroll done for tunneled mode
      video_frame_tracker_->SignalPrerollDoneIfNecessary(true);
    } else {
      Schedule(std::bind(&VideoDecoder::OnPrerollTimeout, this),
               kPrerollTimeoutRetryInterval);
    }
  }
}

void VideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(input_buffer);
  SB_DCHECK(input_buffer->sample_type() == kSbMediaTypeVideo);
  SB_DCHECK(decoder_status_cb_);

  if (!first_buffer_received_) {
    first_buffer_received_ = true;
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
    if (media_decoder_ == NULL) {
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
  }

  // There's a race condition when suspending the app. If surface view is
  // destroyed before video decoder stopped, |media_decoder_| could be null
  // here. And error_cb_() could be handled asynchronously. It's possible
  // that WriteInputBuffer() is called again when the first WriteInputBuffer()
  // fails, in such case is_valid() will also return false.
  if (!is_valid()) {
    SB_LOG(INFO) << "Trying to write input buffer when codec is not available.";
    return;
  }
  media_decoder_->WriteInputBuffer(input_buffer);
  if (number_of_frames_being_decoded_.increment() < kMaxPendingWorkSize) {
    decoder_status_cb_(kNeedMoreInput, NULL);
  }
  video_frame_tracker_->TrackFrame(input_buffer->timestamp(),
                                   SbTimeGetMonotonicNow());
  if (tunneling_audio_session_id_ != -1 &&
      video_frame_tracker_->IsPrerolling()) {
    video_frame_tracker_->SignalPrerollDoneIfNecessary();
  }
}

void VideoDecoder::WriteEndOfStream() {
  SB_DCHECK(decoder_status_cb_);

  if (!first_buffer_received_) {
    // In this case, |media_decoder_|'s decoder thread is not initialized.
    // Return EOS frame directly.
    first_buffer_received_ = true;
    first_buffer_timestamp_ = 0;
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
    return;
  }

  // There's a race condition when suspending the app. If surface view is
  // destroyed before video decoder stopped, |media_decoder_| could be null
  // here. And error_cb_() could be handled asynchronously. It's possible
  // that WriteEndOfStream() is called immediately after the first
  // WriteInputBuffer() fails, in such case is_valid() will also return false.
  if (!is_valid()) {
    SB_LOG(INFO)
        << "Trying to write end of stream when codec is not available.";
    return;
  }
  // tunneled mode use input eos instead of output eos
  input_end_of_stream_ = true;
  media_decoder_->WriteEndOfStream();
}

void VideoDecoder::Reset() {
  TeardownCodec();
  number_of_frames_being_decoded_.store(0);
  first_buffer_received_ = false;
  video_frame_tracker_->Reset();
}

bool VideoDecoder::InitializeCodec(std::string* error_message) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(error_message);

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

      starboard::ScopedLock lock(decode_target_mutex_);
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
  media_decoder_.reset(new MediaDecoder(
      this, video_codec_, width, height, j_output_surface, drm_system_,
      color_metadata_ ? &*color_metadata_ : nullptr, require_software_codec_,
      tunneling_audio_session_id_, error_message));
  if (media_decoder_->is_valid()) {
    if (error_cb_) {
      media_decoder_->Initialize(
          std::bind(&VideoDecoder::ReportError, this, _1, _2),
          std::bind(&VideoDecoder::onFrameRendered, this, _1, _2));
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
    starboard::ScopedLock lock(decode_target_mutex_);
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

void VideoDecoder::ProcessInputBuffer(MediaCodecBridge* media_codec_bridge,
                                      bool end_of_stream_reached) {
  if (tunneling_audio_session_id_ == -1) {
    return;
  }

  number_of_frames_being_decoded_.decrement();

  starboard::ScopedLock lock(decoder_status_cb_mutex_);
  if (decoder_status_cb_) {
    video_frame_tracker_->OnMediaCodecInputBufferAvailable();
    if (video_frame_tracker_->IsPrerolling()) {
      video_frame_tracker_->SignalPrerollDoneIfNecessary(end_of_stream_reached);
    }
    if (end_of_stream_reached) {
      // Tunneled mode try to trigger EOS and end_cb in high level when EOS is
      // sent into video decoder. Since the final EOS is determined by both
      // audio and video EOS, video EOS being not very accurate would not be
      // problem.
      decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
      sink_->Render();
    } else {
      decoder_status_cb_(
          input_end_of_stream_ ? kBufferFull : kNeedMoreInput,
          NULL /*new VideoFrameImpl(dequeue_output_result, media_codec_bridge)*/);
    }
  }
}

void VideoDecoder::ProcessOutputBuffer(
    MediaCodecBridge* media_codec_bridge,
    const DequeueOutputResult& dequeue_output_result) {
  SB_DCHECK(decoder_status_cb_);
  SB_DCHECK(dequeue_output_result.index >= 0);

  number_of_frames_being_decoded_.decrement();
  bool is_end_of_stream =
      dequeue_output_result.flags & BUFFER_FLAG_END_OF_STREAM;
  decoder_status_cb_(
      is_end_of_stream ? kBufferFull : kNeedMoreInput,
      new VideoFrameImpl(dequeue_output_result, media_codec_bridge));
}

void VideoDecoder::RefreshOutputFormat(MediaCodecBridge* media_codec_bridge) {
  SB_DCHECK(media_codec_bridge);
  SB_DLOG(INFO) << "Output format changed, trying to dequeue again.";
  starboard::ScopedLock lock(decode_target_mutex_);
  // Record the latest width/height of the decoded input.
  SurfaceDimensions output_dimensions =
      media_codec_bridge->GetOutputDimensions();
  frame_width_ = output_dimensions.width;
  frame_height_ = output_dimensions.height;
}

bool VideoDecoder::Tick(MediaCodecBridge* media_codec_bridge) {
  // tunneled mode render in low level. Should not be here.
  SB_DCHECK(tunneling_audio_session_id_ == -1);
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
  SbMemoryCopy(matrix4x4, array_values, sizeof(float) * 16);

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
  starboard::ScopedLock lock(decode_target_mutex_);
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

void VideoDecoder::OnNewTextureAvailable() {
  has_new_texture_available_.store(true);
}

void VideoDecoder::OnSurfaceDestroyed() {
  if (!BelongsToCurrentThread()) {
    // Wait until codec is stoped.
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
