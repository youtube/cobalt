/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/filters/shell_raw_video_decoder_linux.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_util.h"
#include "media/filters/shell_ffmpeg.h"

namespace media {

using base::TimeDelta;
typedef ShellVideoDataAllocator::FrameBuffer FrameBuffer;
typedef ShellVideoDataAllocator::YV12Param YV12Param;

namespace {

// FFmpeg requires its decoding buffers to align with platform alignment.  It
// mentions inside
// http://ffmpeg.org/doxygen/trunk/structAVFrame.html#aa52bfc6605f6a3059a0c3226cc0f6567
// that the alignment on most modern desktop systems are 16 or 32.  We use 64 to
// be safe.
const int kPlatformAlignment = 32;

size_t AlignUp(size_t size, int alignment) {
  DCHECK_EQ(alignment & (alignment - 1), 0);
  return (size + alignment - 1) & ~(alignment - 1);
}

size_t GetYV12SizeInBytes(int32 width, int32 height) {
  return width * height * 3 / 2;
}

VideoFrame::Format PixelFormatToVideoFormat(PixelFormat pixel_format) {
  switch (pixel_format) {
    case PIX_FMT_YUV420P:
      return VideoFrame::YV12;
    case PIX_FMT_YUVJ420P:
      return VideoFrame::YV12;
    default:
      DLOG(ERROR) << "Unsupported PixelFormat: " << pixel_format;
  }
  return VideoFrame::INVALID;
}

int VideoCodecProfileToProfileID(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return FF_PROFILE_H264_BASELINE;
    case H264PROFILE_MAIN:
      return FF_PROFILE_H264_MAIN;
    case H264PROFILE_EXTENDED:
      return FF_PROFILE_H264_EXTENDED;
    case H264PROFILE_HIGH:
      return FF_PROFILE_H264_HIGH;
    case H264PROFILE_HIGH10PROFILE:
      return FF_PROFILE_H264_HIGH_10;
    case H264PROFILE_HIGH422PROFILE:
      return FF_PROFILE_H264_HIGH_422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return FF_PROFILE_H264_HIGH_444_PREDICTIVE;
    default:
      DLOG(ERROR) << "Unknown VideoCodecProfile: " << profile;
  }
  return FF_PROFILE_UNKNOWN;
}

PixelFormat VideoFormatToPixelFormat(VideoFrame::Format video_format) {
  switch (video_format) {
    case VideoFrame::YV12:
      return PIX_FMT_YUV420P;
    default:
      DLOG(ERROR) << "Unsupported VideoFrame::Format: " << video_format;
  }
  return PIX_FMT_NONE;
}

void CopyColorPlane(const uint8* src,
                    int32 width_in_bytes,
                    int32 pitch,
                    int32 height,
                    uint8* dest) {
  while (height > 0) {
    memcpy(dest, src, width_in_bytes);
    src += pitch;
    dest += width_in_bytes;
    --height;
  }
}

// Convert a frame buffer whose pitch might be different than its width to a
// frame buffer whose pitch is the same as its width.  This involves line by
// line copy of all three color planes.
scoped_refptr<FrameBuffer> ConvertFrameBuffer(
    const scoped_refptr<FrameBuffer>& src,
    int32 width,
    int32 height) {
  DCHECK(width % 2 == 0) << "Invalid width " << width;
  DCHECK(height % 2 == 0) << "Invalid height " << height;

  ShellVideoDataAllocator* allocator =
      ShellMediaPlatform::Instance()->GetVideoDataAllocator();
  DCHECK(allocator);
  size_t yv12_frame_size = GetYV12SizeInBytes(width, height);
  scoped_refptr<FrameBuffer> dest =
      allocator->AllocateFrameBuffer(yv12_frame_size, kPlatformAlignment);

  // Get the src pitch and height as what we did in GetVideoBuffer().
  int32 src_pitch = AlignUp(width, kPlatformAlignment * 2);
  int32 src_height = AlignUp(height, kPlatformAlignment * 2);
  DCHECK_LE(GetYV12SizeInBytes(src_pitch, src_height), src->size());
  DCHECK_LE(GetYV12SizeInBytes(width, height), dest->size());

  uint8* src_data = src->data();
  uint8* dest_data = dest->data();
  CopyColorPlane(src_data, width, src_pitch, height, dest_data);
  src_data += src_pitch * src_height;
  dest_data += width * height;
  CopyColorPlane(src_data, width / 2, src_pitch / 2, height / 2, dest_data);
  src_data += src_pitch / 2 * src_height / 2;
  dest_data += width / 2 * height / 2;
  CopyColorPlane(src_data, width / 2, src_pitch / 2, height / 2, dest_data);
  return dest;
}

// TODO: Make this decoder handle decoder errors. Now it assumes
// that the input stream is always correct.
class ShellRawVideoDecoderLinux : public ShellRawVideoDecoder {
 public:
  explicit ShellRawVideoDecoderLinux(ShellVideoDataAllocator* allocator);
  ~ShellRawVideoDecoderLinux();

  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) override;
  virtual bool Flush() override;
  virtual bool UpdateConfig(const VideoDecoderConfig& config) override;

 private:
  void ReleaseResource();
  static int GetVideoBuffer(AVCodecContext* codec_context, AVFrame* frame);
  static void ReleaseVideoBuffer(AVCodecContext*, AVFrame* frame);

  ShellVideoDataAllocator* allocator_;
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;
  gfx::Size natural_size_;

  DISALLOW_COPY_AND_ASSIGN(ShellRawVideoDecoderLinux);
};

ShellRawVideoDecoderLinux::ShellRawVideoDecoderLinux(
    ShellVideoDataAllocator* allocator)
    : allocator_(allocator), codec_context_(NULL), av_frame_(NULL) {
  EnsureFfmpegInitialized();
}

ShellRawVideoDecoderLinux::~ShellRawVideoDecoderLinux() {
  ReleaseResource();
}

void ShellRawVideoDecoderLinux::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    const DecodeCB& decode_cb) {
  DCHECK(buffer);
  DCHECK(!decode_cb.is_null());

  AVPacket packet;
  av_init_packet(&packet);
  avcodec_get_frame_defaults(av_frame_);
  packet.data = buffer->GetWritableData();
  packet.size = buffer->GetDataSize();
  packet.pts = buffer->GetTimestamp().InMilliseconds();

  int frame_decoded = 0;
  int result =
      avcodec_decode_video2(codec_context_, av_frame_, &frame_decoded, &packet);
  DCHECK_GE(result, 0);
  if (frame_decoded == 0) {
    decode_cb.Run(NEED_MORE_DATA, NULL);
    return;
  }

  // TODO(fbarchard): Work around for FFmpeg http://crbug.com/27675
  // The decoder is in a bad state and not decoding correctly.
  // Checking for NULL avoids a crash in CopyPlane().
  if (!av_frame_->data[VideoFrame::kYPlane] ||
      !av_frame_->data[VideoFrame::kUPlane] ||
      !av_frame_->data[VideoFrame::kVPlane]) {
    DLOG(ERROR) << "Video frame was produced yet has invalid frame data.";
    decode_cb.Run(FATAL_ERROR, NULL);
    return;
  }

  if (!av_frame_->opaque) {
    DLOG(ERROR) << "VideoFrame object associated with frame data not set.";
    decode_cb.Run(FATAL_ERROR, NULL);
    return;
  }

  scoped_refptr<FrameBuffer> frame_buffer =
      static_cast<FrameBuffer*>(av_frame_->opaque);
  DCHECK(frame_buffer);

  TimeDelta timestamp = TimeDelta::FromMilliseconds(av_frame_->pkt_pts);
  // TODO: Currently the Linux egl backend doesn't support texture
  // with pitch different than its width.  So we create a new video frame whose
  // visible size is the same as its coded size to ensure that the underlying
  // texture has the same pitch as its width.  This makes code complex, though
  // it doesn't necessary mean that the new code is slower as we have to make a
  // copy anyway to avoid using the frame buffer while Ffmpeg is still using it.
  // It is worth revisiting if we are going to release Linux as a production
  // platform.
  YV12Param param(av_frame_->width, av_frame_->height,
                  gfx::Rect(av_frame_->width, av_frame_->height),
                  frame_buffer->data());
  // We have to make a copy of the frame buffer as the frame buffer retrieved
  // from |av_frame_->opaque| may still be used by Ffmpeg.
  size_t yv12_frame_size =
      GetYV12SizeInBytes(av_frame_->width, av_frame_->height);
  scoped_refptr<FrameBuffer> converted_frame_buffer =
      ConvertFrameBuffer(frame_buffer, av_frame_->width, av_frame_->height);
  scoped_refptr<VideoFrame> frame =
      allocator_->CreateYV12Frame(converted_frame_buffer, param, timestamp);

  decode_cb.Run(FRAME_DECODED, frame);
}

bool ShellRawVideoDecoderLinux::Flush() {
  avcodec_flush_buffers(codec_context_);

  return true;
}

bool ShellRawVideoDecoderLinux::UpdateConfig(const VideoDecoderConfig& config) {
  ReleaseResource();

  natural_size_ = config.natural_size();

  codec_context_ = avcodec_alloc_context3(NULL);
  DCHECK(codec_context_);
  codec_context_->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_context_->codec_id = CODEC_ID_H264;
  codec_context_->profile = VideoCodecProfileToProfileID(config.profile());
  codec_context_->coded_width = config.coded_size().width();
  codec_context_->coded_height = config.coded_size().height();
  codec_context_->pix_fmt = VideoFormatToPixelFormat(config.format());

  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->thread_count = 2;
  codec_context_->opaque = this;
  codec_context_->flags |= CODEC_FLAG_EMU_EDGE;
  codec_context_->get_buffer = GetVideoBuffer;
  codec_context_->release_buffer = ReleaseVideoBuffer;

  if (config.extra_data()) {
    codec_context_->extradata_size = config.extra_data_size();
    codec_context_->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data_size() + FF_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context_->extradata, config.extra_data(),
           config.extra_data_size());
    memset(codec_context_->extradata + config.extra_data_size(), '\0',
           FF_INPUT_BUFFER_PADDING_SIZE);
  } else {
    codec_context_->extradata = NULL;
    codec_context_->extradata_size = 0;
  }

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  DCHECK(codec);
  int rv = avcodec_open2(codec_context_, codec, NULL);
  DCHECK_GE(rv, 0);
  if (rv < 0) {
    DLOG(ERROR) << "Unable to open codec, result = " << rv;
    return false;
  }

  av_frame_ = avcodec_alloc_frame();
  DCHECK(av_frame_);

  return true;
}

void ShellRawVideoDecoderLinux::ReleaseResource() {
  if (codec_context_) {
    av_free(codec_context_->extradata);
    avcodec_close(codec_context_);
    av_free(codec_context_);
    codec_context_ = NULL;
  }
  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
}

int ShellRawVideoDecoderLinux::GetVideoBuffer(AVCodecContext* codec_context,
                                              AVFrame* frame) {
  VideoFrame::Format format = PixelFormatToVideoFormat(codec_context->pix_fmt);
  if (format == VideoFrame::INVALID)
    return AVERROR(EINVAL);
  DCHECK_EQ(format, VideoFrame::YV12);

  gfx::Size size(codec_context->width, codec_context->height);
  int ret = av_image_check_size(size.width(), size.height(), 0, NULL);
  if (ret < 0) {
    return ret;
  }

  if (!VideoFrame::IsValidConfig(format, size, gfx::Rect(size), size)) {
    return AVERROR(EINVAL);
  }

  // Align to kPlatformAlignment * 2 as we will divide the y_stride by 2 for u
  // and v planes.
  size_t y_stride = AlignUp(size.width(), kPlatformAlignment * 2);
  size_t uv_stride = y_stride / 2;
  size_t aligned_height = AlignUp(size.height(), kPlatformAlignment * 2);
  scoped_refptr<FrameBuffer> frame_buffer = allocator_->AllocateFrameBuffer(
      GetYV12SizeInBytes(y_stride, aligned_height), kPlatformAlignment);

  // y plane
  frame->base[0] = frame_buffer->data();
  frame->data[0] = frame->base[0];
  frame->linesize[0] = y_stride;
  // u plane
  frame->base[1] = frame->base[0] + y_stride * aligned_height;
  frame->data[1] = frame->base[1];
  memset(frame->data[1], 0, uv_stride * (aligned_height / 2));
  frame->linesize[1] = uv_stride;
  // v plane
  frame->base[2] = frame->base[1] + uv_stride * aligned_height / 2;
  frame->data[2] = frame->base[2];
  memset(frame->data[2], 0, uv_stride * (aligned_height / 2));
  frame->linesize[2] = uv_stride;

  frame->opaque = NULL;
  frame_buffer.swap(reinterpret_cast<FrameBuffer**>(&frame->opaque));
  frame->type = FF_BUFFER_TYPE_USER;
  frame->pkt_pts =
      codec_context->pkt ? codec_context->pkt->pts : AV_NOPTS_VALUE;
  frame->width = codec_context->width;
  frame->height = codec_context->height;
  frame->format = codec_context->pix_fmt;

  return 0;
}

void ShellRawVideoDecoderLinux::ReleaseVideoBuffer(AVCodecContext*,
                                                   AVFrame* frame) {
  scoped_refptr<FrameBuffer> frame_buffer;
  frame_buffer.swap(reinterpret_cast<FrameBuffer**>(&frame->opaque));

  // The FFmpeg API expects us to zero the data pointers in
  // this callback
  memset(frame->data, 0, sizeof(frame->data));
  frame->opaque = NULL;
}

}  // namespace

scoped_ptr<ShellRawVideoDecoder> CreateShellRawVideoDecoderLinux(
    ShellVideoDataAllocator* allocator,
    const VideoDecoderConfig& config,
    Decryptor* decryptor,
    bool was_encrypted) {
  DCHECK(allocator);
  DCHECK_EQ(config.codec(), kCodecH264) << "Video codec " << config.codec()
                                        << " is not supported.";
  scoped_ptr<ShellRawVideoDecoder> decoder(
      new ShellRawVideoDecoderLinux(allocator));
  if (decoder->UpdateConfig(config))
    return decoder.Pass();
  return scoped_ptr<ShellRawVideoDecoder>();
}

}  // namespace media
