// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/tizen/shared/ffmpeg/ffmpeg_video_decoder.h"

#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {

// FFmpeg requires its decoding buffers to align with platform alignment.  It
// mentions inside
// http://ffmpeg.org/doxygen/trunk/structAVFrame.html#aa52bfc6605f6a3059a0c3226cc0f6567
// that the alignment on most modern desktop systems are 16 or 32.
static const int kAlignment = 32;

size_t AlignUp(size_t size, int alignment) {
  SB_DCHECK((alignment & (alignment - 1)) == 0);
  return (size + alignment - 1) & ~(alignment - 1);
}

size_t GetYV12SizeInBytes(int32_t width, int32_t height) {
  return width * height * 3 / 2;
}

int AllocateBuffer(AVCodecContext* codec_context, AVFrame* frame) {
  if (codec_context->pix_fmt != PIX_FMT_YUV420P &&
      codec_context->pix_fmt != PIX_FMT_YUVJ420P) {
    SB_DLOG(WARNING) << "Unsupported pix_fmt " << codec_context->pix_fmt;
    return AVERROR(EINVAL);
  }

  int ret =
      av_image_check_size(codec_context->width, codec_context->height, 0, NULL);
  if (ret < 0) {
    return ret;
  }

  // Align to kAlignment * 2 as we will divide y_stride by 2 for u and v planes
  size_t y_stride = AlignUp(codec_context->width, kAlignment * 2);
  size_t uv_stride = y_stride / 2;
  size_t aligned_height = AlignUp(codec_context->height, kAlignment * 2);
  uint8_t* frame_buffer = reinterpret_cast<uint8_t*>(SbMemoryAllocateAligned(
      kAlignment, GetYV12SizeInBytes(y_stride, aligned_height)));

  // y plane
  frame->base[0] = frame_buffer;
  frame->data[0] = frame->base[0];
  frame->linesize[0] = y_stride;
  // u plane
  frame->base[1] = frame_buffer + y_stride * aligned_height;
  frame->data[1] = frame->base[1];
  frame->linesize[1] = uv_stride;
  // v plane
  frame->base[2] = frame->base[1] + uv_stride * aligned_height / 2;
  frame->data[2] = frame->base[2];
  frame->linesize[2] = uv_stride;

  frame->opaque = frame_buffer;
  frame->type = FF_BUFFER_TYPE_USER;
  frame->pkt_pts =
      codec_context->pkt ? codec_context->pkt->pts : AV_NOPTS_VALUE;
  frame->width = codec_context->width;
  frame->height = codec_context->height;
  frame->format = codec_context->pix_fmt;

  frame->reordered_opaque = codec_context->reordered_opaque;

  return 0;
}

void ReleaseBuffer(AVCodecContext*, AVFrame* frame) {
  SbMemoryDeallocate(frame->opaque);
  frame->opaque = NULL;

  // The FFmpeg API expects us to zero the data pointers in this callback.
  SbMemorySet(frame->data, 0, sizeof(frame->data));
}

}  // namespace

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec)
    : video_codec_(video_codec),
      host_(NULL),
      codec_context_(NULL),
      av_frame_(NULL),
      stream_ended_(false),
      error_occured_(false),
      decoder_thread_(kSbThreadInvalid) {
  InitializeCodec();
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
                       "ff_video_dec", &VideoDecoder::ThreadEntryPoint, this);
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

  if (codec_context_ != NULL) {
    avcodec_flush_buffers(codec_context_);
  }

  decoder_thread_ = kSbThreadInvalid;
  stream_ended_ = false;
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
      return;
    }
    if (error_occured_) {
      continue;
    }
    if (event.type == kWriteInputBuffer) {
      // Send |input_buffer| to ffmpeg and try to decode one frame.
      AVPacket packet;
      av_init_packet(&packet);
      packet.data = const_cast<uint8_t*>(event.input_buffer.data());
      packet.size = event.input_buffer.size();
      packet.pts = event.input_buffer.pts();
      codec_context_->reordered_opaque = packet.pts;

      DecodePacket(&packet);
      host_->OnDecoderStatusUpdate(kNeedMoreInput, NULL);
    } else {
      SB_DCHECK(event.type == kWriteEndOfStream);
      // Stream has ended, try to decode any frames left in ffmpeg.
      AVPacket packet;
      do {
        av_init_packet(&packet);
        packet.data = NULL;
        packet.size = 0;
        packet.pts = 0;
      } while (DecodePacket(&packet));

      host_->OnDecoderStatusUpdate(kBufferFull, VideoFrame::CreateEOSFrame());
    }
  }
}

bool VideoDecoder::DecodePacket(AVPacket* packet) {
  SB_DCHECK(packet != NULL);

  avcodec_get_frame_defaults(av_frame_);
  int frame_decoded = 0;
  int result =
      avcodec_decode_video2(codec_context_, av_frame_, &frame_decoded, packet);
  if (frame_decoded == 0) {
    return false;
  }

  if (av_frame_->opaque == NULL) {
    SB_DLOG(ERROR) << "Video frame was produced yet has invalid frame data.";
    host_->OnDecoderStatusUpdate(kFatalError, NULL);
    error_occured_ = true;
    return false;
  }

  int pitch = AlignUp(av_frame_->width, kAlignment * 2);

  scoped_refptr<VideoFrame> frame = VideoFrame::CreateYV12Frame(
      av_frame_->width, av_frame_->height, pitch,
      codec_context_->reordered_opaque, av_frame_->data[0], av_frame_->data[1],
      av_frame_->data[2]);
  host_->OnDecoderStatusUpdate(kBufferFull, frame);
  return true;
}

void VideoDecoder::InitializeCodec() {
  InitializeFfmpeg();

  codec_context_ = avcodec_alloc_context3(NULL);

  if (codec_context_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    return;
  }

  codec_context_->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_context_->codec_id = AV_CODEC_ID_H264;
  codec_context_->profile = FF_PROFILE_UNKNOWN;
  codec_context_->coded_width = 0;
  codec_context_->coded_height = 0;
  codec_context_->pix_fmt = PIX_FMT_NONE;

  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->thread_count = 2;
  codec_context_->opaque = this;
  codec_context_->flags |= CODEC_FLAG_EMU_EDGE;
  codec_context_->get_buffer = AllocateBuffer;
  codec_context_->release_buffer = ReleaseBuffer;

  codec_context_->extradata = NULL;
  codec_context_->extradata_size = 0;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);

  if (codec == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    TeardownCodec();
    return;
  }

  int rv = OpenCodec(codec_context_, codec);
  if (rv < 0) {
    SB_LOG(ERROR) << "Unable to open codec";
    TeardownCodec();
    return;
  }

  av_frame_ = avcodec_alloc_frame();
  if (av_frame_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate audio frame";
    TeardownCodec();
  }
}

void VideoDecoder::TeardownCodec() {
  if (codec_context_) {
    CloseCodec(codec_context_);
    av_free(codec_context_);
    codec_context_ = NULL;
  }
  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
}

}  // namespace ffmpeg

namespace starboard {
namespace player {
namespace filter {

#if SB_API_VERSION >= 4
// static
bool VideoDecoder::OutputModeSupported(SbPlayerOutputMode output_mode,
                                       SbMediaVideoCodec codec,
                                       SbDrmSystem drm_system) {
  SB_UNREFERENCED_PARAMETER(codec);
  SB_UNREFERENCED_PARAMETER(drm_system);

  return output_mode == kSbPlayerOutputModePunchOut;
}
#endif  // SB_API_VERSION >= 4

}  // namespace filter
}  // namespace player
}  // namespace starboard

}  // namespace shared
}  // namespace starboard
