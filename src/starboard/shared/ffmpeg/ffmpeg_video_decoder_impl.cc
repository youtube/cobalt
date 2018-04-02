// Copyright 2018 Google Inc. All Rights Reserved.
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

// This file contains the explicit specialization of the VideoDecoderImpl class
// for the value 'FFMPEG'.

#include "starboard/shared/ffmpeg/ffmpeg_video_decoder_impl.h"

#include "starboard/linux/shared/decode_target_internal.h"
#include "starboard/memory.h"
#include "starboard/thread.h"

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

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)

void ReleaseBuffer(void* opaque, uint8_t* data) {
  SbMemoryDeallocate(data);
}

int AllocateBufferCallback(AVCodecContext* codec_context,
                           AVFrame* frame,
                           int flags) {
  VideoDecoderImpl<FFMPEG>* video_decoder =
      static_cast<VideoDecoderImpl<FFMPEG>*>(codec_context->opaque);
  return video_decoder->AllocateBuffer(codec_context, frame, flags);
}

#else   // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)

int AllocateBufferCallback(AVCodecContext* codec_context, AVFrame* frame) {
  VideoDecoderImpl<FFMPEG>* video_decoder =
      static_cast<VideoDecoderImpl<FFMPEG>*>(codec_context->opaque);
  return video_decoder->AllocateBuffer(codec_context, frame);
}

void ReleaseBuffer(AVCodecContext*, AVFrame* frame) {
  SbMemoryDeallocate(frame->opaque);
  frame->opaque = NULL;

  // The FFmpeg API expects us to zero the data pointers in this callback.
  SbMemorySet(frame->data, 0, sizeof(frame->data));
}
#endif  // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)

const bool g_registered =
    FFMPEGDispatch::RegisterSpecialization(FFMPEG,
                                           LIBAVCODEC_VERSION_MAJOR,
                                           LIBAVFORMAT_VERSION_MAJOR,
                                           LIBAVUTIL_VERSION_MAJOR);

}  // namespace

VideoDecoderImpl<FFMPEG>::VideoDecoderImpl(
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider)
    : video_codec_(video_codec),
      codec_context_(NULL),
      av_frame_(NULL),
      stream_ended_(false),
      error_occured_(false),
      decoder_thread_(kSbThreadInvalid),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      decode_target_(kSbDecodeTargetInvalid) {
  SB_DCHECK(g_registered) << "Decoder Specialization registration failed.";
  ffmpeg_ = FFMPEGDispatch::GetInstance();
  SB_DCHECK(ffmpeg_);
  if ((ffmpeg_->specialization_version()) == FFMPEG) {
    InitializeCodec();
  }
}

VideoDecoderImpl<FFMPEG>::~VideoDecoderImpl() {
  Reset();
  TeardownCodec();
}

// static
VideoDecoder* VideoDecoderImpl<FFMPEG>::Create(
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider) {
  return new VideoDecoderImpl<FFMPEG>(video_codec, output_mode,
                                      decode_target_graphics_context_provider);
}

void VideoDecoderImpl<FFMPEG>::Initialize(
    const DecoderStatusCB& decoder_status_cb,
    const ErrorCB& error_cb) {
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;
}

void VideoDecoderImpl<FFMPEG>::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(input_buffer);
  SB_DCHECK(queue_.Poll().type == kInvalid);
  SB_DCHECK(decoder_status_cb_);

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }

  if (!SbThreadIsValid(decoder_thread_)) {
    decoder_thread_ = SbThreadCreate(
        0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true, "ff_video_dec",
        &VideoDecoderImpl<FFMPEG>::ThreadEntryPoint, this);
    SB_DCHECK(SbThreadIsValid(decoder_thread_));
  }

  queue_.Put(Event(input_buffer));
}

void VideoDecoderImpl<FFMPEG>::WriteEndOfStream() {
  SB_DCHECK(decoder_status_cb_);

  // We have to flush the decoder to decode the rest frames and to ensure that
  // Decode() is not called when the stream is ended.
  stream_ended_ = true;

  if (!SbThreadIsValid(decoder_thread_)) {
    // In case there is no WriteInputBuffer() call before WriteEndOfStream(),
    // don't create the decoder thread and send the EOS frame directly.
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
    return;
  }

  queue_.Put(Event(kWriteEndOfStream));
}

void VideoDecoderImpl<FFMPEG>::Reset() {
  // Join the thread to ensure that all callbacks in process are finished.
  if (SbThreadIsValid(decoder_thread_)) {
    queue_.Put(Event(kReset));
    SbThreadJoin(decoder_thread_, NULL);
  }

  if (codec_context_ != NULL) {
    ffmpeg_->avcodec_flush_buffers(codec_context_);
  }

  decoder_thread_ = kSbThreadInvalid;
  stream_ended_ = false;

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    TeardownCodec();
    InitializeCodec();
  }
}

bool VideoDecoderImpl<FFMPEG>::is_valid() const {
  return (ffmpeg_ != NULL) && ffmpeg_->is_valid() && (codec_context_ != NULL);
}

// static
void* VideoDecoderImpl<FFMPEG>::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  VideoDecoderImpl<FFMPEG>* decoder =
      reinterpret_cast<VideoDecoderImpl<FFMPEG>*>(context);
  decoder->DecoderThreadFunc();
  return NULL;
}

void VideoDecoderImpl<FFMPEG>::DecoderThreadFunc() {
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
      ffmpeg_->av_init_packet(&packet);
      packet.data = const_cast<uint8_t*>(event.input_buffer->data());
      packet.size = event.input_buffer->size();
      packet.pts = event.input_buffer->timestamp();
      codec_context_->reordered_opaque = packet.pts;

      DecodePacket(&packet);
      decoder_status_cb_(kNeedMoreInput, NULL);
    } else {
      SB_DCHECK(event.type == kWriteEndOfStream);
      // Stream has ended, try to decode any frames left in ffmpeg.
      AVPacket packet;
      do {
        ffmpeg_->av_init_packet(&packet);
        packet.data = NULL;
        packet.size = 0;
        packet.pts = 0;
      } while (DecodePacket(&packet));

      decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
    }
  }
}

bool VideoDecoderImpl<FFMPEG>::DecodePacket(AVPacket* packet) {
  SB_DCHECK(packet != NULL);

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  ffmpeg_->av_frame_unref(av_frame_);
#else   // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  ffmpeg_->avcodec_get_frame_defaults(av_frame_);
#endif  // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  int frame_decoded = 0;
  int decode_result = ffmpeg_->avcodec_decode_video2(codec_context_, av_frame_,
                                                     &frame_decoded, packet);
  if (decode_result < 0) {
    SB_DLOG(ERROR) << "avcodec_decode_video2() failed with result "
                   << decode_result;
    error_cb_();
    error_occured_ = true;
    return false;
  }
  if (frame_decoded == 0) {
    return false;
  }

  if (av_frame_->opaque == NULL) {
    SB_DLOG(ERROR) << "Video frame was produced yet has invalid frame data.";
    error_cb_();
    error_occured_ = true;
    return false;
  }

  int pitch = AlignUp(av_frame_->width, kAlignment * 2);

  scoped_refptr<CpuVideoFrame> frame = CpuVideoFrame::CreateYV12Frame(
      av_frame_->width, av_frame_->height, pitch, av_frame_->reordered_opaque,
      av_frame_->data[0], av_frame_->data[1], av_frame_->data[2]);

  bool result = true;
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    result = UpdateDecodeTarget(frame);
  }

  decoder_status_cb_(kBufferFull, frame);

  return result;
}

bool VideoDecoderImpl<FFMPEG>::UpdateDecodeTarget(
    const scoped_refptr<CpuVideoFrame>& frame) {
  SbDecodeTarget decode_target = DecodeTargetCreate(
      decode_target_graphics_context_provider_, frame, decode_target_);

  // Lock only after the post to the renderer thread, to prevent deadlock.
  ScopedLock lock(decode_target_mutex_);
  decode_target_ = decode_target;

  if (!SbDecodeTargetIsValid(decode_target)) {
    SB_LOG(ERROR) << "Could not acquire a decode target from provider.";
    return false;
  }

  return true;
}

void VideoDecoderImpl<FFMPEG>::InitializeCodec() {
  codec_context_ = ffmpeg_->avcodec_alloc_context3(NULL);

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
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  codec_context_->get_buffer2 = AllocateBufferCallback;
#else   // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  codec_context_->get_buffer = AllocateBufferCallback;
  codec_context_->release_buffer = ReleaseBuffer;
#endif  // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)

  codec_context_->extradata = NULL;
  codec_context_->extradata_size = 0;

  AVCodec* codec = ffmpeg_->avcodec_find_decoder(codec_context_->codec_id);

  if (codec == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    TeardownCodec();
    return;
  }

  int rv = ffmpeg_->OpenCodec(codec_context_, codec);
  if (rv < 0) {
    SB_LOG(ERROR) << "Unable to open codec";
    TeardownCodec();
    return;
  }

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  av_frame_ = ffmpeg_->av_frame_alloc();
#else   // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  av_frame_ = ffmpeg_->avcodec_alloc_frame();
#endif  // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  if (av_frame_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate audio frame";
    TeardownCodec();
  }
}

void VideoDecoderImpl<FFMPEG>::TeardownCodec() {
  if (codec_context_) {
    ffmpeg_->CloseCodec(codec_context_);
    ffmpeg_->av_freep(&codec_context_);
  }
  ffmpeg_->av_freep(&av_frame_);

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    ScopedLock lock(decode_target_mutex_);
    if (SbDecodeTargetIsValid(decode_target_)) {
      DecodeTargetRelease(decode_target_graphics_context_provider_,
                          decode_target_);
      decode_target_ = kSbDecodeTargetInvalid;
    }
  }
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget VideoDecoderImpl<FFMPEG>::GetCurrentDecodeTarget() {
  SB_DCHECK(output_mode_ == kSbPlayerOutputModeDecodeToTexture);

  // We must take a lock here since this function can be called from a
  // separate thread.
  ScopedLock lock(decode_target_mutex_);
  if (SbDecodeTargetIsValid(decode_target_)) {
    // Make a disposable copy, since the state is internally reused by this
    // class (to avoid recreating GL objects).
    return DecodeTargetCopy(decode_target_);
  } else {
    return kSbDecodeTargetInvalid;
  }
}

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)

int VideoDecoderImpl<FFMPEG>::AllocateBuffer(AVCodecContext* codec_context,
                                             AVFrame* frame,
                                             int flags) {
  if (codec_context->pix_fmt != PIX_FMT_YUV420P &&
      codec_context->pix_fmt != PIX_FMT_YUVJ420P) {
    SB_DLOG(WARNING) << "Unsupported pix_fmt " << codec_context->pix_fmt;
    return AVERROR(EINVAL);
  }

  int ret = ffmpeg_->av_image_check_size(codec_context->width,
                                         codec_context->height, 0, NULL);
  if (ret < 0) {
    return ret;
  }

  // Align to kAlignment * 2 as we will divide y_stride by 2 for u and v planes
  size_t y_stride = AlignUp(codec_context->width, kAlignment * 2);
  size_t uv_stride = y_stride / 2;
  size_t aligned_height = AlignUp(codec_context->height, kAlignment * 2);

  uint8_t* frame_buffer = reinterpret_cast<uint8_t*>(SbMemoryAllocateAligned(
      kAlignment, GetYV12SizeInBytes(y_stride, aligned_height)));

  frame->data[0] = frame_buffer;
  frame->linesize[0] = y_stride;

  frame->data[1] = frame_buffer + y_stride * aligned_height;
  frame->linesize[1] = uv_stride;

  frame->data[2] = frame->data[1] + uv_stride * aligned_height / 2;
  frame->linesize[2] = uv_stride;

  frame->opaque = frame;
  frame->width = codec_context->width;
  frame->height = codec_context->height;
  frame->format = codec_context->pix_fmt;

  frame->reordered_opaque = codec_context->reordered_opaque;

  frame->buf[0] = static_cast<AVBufferRef*>(ffmpeg_->av_buffer_create(
      frame_buffer, GetYV12SizeInBytes(y_stride, aligned_height),
      &ReleaseBuffer, frame->opaque, 0));
  return 0;
}

#else   // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)

int VideoDecoderImpl<FFMPEG>::AllocateBuffer(AVCodecContext* codec_context,
                                             AVFrame* frame) {
  if (codec_context->pix_fmt != PIX_FMT_YUV420P &&
      codec_context->pix_fmt != PIX_FMT_YUVJ420P) {
    SB_DLOG(WARNING) << "Unsupported pix_fmt " << codec_context->pix_fmt;
    return AVERROR(EINVAL);
  }

  int ret = ffmpeg_->av_image_check_size(codec_context->width,
                                         codec_context->height, 0, NULL);
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
#endif  // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
