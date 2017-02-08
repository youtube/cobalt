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

#include <cmath>

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/decode_target_internal.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/android/shared/window_internal.h"
#include "starboard/decode_target.h"
#include "starboard/drm.h"
#include "starboard/memory.h"
#include "starboard/string.h"
#include "starboard/thread.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

const int64_t kSecondInMicroseconds = 1000 * 1000;
const int64_t kDequeueTimeout = kSecondInMicroseconds;
const SbMediaTime kOutputBufferWorkFailure = -1;
const SbMediaTime kSbMediaTimeNone = -1;
const SbTime kFrameTooEarlyThreshold = -15000;

int64_t ConvertToMicroseconds(const SbMediaTime media_time) {
  return media_time * kSecondInMicroseconds / kSbMediaTimeSecond;
}

SbMediaTime ConvertToMediaTime(const int64_t microseconds) {
  return microseconds * kSbMediaTimeSecond / kSecondInMicroseconds;
}

// Map an |SbMediaVideoCodec| into its corresponding mime type string.
const char* GetMimeTypeFromCodecType(const SbMediaVideoCodec type) {
  switch (type) {
    case kSbMediaVideoCodecNone:
      return NULL;
    case kSbMediaVideoCodecH264:
      return "video/avc";
    case kSbMediaVideoCodecH265:
      return "video/hevc";
    case kSbMediaVideoCodecMpeg2:
      return NULL;
    case kSbMediaVideoCodecTheora:
      return NULL;
    case kSbMediaVideoCodecVc1:
      return NULL;
    case kSbMediaVideoCodecVp10:
      return NULL;
    case kSbMediaVideoCodecVp8:
      return "video/x-vnd.on2.vp8";
    case kSbMediaVideoCodecVp9:
      return "video/x-vnd.on2.vp9";
  }
  return NULL;
}

}  // namespace

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec,
                           SbPlayerOutputMode output_mode,
                           SbDecodeTargetProvider* decode_target_provider)
    : video_codec_(video_codec),
      host_(NULL),
      stream_ended_(false),
      error_occured_(false),
      decoder_thread_(kSbThreadInvalid),
      media_format_(NULL),
      media_codec_(NULL),
      width_(-1),
      height_(-1),
      color_format_(-1),
      output_mode_(output_mode),
      decode_target_provider_(decode_target_provider),
      decode_target_(kSbDecodeTargetInvalid),
      frame_width_(0),
      frame_height_(0) {
  if (!InitializeCodec()) {
    TeardownCodec();
  }
}

VideoDecoder::~VideoDecoder() {
  Reset();
  TeardownCodec();
}

void VideoDecoder::SetHost(Host* host) {
  SB_DCHECK(host != NULL);
  SB_DCHECK(host_ == NULL);
  host_ = host;
}

void VideoDecoder::WriteInputBuffer(const InputBuffer& input_buffer) {
  SB_DCHECK(queue_.Poll().type == kInvalid);
  SB_DCHECK(host_ != NULL);

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }

  if (!SbThreadIsValid(decoder_thread_)) {
    decoder_thread_ =
        SbThreadCreate(0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
                       "video_decoder", &VideoDecoder::ThreadEntryPoint, this);
    SB_DCHECK(SbThreadIsValid(decoder_thread_));
  }

  queue_.Put(Event(input_buffer));
}

void VideoDecoder::WriteEndOfStream() {
  SB_DCHECK(host_ != NULL);

  // We have to flush the decoder to decode the rest frames and to ensure that
  // Decode() is not called when the stream is ended.
  stream_ended_ = true;
  queue_.Put(Event(kWriteEndOfStream));
}

void VideoDecoder::Reset() {
  // Join the thread to ensure that all callbacks in process are finished.
  if (SbThreadIsValid(decoder_thread_)) {
    queue_.Put(Event(kReset));
    SbThreadJoin(decoder_thread_, NULL);
  }

  decoder_thread_ = kSbThreadInvalid;
  stream_ended_ = false;
}

bool VideoDecoder::InitializeCodec() {
  video_window_ = GetVideoWindow();
  if (!video_window_) {
    SB_LOG(ERROR) << "Got null video window.";
    return false;
  }

  // Setup the output ANativeWindow object.  If we are in punch-out mode, target
  // the passed in Android video window.  If we are in decode-to-texture mode,
  // create a ANativeWindow from a new texture target and use that as the
  // output window.
  switch (output_mode_) {
    case kSbPlayerOutputModePunchOut: {
      output_window_ = video_window_;
    } break;
    case kSbPlayerOutputModeDecodeToTexture: {
      starboard::ScopedLock lock(decode_target_mutex_);
      // A width and height of (0, 0) is provided here because Android doesn't
      // actually allocate any memory into the texture at this time.  That is
      // done behind the scenes, the acquired texture is not actually backed
      // by texture data until updateTexImage() is called on it.
      decode_target_ = SbDecodeTargetAcquireFromProvider(
          decode_target_provider_, kSbDecodeTargetFormat1PlaneRGBA, 0, 0);
      if (!SbDecodeTargetIsValid(decode_target_)) {
        SB_LOG(ERROR) << "Could not acquire a decode target from provider.";
        return false;
      }
      output_window_ = decode_target_->data->native_window;
    } break;
    case kSbPlayerOutputModeInvalid: {
      SB_NOTREACHED();
    } break;
  }

  width_ = ANativeWindow_getWidth(video_window_);
  height_ = ANativeWindow_getHeight(video_window_);
  color_format_ = ANativeWindow_getFormat(video_window_);
  if (color_format_ == -1) {
    SB_LOG(ERROR) << "Failed to get color format of window.";
    return false;
  }

  const char* mime_type = GetMimeTypeFromCodecType(video_codec_);
  if (!mime_type) {
    SB_LOG(INFO) << "Unsupported video codec type: " << video_codec_;
    return false;
  }

  media_format_ = AMediaFormat_new();
  SB_DCHECK(media_format_);
  AMediaFormat_setString(media_format_, AMEDIAFORMAT_KEY_MIME, mime_type);
  AMediaFormat_setInt32(media_format_, AMEDIAFORMAT_KEY_WIDTH, width_);
  AMediaFormat_setInt32(media_format_, AMEDIAFORMAT_KEY_HEIGHT, height_);
  AMediaFormat_setInt32(media_format_, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                        color_format_);
  AMediaFormat_setInt32(media_format_,
                        AMEDIAFORMAT_KEY_PUSH_BLANK_BUFFERS_ON_STOP, 1);

  media_codec_ = AMediaCodec_createDecoderByType(mime_type);
  if (!media_codec_) {
    SB_LOG(ERROR) << "|AMediaCodec_createDecoderByType| failed.";
    return false;
  }

  media_status_t status =
      AMediaCodec_configure(media_codec_, media_format_, output_window_,
                            /*crypto=*/NULL, /*flags=*/0);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "|AMediaCodec_configure| failed with status: " << status;
    return false;
  }

  status = AMediaCodec_start(media_codec_);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "|AMediaCodec_start| failed with status: " << status;
    return false;
  }

  return true;
}

void VideoDecoder::TeardownCodec() {
  if (media_codec_) {
    AMediaCodec_delete(media_codec_);
    media_codec_ = NULL;
  }
  if (media_format_) {
    AMediaFormat_delete(media_format_);
    media_format_ = NULL;
  }

  starboard::ScopedLock lock(decode_target_mutex_);
  if (SbDecodeTargetIsValid(decode_target_)) {
    SbDecodeTargetReleaseToProvider(decode_target_provider_, decode_target_);
    decode_target_ = kSbDecodeTargetInvalid;
  }
}

// static
void* VideoDecoder::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  VideoDecoder* decoder = static_cast<VideoDecoder*>(context);
  decoder->DecoderThreadFunc();
  return NULL;
}

void VideoDecoder::DecoderThreadFunc() {
  for (;;) {
    Event event = queue_.Get();
    if (event.type == kReset) {
      SB_LOG(INFO) << "Reset event occurred.";
      media_status_t status = AMediaCodec_flush(media_codec_);
      if (status != AMEDIA_OK) {
        SB_LOG(ERROR) << "|AMediaCodec_flush| failed with status: " << status;
      }
      return;
    }
    if (error_occured_) {
      continue;
    }
    if (event.type == kWriteInputBuffer) {
      // Send |input_buffer| to |media_codec_| and try to decode one frame.
      DecodeInputBuffer(event.input_buffer);
      host_->OnDecoderStatusUpdate(kNeedMoreInput, NULL);
    } else if (event.type == kWriteEndOfStream) {
      ssize_t index =
          AMediaCodec_dequeueInputBuffer(media_codec_, kDequeueTimeout);
      media_status_t status = AMediaCodec_queueInputBuffer(
          media_codec_, index, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
      AMediaCodec_flush(media_codec_);
      host_->OnDecoderStatusUpdate(kBufferFull, VideoFrame::CreateEOSFrame());
    } else {
      SB_NOTREACHED();
    }
  }
}

bool VideoDecoder::DecodeInputBuffer(
    const InputBuffer& starboard_input_buffer) {
  const uint8_t* data = starboard_input_buffer.data();
  size_t input_size = starboard_input_buffer.size();
  ssize_t index = AMediaCodec_dequeueInputBuffer(media_codec_, kDequeueTimeout);
  if (index < 0) {
    SB_LOG(ERROR) << "|AMediaCodec_dequeueInputBuffer| failed with status: "
                  << index;
    return false;
  }
  size_t size = 0;
  uint8_t* input_buffer =
      AMediaCodec_getInputBuffer(media_codec_, index, &size);
  SB_DCHECK(input_buffer);
  if (input_size >= size) {
    SB_LOG(ERROR) << "Media codec input buffer too small to write to.";
    return false;
  }
  SbMemoryCopy(input_buffer, data, input_size);
  int64_t pts_us = ConvertToMicroseconds(starboard_input_buffer.pts());
  media_status_t status = AMediaCodec_queueInputBuffer(media_codec_, index, 0,
                                                       input_size, pts_us, 0);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "|AMediaCodec_queueInputBuffer| failed with status: "
                  << status;
    return false;
  }

  AMediaCodecBufferInfo info;
  ssize_t out_index =
      AMediaCodec_dequeueOutputBuffer(media_codec_, &info, kDequeueTimeout);
  if (out_index < 0) {
    SB_LOG(ERROR) << "|AMediaCodec_dequeueOutputBuffer| failed with status: "
                  << out_index;
    return false;
  }

  const SbTime delay =
      (current_time_ == -1) ? 0 : info.presentationTimeUs -
                                      ConvertToMicroseconds(current_time_);
  const bool frame_too_early = delay < kFrameTooEarlyThreshold;
  if (delay > 0) {
    SbThreadSleep(delay);
  } else if (frame_too_early) {
    SB_LOG(INFO) << "Frame too early.";
  }

  status = AMediaCodec_releaseOutputBuffer(media_codec_, out_index,
                                           !frame_too_early && info.size > 0);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "|AMediaCodec_releaseOutputBuffer| failed with status: "
                  << status << ", at index:" << out_index;
    return false;
  }

  // Record the latest width/height of the decoded input.
  AMediaFormat* format = AMediaCodec_getOutputFormat(media_codec_);
  AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &frame_width_);
  AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &frame_height_);

  const SbMediaTime out_pts = ConvertToMediaTime(info.presentationTimeUs);
  host_->OnDecoderStatusUpdate(kBufferFull,
                               VideoFrame::CreateEmptyFrame(out_pts));
  return true;
}

namespace {
void updateTexImage(jobject surface_texture) {
  JniEnvExt::Get()->CallVoidMethod(surface_texture, "updateTexImage", "()V");
}

void getTransformMatrix(jobject surface_texture, float* matrix4x4) {
  JniEnvExt* env = JniEnvExt::Get();

  jfloatArray java_array = env->NewFloatArray(16);
  SB_DCHECK(java_array);

  env->CallVoidMethod(surface_texture, "getTransformMatrix", "([F)V",
                      java_array);

  jfloat* array_values = env->GetFloatArrayElements(java_array, 0);
  SbMemoryCopy(matrix4x4, array_values, sizeof(float) * 16);

  env->DeleteLocalRef(java_array);
}

// Rounds the float to the nearest integer, and also does a DCHECK to make sure
// that the input float was already near an integer value.
int RoundToNearInteger(float x) {
  int rounded = static_cast<int>(x + 0.5f);
  SB_DCHECK(std::abs(static_cast<float>(x - rounded)) < 0.01f);
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
  // We must take a lock here since this function can be called from a
  // separate thread.
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
VideoDecoder* VideoDecoder::Create(const Parameters& parameters) {
  ::starboard::android::shared::VideoDecoder* decoder =
      new ::starboard::android::shared::VideoDecoder(
          parameters.video_codec
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
          ,
          parameters.output_mode, parameters.decode_target_provider
#endif        // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
          );  // NOLINT(whitespace/parens)
  if (!decoder->is_valid()) {
    delete decoder;
    return NULL;
  }
  return decoder;
}

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
