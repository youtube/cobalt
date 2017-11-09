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
#include "starboard/android/shared/decode_target_create.h"
#include "starboard/android/shared/decode_target_internal.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/video_renderer.h"
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

const jlong kDequeueTimeout = 0;

const jint kNoOffset = 0;
const jlong kNoPts = 0;
const jint kNoSize = 0;
const jint kNoBufferFlags = 0;

// Convenience HDR mastering metadata.
const SbMediaMasteringMetadata kEmptyMasteringMetadata = {};

// TODO: Merge this once Android decoders are unified
const char* GetNameForMediaCodecStatus(jint status) {
  switch (status) {
    case MEDIA_CODEC_OK:
      return "MEDIA_CODEC_OK";
    case MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
      return "MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER";
    case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
      return "MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER";
    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
      return "MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED";
    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
      return "MEDIA_CODEC_OUTPUT_FORMAT_CHANGED";
    case MEDIA_CODEC_INPUT_END_OF_STREAM:
      return "MEDIA_CODEC_INPUT_END_OF_STREAM";
    case MEDIA_CODEC_OUTPUT_END_OF_STREAM:
      return "MEDIA_CODEC_OUTPUT_END_OF_STREAM";
    case MEDIA_CODEC_NO_KEY:
      return "MEDIA_CODEC_NO_KEY";
    case MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION:
      return "MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION";
    case MEDIA_CODEC_ABORT:
      return "MEDIA_CODEC_ABORT";
    case MEDIA_CODEC_ERROR:
      return "MEDIA_CODEC_ERROR";
    default:
      SB_NOTREACHED();
      return "MEDIA_CODEC_ERROR_UNKNOWN";
  }
}

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

// TODO: Re-order ctor initialization list according to the declaration order.
VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec,
                           SbDrmSystem drm_system,
                           SbPlayerOutputMode output_mode,
                           SbDecodeTargetGraphicsContextProvider*
                               decode_target_graphics_context_provider)
    : video_codec_(video_codec),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      host_(NULL),
      stream_ended_(false),
      decoder_thread_(kSbThreadInvalid),
      media_codec_bridge_(NULL),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      decode_target_(kSbDecodeTargetInvalid),
      frame_width_(0),
      frame_height_(0),
      has_written_buffer_since_reset_(false) {
  if (!InitializeCodec()) {
    SB_LOG(ERROR) << "Failed to initialize video decoder.";
    TeardownCodec();
  }
}

VideoDecoder::~VideoDecoder() {
  JoinOnDecoderThread();
  TeardownCodec();
  ClearVideoWindow();
}

void VideoDecoder::Initialize(const Closure& error_cb) {
  SB_DCHECK(!error_cb_.is_valid());
  SB_DCHECK(error_cb.is_valid());
  error_cb_ = error_cb;
}

void VideoDecoder::SetHost(VideoRenderer* host) {
  SB_DCHECK(host != NULL);
  SB_DCHECK(host_ == NULL);
  host_ = host;
}

void VideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(input_buffer);
  SB_DCHECK(host_ != NULL);

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputBuffer() was called after WriteEndOfStream().";
    return;
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

  if (!SbThreadIsValid(decoder_thread_)) {
    decoder_thread_ =
        SbThreadCreate(0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
                       "video_decoder", &VideoDecoder::ThreadEntryPoint, this);
    SB_DCHECK(SbThreadIsValid(decoder_thread_));
  }

  event_queue_.PushBack(Event(input_buffer));
}

void VideoDecoder::WriteEndOfStream() {
  SB_DCHECK(host_ != NULL);
  stream_ended_ = true;
  event_queue_.PushBack(Event(Event::kWriteEndOfStream));
}

void VideoDecoder::Reset() {
  JoinOnDecoderThread();
  TeardownCodec();
  if (!InitializeCodec()) {
    // TODO: Communicate this failure to our clients somehow.
    SB_LOG(ERROR) << "Failed to initialize codec after reset.";
  }

  stream_ended_ = false;
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
  media_codec_bridge_ = MediaCodecBridge::CreateVideoMediaCodecBridge(
      video_codec_, width, height, j_output_surface, j_media_crypto,
      previous_color_metadata_ ? &*previous_color_metadata_ : nullptr);
  if (!media_codec_bridge_) {
    SB_LOG(ERROR) << "Failed to create video media codec bridge.";
    return false;
  }

  return true;
}

void VideoDecoder::TeardownCodec() {
  media_codec_bridge_.reset();

  starboard::ScopedLock lock(decode_target_mutex_);
  if (SbDecodeTargetIsValid(decode_target_)) {
    SbDecodeTargetReleaseInGlesContext(decode_target_graphics_context_provider_,
                                       decode_target_);
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
  // A second staging queue that holds events of type |kWriteInputBuffer| and
  // |kWriteEndOfStream| only, and is only accessed from the decoder thread,
  // so it does not need to be thread-safe.
  std::deque<Event> pending_work;

  // We will switch off each iteration between attempting to enqueue an input
  // buffer and then handling owned output buffers, and attempting to dequeue
  // and output buffer and then handling owned output buffers.
  enum MediaCodecWorkType {
    kAttemptHandleInputBuffer,
    kAttemptHandleOutputBuffer,
  };
  MediaCodecWorkType work_type = kAttemptHandleInputBuffer;

  for (;;) {
    // TODO: Replace |event_queue_| with a plain locked std::queue and only call
    // swap() when |pending_work| is empty to avoid unnecessary locks.
    Event event = event_queue_.PollFront();

    if (event.type == Event::kWriteInputBuffer ||
        event.type == Event::kWriteEndOfStream) {
      pending_work.push_back(event);
    }

    if (event.type == Event::kReset) {
      SB_LOG(INFO) << "Reset event occurred.";
      // |media_codec_bridge_->Flush| will reclaim the actual output buffers,
      // so we just need to forget that we have these handles.
      dequeue_output_results_.clear();
      jint status = media_codec_bridge_->Flush();
      if (status != MEDIA_CODEC_OK) {
        SB_LOG(ERROR) << "Failed to flush video media codec.";
      }
      return;
    }

    bool did_work = false;
    if (work_type == kAttemptHandleInputBuffer) {
      did_work |= ProcessOneInputBuffer(&pending_work);
      work_type = kAttemptHandleOutputBuffer;
    } else if (work_type == kAttemptHandleOutputBuffer) {
      did_work |= DequeueAndProcessOutputBuffer();
      work_type = kAttemptHandleInputBuffer;
    } else {
      SB_NOTREACHED();
    }

    // Pass all of our decoded frames into our host, which will decide whether
    // to drop them, render them, or hold them.  If our host does anything
    // with at least one frame, then we consider that work, and will do
    // another loop iteration immediately.
    size_t previous_size = dequeue_output_results_.size();
    host_->HandleDecodedFrames(media_codec_bridge_.get(),
                               &dequeue_output_results_);
    did_work |= (dequeue_output_results_.size() != previous_size);

    if (event.type == Event::kInvalid && !did_work) {
      SbThreadSleep(kSbTimeMillisecond);
      continue;
    }
  }
}

void VideoDecoder::JoinOnDecoderThread() {
  if (SbThreadIsValid(decoder_thread_)) {
    event_queue_.Clear();
    event_queue_.PushBack(Event(Event::kReset));
    SbThreadJoin(decoder_thread_, NULL);
    event_queue_.Clear();
  }
  decoder_thread_ = kSbThreadInvalid;
}

bool VideoDecoder::ProcessOneInputBuffer(std::deque<Event>* pending_work) {
  SB_DCHECK(pending_work);
  if (pending_work->empty()) {
    return false;
  }

  SB_CHECK(media_codec_bridge_);

  // During secure playback, and only secure playback, is is possible that our
  // attempt to enqueue an input buffer will be rejected by MediaCodec because
  // we do not have a key yet.  In this case, we hold on to the input buffer
  // that we have already set up, and repeatedly attempt to enqueue it until
  // it works.  Ideally, we would just wait until MediaDrm was ready, however
  // the shared starboard player framework assumes that it is possible to
  // perform decryption and decoding as separate steps, so from its
  // perspective, having made it to this point implies that we ready to
  // decode.  It is not possible to do them as separate steps on Android. From
  // the perspective of user application, decryption and decoding are one
  // atomic step.
  DequeueInputResult dequeue_input_result;
  Event event;
  bool input_buffer_already_written = false;
  if (pending_queue_input_buffer_task_) {
    dequeue_input_result =
        pending_queue_input_buffer_task_->dequeue_input_result;
    SB_DCHECK(dequeue_input_result.index >= 0);
    event = pending_queue_input_buffer_task_->event;
    pending_queue_input_buffer_task_ = nullopt_t();
    input_buffer_already_written = true;
  } else {
    dequeue_input_result =
        media_codec_bridge_->DequeueInputBuffer(kDequeueTimeout);
    event = pending_work->front();
    if (dequeue_input_result.index < 0) {
      HandleError("dequeueInputBuffer", dequeue_input_result.status);
      return false;
    }
    pending_work->pop_front();
  }

  SB_DCHECK(event.type == Event::kWriteInputBuffer ||
            event.type == Event::kWriteEndOfStream);

  scoped_refptr<InputBuffer> input_buffer = event.input_buffer;
  if (event.type == Event::kWriteEndOfStream) {
    SB_DCHECK(pending_work->empty());
  }
  const void* data = NULL;
  int size = 0;
  if (event.type == Event::kWriteInputBuffer) {
    data = input_buffer->data();
    size = input_buffer->size();
  } else if (event.type == Event::kWriteEndOfStream) {
    data = NULL;
    size = 0;
  }

  // Don't bother rewriting the same data if we already did it last time we
  // were called and had it stored in |pending_queue_input_buffer_task_|.
  if (!input_buffer_already_written && event.type != Event::kWriteEndOfStream) {
    ScopedJavaByteBuffer byte_buffer(
        media_codec_bridge_->GetInputBuffer(dequeue_input_result.index));
    if (byte_buffer.IsNull() || byte_buffer.capacity() < size) {
      SB_LOG(ERROR) << "Unable to write to MediaCodec input buffer.";
      return false;
    }
    byte_buffer.CopyInto(data, size);
  }

  jint status;
  if (event.type == Event::kWriteInputBuffer) {
    jlong pts_us = ConvertSbMediaTimeToMicroseconds(input_buffer->pts());
    if (drm_system_ && input_buffer->drm_info()) {
      status = media_codec_bridge_->QueueSecureInputBuffer(
          dequeue_input_result.index, kNoOffset, *input_buffer->drm_info(),
          pts_us);
    } else {
      status = media_codec_bridge_->QueueInputBuffer(
          dequeue_input_result.index, kNoOffset, size, pts_us, kNoBufferFlags);
    }
  } else {
    status = media_codec_bridge_->QueueInputBuffer(dequeue_input_result.index,
                                                   kNoOffset, 0, kNoPts,
                                                   BUFFER_FLAG_END_OF_STREAM);
  }

  if (status != MEDIA_CODEC_OK) {
    HandleError("queue(Secure)?InputBuffer", status);
    // TODO: Stop the decoding loop on fatal error.
    SB_DCHECK(!pending_queue_input_buffer_task_);
    pending_queue_input_buffer_task_ = {dequeue_input_result, event};
    return false;
  }

  return true;
}

bool VideoDecoder::DequeueAndProcessOutputBuffer() {
  SB_CHECK(media_codec_bridge_);

  DequeueOutputResult dequeue_output_result =
      media_codec_bridge_->DequeueOutputBuffer(kDequeueTimeout);

  // Note that if the |index| field of |DequeueOutputResult| is negative, then
  // all fields other than |status| and |index| are invalid.  This is
  // especially important, as the Java side of |MediaCodecBridge| will reuse
  // objects for returned results behind the scenes.
  if (dequeue_output_result.index < 0) {
    if (dequeue_output_result.status == MEDIA_CODEC_OUTPUT_FORMAT_CHANGED) {
      RefreshOutputFormat();
      return true;
    }

    if (dequeue_output_result.status == MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED) {
      SB_DLOG(INFO) << "Output buffers changed, trying to dequeue again.";
      return true;
    }

    HandleError("dequeueOutputBuffer", dequeue_output_result.status);
    return false;
  }

  ProcessOutputBuffer(dequeue_output_result);
  return true;
}

void VideoDecoder::ProcessOutputBuffer(
    const DequeueOutputResult& dequeue_output_result) {
  SB_DCHECK(dequeue_output_result.index >= 0);
  dequeue_output_results_.push_back(dequeue_output_result);
}

void VideoDecoder::RefreshOutputFormat() {
  SB_DCHECK(media_codec_bridge_);
  SB_DLOG(INFO) << "Output format changed, trying to dequeue again.";
  // Record the latest width/height of the decoded input.
  SurfaceDimensions output_dimensions =
      media_codec_bridge_->GetOutputDimensions();
  frame_width_ = output_dimensions.width;
  frame_height_ = output_dimensions.height;
}

void VideoDecoder::HandleError(const char* action_name, jint status) {
  SB_DCHECK(status != MEDIA_CODEC_OK);

  bool retry = false;
  if (status == MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER) {
    // Don't bother logging a try again later status, it happens a lot.
    return;
  } else if (status == MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER) {
    // Don't bother logging a try again later status, it happens a lot.
    return;
  } else if (status == MEDIA_CODEC_NO_KEY) {
    retry = true;
  } else if (status == MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION) {
    drm_system_->OnInsufficientOutputProtection();
  } else {
    error_cb_.Run();
  }

  if (retry) {
    SB_LOG(INFO) << "|" << action_name << "| failed with status: "
                 << GetNameForMediaCodecStatus(status)
                 << ", will try again after a delay.";
  } else {
    SB_LOG(ERROR) << "|" << action_name << "| failed with status: "
                  << GetNameForMediaCodecStatus(status) << ".";
  }
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
