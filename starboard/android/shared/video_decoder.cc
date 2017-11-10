// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/decode_target_create.h"
#include "starboard/android/shared/decode_target_internal.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/android/shared/window_internal.h"
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
                       : dequeue_output_result.presentation_time_microseconds *
                             kSbMediaTimeSecond / kSbTimeSecond),
        dequeue_output_result_(dequeue_output_result),
        media_codec_bridge_(media_codec_bridge),
        released_(false) {
    SB_DCHECK(media_codec_bridge_);
  }

  ~VideoFrameImpl() {
    if (!released_) {
      media_codec_bridge_->ReleaseOutputBuffer(dequeue_output_result_.index,
                                               false);
      if (is_end_of_stream()) {
        media_codec_bridge_->Flush();
      }
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

const int kInitialPrerollFrameCount = 8;
const int kNonInitialPrerollFrameCount = 1;

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
    SB_UNREFERENCED_PARAMETER(z_index);
    SB_UNREFERENCED_PARAMETER(x);
    SB_UNREFERENCED_PARAMETER(y);
    SB_UNREFERENCED_PARAMETER(width);
    SB_UNREFERENCED_PARAMETER(height);
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

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec,
                           SbDrmSystem drm_system,
                           SbPlayerOutputMode output_mode,
                           SbDecodeTargetGraphicsContextProvider*
                               decode_target_graphics_context_provider)
    : video_codec_(video_codec),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      decode_target_(kSbDecodeTargetInvalid),
      frame_width_(0),
      frame_height_(0),
      has_written_buffer_since_reset_(false),
      first_buffer_received_(false) {
  if (!InitializeCodec()) {
    SB_LOG(ERROR) << "Failed to initialize video decoder.";
    TeardownCodec();
  }
}

VideoDecoder::~VideoDecoder() {
  TeardownCodec();
  ClearVideoWindow();
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

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;

  media_decoder_->Initialize(error_cb_);
}

size_t VideoDecoder::GetPrerollFrameCount() const {
  if (first_buffer_received_ && first_buffer_pts_ != 0) {
    return kNonInitialPrerollFrameCount;
  }
  return kInitialPrerollFrameCount;
}

SbTime VideoDecoder::GetPrerollTimeout() const {
  if (first_buffer_received_ && first_buffer_pts_ != 0) {
    return kSbTimeMax;
  }
  return kInitialPrerollTimeout;
}

void VideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(input_buffer);
  SB_DCHECK(decoder_status_cb_);

  if (!first_buffer_received_) {
    first_buffer_received_ = true;
    first_buffer_pts_ = input_buffer->pts();
  }

  if (!has_written_buffer_since_reset_) {
    SB_LOG(INFO) << "Attempting to reconfigure with HDR metadata";
    has_written_buffer_since_reset_ = true;
    // If color metadata is present, is not "null", and different than our
    // previously queued (which is possibly already set) color metadata, then
    // queue up recreating the MediaCodec video decoder using the new meta data.
    auto* color_metadata = input_buffer->video_sample_info()->color_metadata;
    if (color_metadata && !IsIdentity(*color_metadata) &&
        !previous_color_metadata_) {
      previous_color_metadata_ = *color_metadata;
      TeardownCodec();
      if (!InitializeCodec()) {
        SB_LOG(ERROR) << "Failed to reinitialize codec with HDR metadata.";
      }
    }
  }

  media_decoder_->WriteInputBuffer(input_buffer);
  if (number_of_frames_being_decoded_.increment() < kMaxPendingWorkSize) {
    decoder_status_cb_(kNeedMoreInput, NULL);
  }
}

void VideoDecoder::WriteEndOfStream() {
  SB_DCHECK(decoder_status_cb_);

  if (!first_buffer_received_) {
    first_buffer_received_ = true;
    first_buffer_pts_ = 0;
  }

  media_decoder_->WriteEndOfStream();
}

void VideoDecoder::Reset() {
  TeardownCodec();
  number_of_frames_being_decoded_.store(0);
  first_buffer_received_ = false;
  if (!InitializeCodec()) {
    // TODO: Communicate this failure to our clients somehow.
    SB_LOG(ERROR) << "Failed to initialize codec after reset.";
  }
}

bool VideoDecoder::InitializeCodec() {
  // Setup the output surface object.  If we are in punch-out mode, target
  // the passed in Android video surface.  If we are in decode-to-texture
  // mode, create a surface from a new texture target and use that as the
  // output surface.
  jobject j_output_surface = NULL;
  switch (output_mode_) {
    case kSbPlayerOutputModePunchOut: {
      j_output_surface = GetVideoSurface();
      SB_DCHECK(j_output_surface);
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
        SB_LOG(ERROR) << "Could not acquire a decode target from provider.";
        return false;
      }
      j_output_surface = decode_target->data->surface;

      starboard::ScopedLock lock(decode_target_mutex_);
      decode_target_ = decode_target;
    } break;
    case kSbPlayerOutputModeInvalid: {
      SB_NOTREACHED();
    } break;
  }
  SB_DCHECK(j_output_surface);

  ANativeWindow* video_window = GetVideoWindow();
  SB_DCHECK(video_window);
  int width = ANativeWindow_getWidth(video_window);
  int height = ANativeWindow_getHeight(video_window);

  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  SB_DCHECK(!drm_system_ || j_media_crypto);
  media_decoder_.reset(new MediaDecoder(
      this, video_codec_, width, height, j_output_surface, drm_system_,
      previous_color_metadata_ ? &*previous_color_metadata_ : nullptr));
  if (media_decoder_->is_valid()) {
    if (error_cb_) {
      media_decoder_->Initialize(error_cb_);
    }
    return true;
  }
  media_decoder_.reset();
  return false;
}

void VideoDecoder::TeardownCodec() {
  media_decoder_.reset();

  starboard::ScopedLock lock(decode_target_mutex_);
  if (SbDecodeTargetIsValid(decode_target_)) {
    SbDecodeTargetReleaseInGlesContext(decode_target_graphics_context_provider_,
                                       decode_target_);
    decode_target_ = kSbDecodeTargetInvalid;
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
  // Record the latest width/height of the decoded input.
  SurfaceDimensions output_dimensions =
      media_codec_bridge->GetOutputDimensions();
  frame_width_ = output_dimensions.width;
  frame_height_ = output_dimensions.height;
}

bool VideoDecoder::Tick(MediaCodecBridge* media_codec_bridge) {
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

  content_region->left = RoundToNearInteger(origin_x * width);
  content_region->right = RoundToNearInteger(extent_x * width);

  // Note that in GL coordinates, the origin is the bottom and the extent
  // is the top.
  content_region->top = RoundToNearInteger(extent_y * height);
  content_region->bottom = RoundToNearInteger(origin_y * height);
}
}  // namespace

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget VideoDecoder::GetCurrentDecodeTarget() {
  SB_DCHECK(output_mode_ == kSbPlayerOutputModeDecodeToTexture);
  // We must take a lock here since this function can be called from a separate
  // thread.
  starboard::ScopedLock lock(decode_target_mutex_);

  if (SbDecodeTargetIsValid(decode_target_)) {
    updateTexImage(decode_target_->data->surface_texture);

    float matrix4x4[16];
    getTransformMatrix(decode_target_->data->surface_texture, matrix4x4);
    SetDecodeTargetContentRegionFromMatrix(
        &decode_target_->data->info.planes[0].content_region, frame_width_,
        frame_height_, matrix4x4);

    // Take this opportunity to update the decode target's width and height.
    decode_target_->data->info.planes[0].width = frame_width_;
    decode_target_->data->info.planes[0].height = frame_height_;
    decode_target_->data->info.width = frame_width_;
    decode_target_->data->info.height = frame_height_;

    SbDecodeTarget out_decode_target = new SbDecodeTargetPrivate;
    out_decode_target->data = decode_target_->data;

    return out_decode_target;
  } else {
    return kSbDecodeTargetInvalid;
  }
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// static
bool VideoDecoder::OutputModeSupported(SbPlayerOutputMode output_mode,
                                       SbMediaVideoCodec codec,
                                       SbDrmSystem drm_system) {
  if (output_mode == kSbPlayerOutputModePunchOut) {
    return true;
  }

  if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
    return !SbDrmSystemIsValid(drm_system);
  }

  return false;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
