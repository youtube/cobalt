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

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/android/shared/window_internal.h"
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

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec)
    : video_codec_(video_codec),
      host_(NULL),
      stream_ended_(false),
      error_occured_(false),
      decoder_thread_(kSbThreadInvalid),
      media_format_(NULL),
      media_codec_(NULL),
      width_(-1),
      height_(-1),
      color_format_(-1) {
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
      AMediaCodec_configure(media_codec_, media_format_, video_window_,
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

  const SbMediaTime out_pts = ConvertToMediaTime(info.presentationTimeUs);
  host_->OnDecoderStatusUpdate(kBufferFull,
                               VideoFrame::CreateEmptyFrame(out_pts));
  return true;
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
VideoDecoder* VideoDecoder::Create(SbMediaVideoCodec video_codec) {
  ::starboard::android::shared::VideoDecoder* decoder =
      new ::starboard::android::shared::VideoDecoder(video_codec);
  if (!decoder->is_valid()) {
    delete decoder;
    return NULL;
  }
  return decoder;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
