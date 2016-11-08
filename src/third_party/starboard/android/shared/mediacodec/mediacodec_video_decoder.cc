// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "third_party/starboard/android/shared/mediacodec/mediacodec_video_decoder.h"
#include "starboard/memory.h"

#include "third_party/starboard/android/shared/application_android.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace mediacodec {

SbTime renderstart = -1;

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec)
    : video_codec_(video_codec),
      host_(NULL),
      stream_ended_(false),
      error_occured_(false),
      decoder_thread_(kSbThreadInvalid) {

  renderstart = -1;

  SbWindow window = ::starboard::android::shared::ApplicationAndroid::Get()->GetWindow();
  SbWindowSize size;
  SbWindowGetSize(window, &size); // Assume full-screen, i.e. video width/height = window width/height

  vwindow_ = ::starboard::android::shared::ApplicationAndroid::Get()->GetVideoWindowHandle();
  char type[20];
  if(!GetMimeTypeFromCodecType(type, video_codec)) {
    LOGI("mediacodec: video: Unsupported video codec type: %d\n", video_codec);
    return;
  }

  mediacodec_ = AMediaCodec_createDecoderByType(type);
  if(mediacodec_ == NULL) {
    LOGI("mediacodec: video: Failed to create for MIME type: %s\n", type);
    return;
  }
  mediaformat_ = AMediaFormat_new();
  //TODO: Don't hadcode MIME type or dimensions
  AMediaFormat_setString(mediaformat_, AMEDIAFORMAT_KEY_MIME, type);
  AMediaFormat_setInt32(mediaformat_, AMEDIAFORMAT_KEY_WIDTH, size.width);
  AMediaFormat_setInt32(mediaformat_, AMEDIAFORMAT_KEY_HEIGHT, size.height);
  AMediaFormat_setInt32(mediaformat_, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7f00a000); /* COLOR_Format32bitABGR8888 */
  AMediaFormat_setInt32(mediaformat_, AMEDIAFORMAT_KEY_PUSH_BLANK_BUFFERS_ON_STOP, 1);
  media_status_t status = AMediaCodec_configure(mediacodec_, mediaformat_, vwindow_, NULL, 0);
  if(status != 0) {
    const char *format_string = AMediaFormat_toString(mediaformat_);
    LOGI("mediacodec: video: Failed to configure for format: %s; status = %d\n", format_string, status);
    return;
  }
  status = AMediaCodec_start(mediacodec_);
  if(status != 0) {
    LOGI("mediacodec: video: Failed to start; status = %d\n", status);
    return;
  }
  LOGI("mediacodec: video: Initialized\n");
}

VideoDecoder::~VideoDecoder() {
  Reset();
  TeardownCodec();
}

bool VideoDecoder::GetMimeTypeFromCodecType(char *out, SbMediaVideoCodec type) {
    char tmp[20];
    tmp[0] = '\0';
    switch(type) {
      case kSbMediaVideoCodecNone:
        return false;
      case kSbMediaVideoCodecH264:
        SbStringConcat(tmp, "video/avc", 20);
        break;
      case kSbMediaVideoCodecH265:
        SbStringConcat(tmp, "video/hevc", 20);
        break;
      case kSbMediaVideoCodecMpeg2:
        return false;
      case kSbMediaVideoCodecTheora:
        return false;
      case kSbMediaVideoCodecVc1:
        return false;
      case kSbMediaVideoCodecVp10:
        return false;
      case kSbMediaVideoCodecVp8:
        SbStringConcat(tmp, "video/x-vnd.on2.vp8", 20);
        break;
      case kSbMediaVideoCodecVp9:
        SbStringConcat(tmp, "video/x-vnd.on2.vp9", 20);
        break;
    }
    SbStringCopy(out, tmp, 20);
    return true;
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
                       "mc_video_dec", &VideoDecoder::ThreadEntryPoint, this);
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

void VideoDecoder::TeardownCodec() {
    renderstart = -1;
    LOGI("mediacodec: video: Teardown\n");
    if(mediacodec_ != NULL) {
	AMediaCodec_delete(mediacodec_);
    }
    if(mediaformat_ != NULL) {
        AMediaFormat_delete(mediaformat_);
    }
}

// static
void* VideoDecoder::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  VideoDecoder* decoder = reinterpret_cast<VideoDecoder*>(context);
  decoder->DecoderThreadFunc();
  return NULL;
}

void VideoDecoder::DecoderThreadFunc() {
  for (;;) {
    Event event = queue_.Get();
    if (event.type == kReset) {
      renderstart = -1;
      AMediaCodec_flush(mediacodec_);
      LOGI("mediacodec: video: Reset event occurred\n");
      return;
    }
    if (error_occured_) {
      LOGI("mediacodec: video: error occured\n");
      continue;
    }
    if (event.type == kWriteInputBuffer) {
      if(!InputBufferWork(event)) {
        LOGI("mediacodec: video: Dropping frame\n");
      }

      // TODO: Should this always be input pts?
      SbMediaTime output_pts = OutputBufferWork();
      if(output_pts == -1) {
        output_pts = event.input_buffer.pts();
      }

      /* WAR: Send empty frames to host renderer even though they will not be used */
      VideoFrame frame = VideoFrame::CreateEmptyFrame(output_pts);
      host_->OnDecoderStatusUpdate(kBufferFull, &frame);
      host_->OnDecoderStatusUpdate(kNeedMoreInput, NULL);
    } else {
      SB_DCHECK(event.type == kWriteEndOfStream);
      LOGI("mediacodec: video: received EOS event\n");
      //TODO: queue buffer with EOS flag, stop mediacodec_
      // Stream has ended, flush mediacodec
      InputBufferWork(event);
      while(OutputBufferWork() != -1) {
        LOGI("mediacodec: video: Saw EOS; releasing remaining output buffers\n");
      }
      AMediaCodec_flush(mediacodec_);
      // TODO: include stop here?
      VideoFrame frame = VideoFrame::CreateEOSFrame();
      host_->OnDecoderStatusUpdate(kBufferFull, &frame);
    }
  }
}

bool VideoDecoder::InputBufferWork(Event event) {
  if(event.type == kWriteEndOfStream) {
     ssize_t index = AMediaCodec_dequeueInputBuffer(mediacodec_, kSbTimeSecond);
     if(index < 0) {
       LOGI("mediacodec: video: EOS: bad input buffer index: %d\n", index);
       return false;
     }
     media_status_t status = AMediaCodec_queueInputBuffer(mediacodec_, index, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
     if(status < 0) {
       LOGI("mediacodec: video: failed to queue input buffer; status = %d\n", status);
       return false;
     }
     return true;
   }

  const uint8_t* data = event.input_buffer.data();
  int input_size = event.input_buffer.size();

  if(data == NULL) {
    LOGI("mediacodec: video: input buffer data NULL\n");
    return false;
  }

  /* Dequeue and fill input buffer */
  ssize_t index = AMediaCodec_dequeueInputBuffer(mediacodec_, 2000);
  if(index < 0) {
    LOGI("mediacodec: video: bad input buffer index: %d\n", index);
    return false;
  }
  size_t size;
  uint8_t* input_buffer = AMediaCodec_getInputBuffer(mediacodec_, index, &size);
  if(input_buffer == NULL) {
    LOGI("mediacodec: video: input buffer NULL\n");
    return false;
  }
  size_t write_size = 0;
  if(size > (size_t)input_size) {
    write_size = (size_t)input_size;
  } else {
    write_size = size;
  }
  SbMemoryCopy(input_buffer, data, write_size);

  /* Queue input buffer */
  int64_t pts_us = (event.input_buffer.pts() * 1000000) / kSbMediaTimeSecond;
  media_status_t status = AMediaCodec_queueInputBuffer(mediacodec_, index, 0, write_size, pts_us, 0);
  if(status < 0) {
    LOGI("mediacodec: video: failed to queue input buffer; status = %d\n", status);
    return false;
  }

  if((size_t)input_size > size) {
    LOGI("mediacodec: video: input data size = %lx > input buffer size = %lx\n", (size_t)input_size, size);
  }

  return true;
}

SbMediaTime VideoDecoder::OutputBufferWork() {
  /* Dequeue and release/render output buffer */
  AMediaCodecBufferInfo info;
  ssize_t out_index = AMediaCodec_dequeueOutputBuffer(mediacodec_, &info, 2000);
  if(out_index < 0) {
    LOGI("mediacodec: video: bad output buffer index: %d\n", out_index);
    return -1;
  }

  SbTime ptsMicro = info.presentationTimeUs;
  if(renderstart < 0) {
      renderstart = SbTimeGetNow() - ptsMicro;
  }
  SbTime delay = renderstart + ptsMicro - SbTimeGetNow();
  if (delay > 0) {
      SbThreadSleep(delay);
  }

  if(info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM != 0) {
    LOGI("mediacodec: video: saw output EOS\n");
  }

  media_status_t status = AMediaCodec_releaseOutputBuffer(mediacodec_, out_index, info.size > 0);
  if(status != 0) {
    LOGI("mediacodec: video: error releasing output buffer; status = %d\n", status);
    return -1;
  }

  return (ptsMicro * kSbMediaTimeSecond) / 1000000;
}

}  // namespace mediacodec

namespace starboard {
namespace player {
namespace filter {

// static
VideoDecoder* VideoDecoder::Create(SbMediaVideoCodec video_codec) {
  mediacodec::VideoDecoder* decoder = new mediacodec::VideoDecoder(video_codec);
  if (decoder->is_valid()) {
    return decoder;
  }
  delete decoder;
  return NULL;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard

}  // namespace shared
}  // namespace starboard
