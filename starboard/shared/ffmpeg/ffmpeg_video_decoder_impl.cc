// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <stdlib.h>

#include <string>

#include "starboard/common/check_op.h"
#include "starboard/common/string.h"
#include "starboard/linux/shared/decode_target_internal.h"
#include "starboard/thread.h"

namespace starboard {

namespace {

// FFmpeg requires its decoding buffers to align with platform alignment.  It
// mentions inside
// http://ffmpeg.org/doxygen/trunk/structAVFrame.html#aa52bfc6605f6a3059a0c3226cc0f6567
// that the alignment on most modern desktop systems are 16 or 32.
static const int kAlignment = 32;

size_t AlignUp(size_t size, int alignment) {
  SB_DCHECK_EQ((alignment & (alignment - 1)), 0);
  return (size + alignment - 1) & ~(alignment - 1);
}

size_t GetYV12SizeInBytes(int32_t width, int32_t height) {
  return width * height * 3 / 2;
}

#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8

void ReleaseBuffer(void* opaque, uint8_t* data) {
  free(data);
}

int AllocateBufferCallback(AVCodecContext* codec_context,
                           AVFrame* frame,
                           int flags) {
  FfmpegVideoDecoderImpl<FFMPEG>* video_decoder =
      static_cast<FfmpegVideoDecoderImpl<FFMPEG>*>(codec_context->opaque);
  return video_decoder->AllocateBuffer(codec_context, frame, flags);
}

#else   // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8

int AllocateBufferCallback(AVCodecContext* codec_context, AVFrame* frame) {
  FfmpegVideoDecoderImpl<FFMPEG>* video_decoder =
      static_cast<FfmpegVideoDecoderImpl<FFMPEG>*>(codec_context->opaque);
  return video_decoder->AllocateBuffer(codec_context, frame);
}

void ReleaseBuffer(AVCodecContext*, AVFrame* frame) {
  free(frame->opaque);
  frame->opaque = NULL;

  // The FFmpeg API expects us to zero the data pointers in this callback.
  memset(frame->data, 0, sizeof(frame->data));
}
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8

AVCodecID GetFfmpegCodecIdByMediaCodec(SbMediaVideoCodec video_codec) {
  // Note: although SbMediaVideoCodec values exist for them, MPEG2VIDEO, THEORA,
  // and VC1 are not included here since we do not officially support them.
  // Also, note that VP9 and HEVC use different decoders (not FFmpeg).
  switch (video_codec) {
    case kSbMediaVideoCodecH264:
      return AV_CODEC_ID_H264;
    case kSbMediaVideoCodecVp8:
      return AV_CODEC_ID_VP8;
    default:
      SB_DLOG(WARNING) << "FFmpeg decoder does not support SbMediaVideoCodec "
                       << video_codec;
  }
  return AV_CODEC_ID_NONE;
}

const bool g_registered =
    FFMPEGDispatch::RegisterSpecialization(FFMPEG,
                                           LIBAVCODEC_VERSION_MAJOR,
                                           LIBAVFORMAT_VERSION_MAJOR,
                                           LIBAVUTIL_VERSION_MAJOR);

}  // namespace

FfmpegVideoDecoderImpl<FFMPEG>::FfmpegVideoDecoderImpl(
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider)
    : Thread("ff_video_dec"),
      video_codec_(video_codec),
      codec_context_(NULL),
      av_frame_(NULL),
      stream_ended_(false),
      error_occurred_(false),
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

FfmpegVideoDecoderImpl<FFMPEG>::~FfmpegVideoDecoderImpl() {
  Reset();
  TeardownCodec();
}

// static
FfmpegVideoDecoder* FfmpegVideoDecoderImpl<FFMPEG>::Create(
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider) {
  return new FfmpegVideoDecoderImpl<FFMPEG>(
      video_codec, output_mode, decode_target_graphics_context_provider);
}

void FfmpegVideoDecoderImpl<FFMPEG>::Initialize(
    const DecoderStatusCB& decoder_status_cb,
    const ErrorCB& error_cb) {
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;
}

void FfmpegVideoDecoderImpl<FFMPEG>::WriteInputBuffers(
    const InputBuffers& input_buffers) {
  SB_DCHECK_EQ(input_buffers.size(), 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK_EQ(queue_.Poll().type, kInvalid);
  SB_DCHECK(decoder_status_cb_);

  const auto& input_buffer = input_buffers[0];

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }

  if (!decoder_thread_) {
    SbThreadSetPriority(kSbThreadPriorityHigh);
    Start();
    decoder_thread_ = SbThreadGetId();
  }
  queue_.Put(Event(input_buffer));
}

void FfmpegVideoDecoderImpl<FFMPEG>::WriteEndOfStream() {
  SB_DCHECK(decoder_status_cb_);

  // We have to flush the decoder to decode the rest frames and to ensure that
  // Decode() is not called when the stream is ended.
  stream_ended_ = true;

  if (!decoder_thread_) {
    // In case there is no WriteInputBuffers() call before WriteEndOfStream(),
    // don't create the decoder thread and send the EOS frame directly.
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
    return;
  }

  queue_.Put(Event(kWriteEndOfStream));
}

void FfmpegVideoDecoderImpl<FFMPEG>::Reset() {
  // Join the thread to ensure that all callbacks in process are finished.
  if (decoder_thread_) {
    queue_.Put(Event(kReset));
    Join();
  }

  if (codec_context_ != NULL) {
    ffmpeg_->avcodec_flush_buffers(codec_context_);
  }

  decoder_thread_ = std::nullopt;
  stream_ended_ = false;

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    TeardownCodec();
    InitializeCodec();
  }

  std::lock_guard lock(decode_target_and_frames_mutex_);
  decltype(frames_) frames;
  frames_ = std::queue<scoped_refptr<CpuVideoFrame>>();
}

bool FfmpegVideoDecoderImpl<FFMPEG>::is_valid() const {
  return (ffmpeg_ != NULL) && ffmpeg_->is_valid() && (codec_context_ != NULL);
}

void FfmpegVideoDecoderImpl<FFMPEG>::Run() {
  for (;;) {
    Event event = queue_.Get();
    if (event.type == kReset) {
      return;
    }
    if (error_occurred_) {
      continue;
    }
    if (event.type == kWriteInputBuffer) {
      // Send |input_buffer| to ffmpeg and try to decode one frame.
      AVPacket packet;
      ffmpeg_->av_init_packet(&packet);
      packet.data = const_cast<uint8_t*>(event.input_buffer->data());
      packet.size = event.input_buffer->size();
      packet.pts = event.input_buffer->timestamp();
#if LIBAVCODEC_VERSION_MAJOR >= 61
// |reordered_opaque| is deprecated.
// We use ffmpeg's default mechanism to calculate |AVFrame.pts| from
// |AVPacket.pts|.
#else
      codec_context_->reordered_opaque = packet.pts;
#endif
      DecodePacket(&packet);
      decoder_status_cb_(kNeedMoreInput, NULL);
    } else {
      SB_DCHECK_EQ(event.type, kWriteEndOfStream);
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

bool FfmpegVideoDecoderImpl<FFMPEG>::DecodePacket(AVPacket* packet) {
  SB_DCHECK(packet);

  if (ffmpeg_->avcodec_version() > kAVCodecSupportsAvFrameAlloc) {
    ffmpeg_->av_frame_unref(av_frame_);
  } else {
    ffmpeg_->avcodec_get_frame_defaults(av_frame_);
  }
  int decode_result = 0;

  if (ffmpeg_->avcodec_version() < kAVCodecHasUniformDecodeAPI) {
    // Old decode API.
    int frame_decoded = 0;
    decode_result = ffmpeg_->avcodec_decode_video2(codec_context_, av_frame_,
                                                   &frame_decoded, packet);
    if (decode_result < 0) {
      SB_DLOG(ERROR) << "avcodec_decode_video2() failed with result "
                     << decode_result;
      error_cb_(kSbPlayerErrorDecode,
                FormatString("avcodec_decode_video2() failed with result %d.",
                             decode_result));
      error_occurred_ = true;
      return false;
    }

    if (frame_decoded == 0) {
      return false;
    }

    return ProcessDecodedFrame(*av_frame_);
  }

  // Newer decode API.
  const int send_packet_result =
      ffmpeg_->avcodec_send_packet(codec_context_, packet);
  if (send_packet_result != 0) {
    const std::string error_message = FormatString(
        "avcodec_send_packet() failed with result %d.", send_packet_result);
    SB_DLOG(WARNING) << error_message;
    error_cb_(kSbPlayerErrorDecode, error_message);
    return false;
  }

  // Keep receiving frames until the decoder has processed the entire packet.
  for (;;) {
    decode_result = ffmpeg_->avcodec_receive_frame(codec_context_, av_frame_);
    if (decode_result != 0) {
      // We either hit an error or are done processing packet.
      break;
    }

    if (!ProcessDecodedFrame(*av_frame_)) {
      return false;
    }
  }

  // A return value of AVERROR(EAGAIN) signifies that the decoder needs
  // another packet, so we are done processing the existing packet at that
  // point.
  if (decode_result != AVERROR(EAGAIN)) {
    SB_DLOG(WARNING) << "avcodec_receive_frame() failed with result: "
                     << decode_result;
    return false;
  }

  return true;
}

bool FfmpegVideoDecoderImpl<FFMPEG>::ProcessDecodedFrame(
    const AVFrame& av_frame) {
  if (av_frame.opaque == NULL) {
    SB_DLOG(ERROR) << "Video frame was produced yet has invalid frame data.";
    error_cb_(kSbPlayerErrorDecode,
              "Video frame was produced yet has invalid frame data.");
    error_occurred_ = true;
    return false;
  }

  int codec_aligned_width = av_frame.width;
  int codec_aligned_height = av_frame.height;
  int codec_linesize_align[AV_NUM_DATA_POINTERS];
  ffmpeg_->avcodec_align_dimensions2(codec_context_, &codec_aligned_width,
                                     &codec_aligned_height,
                                     codec_linesize_align);

  int y_pitch = AlignUp(av_frame.width, codec_linesize_align[0] * 2);
  int uv_pitch = av_frame.linesize[1];

  const int kBitDepth = 8;
  const int64_t frame_pts =
#if LIBAVCODEC_VERSION_MAJOR >= 61
      av_frame.pts;
#else
      av_frame.reordered_opaque;
#endif

  scoped_refptr<CpuVideoFrame> frame = CpuVideoFrame::CreateYV12Frame(
      kBitDepth, av_frame.width, av_frame.height, y_pitch, uv_pitch, frame_pts,
      av_frame.data[0], av_frame.data[1], av_frame.data[2]);

  bool result = true;
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    std::lock_guard lock(decode_target_and_frames_mutex_);
    frames_.push(frame);
  }

  decoder_status_cb_(kBufferFull, frame);

  return true;
}

void FfmpegVideoDecoderImpl<FFMPEG>::UpdateDecodeTarget_Locked(
    const scoped_refptr<CpuVideoFrame>& frame) {
  SbDecodeTarget decode_target = DecodeTargetCreate(
      decode_target_graphics_context_provider_, frame, decode_target_);

  // Lock only after the post to the renderer thread, to prevent deadlock.
  decode_target_ = decode_target;

  if (!SbDecodeTargetIsValid(decode_target)) {
    SB_LOG(ERROR) << "Could not acquire a decode target from provider.";
  }
}

void FfmpegVideoDecoderImpl<FFMPEG>::InitializeCodec() {
  codec_context_ = ffmpeg_->avcodec_alloc_context3(NULL);

  if (codec_context_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    return;
  }

  codec_context_->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_context_->codec_id = GetFfmpegCodecIdByMediaCodec(video_codec_);
  codec_context_->profile = FF_PROFILE_UNKNOWN;
  codec_context_->coded_width = 0;
  codec_context_->coded_height = 0;
  codec_context_->pix_fmt = PIX_FMT_NONE;

  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->thread_count = 2;
  codec_context_->opaque = this;
#if defined(CODEC_FLAG_EMU_EDGE)
  codec_context_->flags |= CODEC_FLAG_EMU_EDGE;
#endif
#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  codec_context_->get_buffer2 = AllocateBufferCallback;
#else   // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  codec_context_->get_buffer = AllocateBufferCallback;
  codec_context_->release_buffer = ReleaseBuffer;
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8

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

#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  av_frame_ = ffmpeg_->av_frame_alloc();
#else   // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  av_frame_ = ffmpeg_->avcodec_alloc_frame();
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  if (av_frame_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate audio frame";
    TeardownCodec();
  }
}

void FfmpegVideoDecoderImpl<FFMPEG>::TeardownCodec() {
  if (codec_context_) {
    ffmpeg_->CloseCodec(codec_context_);
    ffmpeg_->FreeContext(&codec_context_);
  }
  ffmpeg_->FreeFrame(&av_frame_);

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    std::lock_guard lock(decode_target_and_frames_mutex_);
    if (SbDecodeTargetIsValid(decode_target_)) {
      DecodeTargetRelease(decode_target_graphics_context_provider_,
                          decode_target_);
      decode_target_ = kSbDecodeTargetInvalid;
    }
  }
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget FfmpegVideoDecoderImpl<FFMPEG>::GetCurrentDecodeTarget() {
  SB_DCHECK_EQ(output_mode_, kSbPlayerOutputModeDecodeToTexture);

  // We must take a lock here since this function can be called from a
  // separate thread.
  std::lock_guard lock(decode_target_and_frames_mutex_);
  while (frames_.size() > 1 && frames_.front()->HasOneRef()) {
    frames_.pop();
  }
  if (!frames_.empty()) {
    UpdateDecodeTarget_Locked(frames_.front());
  }
  if (SbDecodeTargetIsValid(decode_target_)) {
    // Make a disposable copy, since the state is internally reused by this
    // class (to avoid recreating GL objects).
    return DecodeTargetCopy(decode_target_);
  } else {
    return kSbDecodeTargetInvalid;
  }
}

#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8

int FfmpegVideoDecoderImpl<FFMPEG>::AllocateBuffer(
    AVCodecContext* codec_context,
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

  int codec_aligned_width = codec_context->width;
  int codec_aligned_height = codec_context->height;
  int codec_linesize_align[AV_NUM_DATA_POINTERS];
  ffmpeg_->avcodec_align_dimensions2(codec_context, &codec_aligned_width,
                                     &codec_aligned_height,
                                     codec_linesize_align);

  // Align to linesize alignment * 2 as we will divide y_stride by 2 for
  // u and v planes.
  size_t y_stride = AlignUp(codec_context->width, codec_linesize_align[0] * 2);
  size_t uv_stride = y_stride / 2;
  size_t aligned_height = codec_aligned_height;

  uint8_t* frame_buffer = nullptr;
  posix_memalign(reinterpret_cast<void**>(&frame_buffer), kAlignment,
                 GetYV12SizeInBytes(y_stride, aligned_height));

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

#if LIBAVCODEC_VERSION_MAJOR >= 61
// We don't copy user data using |reordered_opaque|. |reordered_opaque| is
// deprecated and we don't need it, since we use ffmpeg's default mechanism to
// calculate |AVFrame.pts| from |AVPacket.pts|.
#else
  frame->reordered_opaque = codec_context->reordered_opaque;
#endif

  frame->buf[0] = static_cast<AVBufferRef*>(ffmpeg_->av_buffer_create(
      frame_buffer, GetYV12SizeInBytes(y_stride, aligned_height),
      &ReleaseBuffer, frame->opaque, 0));
  return 0;
}

#else   // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8

int FfmpegVideoDecoderImpl<FFMPEG>::AllocateBuffer(
    AVCodecContext* codec_context,
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

  int codec_aligned_width = codec_context->width;
  int codec_aligned_height = codec_context->height;
  int codec_linesize_align[AV_NUM_DATA_POINTERS];
  ffmpeg_->avcodec_align_dimensions2(codec_context, &codec_aligned_width,
                                     &codec_aligned_height,
                                     codec_linesize_align);

  // Align to linesize alignment * 2 as we will divide y_stride by 2 for
  // u and v planes.
  size_t y_stride = AlignUp(codec_context->width, codec_linesize_align[0] * 2);
  size_t uv_stride = y_stride / 2;
  size_t aligned_height = codec_aligned_height;
  uint8_t* frame_buffer = nullptr;
  posix_memalign(reinterpret_cast<void**>(&frame_buffer), kAlignment,
                 GetYV12SizeInBytes(y_stride, aligned_height));

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
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8

}  // namespace starboard
