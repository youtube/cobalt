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

#include "starboard/shared/libvpx/vpx_video_decoder.h"

#include "starboard/linux/shared/decode_target_internal.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace vpx {

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec,
                           SbPlayerOutputMode output_mode,
                           SbDecodeTargetGraphicsContextProvider*
                               decode_target_graphics_context_provider)
    : current_frame_width_(0),
      current_frame_height_(0),
      stream_ended_(false),
      error_occured_(false),
      decoder_thread_(kSbThreadInvalid),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      decode_target_(kSbDecodeTargetInvalid) {
  SB_DCHECK(video_codec == kSbMediaVideoCodecVp9);
}

VideoDecoder::~VideoDecoder() {
  Reset();
  TeardownCodec();
}

void VideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                              const ErrorCB& error_cb) {
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;
}

void VideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(input_buffer);
  SB_DCHECK(queue_.Poll().type == kInvalid);
  SB_DCHECK(decoder_status_cb_);

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }

  if (!SbThreadIsValid(decoder_thread_)) {
    decoder_thread_ =
        SbThreadCreate(0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
                       "vp9_video_dec", &VideoDecoder::ThreadEntryPoint, this);
    SB_DCHECK(SbThreadIsValid(decoder_thread_));
  }

  queue_.Put(Event(input_buffer));
}

void VideoDecoder::WriteEndOfStream() {
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

void VideoDecoder::Reset() {
  // Join the thread to ensure that all callbacks in process are finished.
  if (SbThreadIsValid(decoder_thread_)) {
    queue_.Put(Event(kReset));
    SbThreadJoin(decoder_thread_, NULL);
  }

  decoder_thread_ = kSbThreadInvalid;
  error_occured_ = false;
  stream_ended_ = false;

  TeardownCodec();
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
      DecodeOneBuffer(event.input_buffer);
    } else {
      SB_DCHECK(event.type == kWriteEndOfStream);
      // TODO: Flush the frames inside the decoder, though this is not required
      //       for vp9 in most cases.
      decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
    }
  }
}

bool VideoDecoder::UpdateDecodeTarget(
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

void VideoDecoder::ReportError() {
  error_occured_ = true;
  error_cb_();
}

void VideoDecoder::InitializeCodec() {
  context_.reset(new vpx_codec_ctx);
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_config.w = current_frame_width_;
  vpx_config.h = current_frame_height_;
  vpx_config.threads = 8;

  vpx_codec_err_t status =
      vpx_codec_dec_init(context_.get(), vpx_codec_vp9_dx(), &vpx_config, 0);
  if (status != VPX_CODEC_OK) {
    ReportError();
    context_.reset();
  }
}

void VideoDecoder::TeardownCodec() {
  if (context_) {
    vpx_codec_destroy(context_.get());
    context_.reset();
  }

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    SbDecodeTarget decode_target_to_release;
    {
      ScopedLock lock(decode_target_mutex_);
      decode_target_to_release = decode_target_;
      decode_target_ = kSbDecodeTargetInvalid;
    }

    if (SbDecodeTargetIsValid(decode_target_to_release)) {
      DecodeTargetRelease(decode_target_graphics_context_provider_,
                          decode_target_to_release);
    }
  }
}

void VideoDecoder::DecodeOneBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(input_buffer);
  const SbMediaVideoSampleInfo* sample_info = input_buffer->video_sample_info();
  SB_DCHECK(sample_info);
  if (!context_ || sample_info->frame_width != current_frame_width_ ||
      sample_info->frame_height != current_frame_height_) {
    current_frame_width_ = sample_info->frame_width;
    current_frame_height_ = sample_info->frame_height;
    TeardownCodec();
    InitializeCodec();
  }

  SB_DCHECK(context_);

  SbMediaTime pts = input_buffer->pts();
  vpx_codec_err_t status = vpx_codec_decode(
      context_.get(), input_buffer->data(), input_buffer->size(), &pts, 0);
  if (status != VPX_CODEC_OK) {
    SB_DLOG(ERROR) << "vpx_codec_decode() failed, status=" << status;
    ReportError();
    return;
  }

  // Gets pointer to decoded data.
  vpx_codec_iter_t dummy = NULL;
  const vpx_image_t* vpx_image = vpx_codec_get_frame(context_.get(), &dummy);
  if (!vpx_image) {
    return;
  }

  if (vpx_image->user_priv != &pts) {
    SB_DLOG(ERROR) << "Invalid output timestamp.";
    ReportError();
    return;
  }

  if (vpx_image->fmt != VPX_IMG_FMT_YV12) {
    SB_DCHECK(vpx_image->fmt == VPX_IMG_FMT_I420)
        << "Invalid vpx_image->fmt: " << vpx_image->fmt;
    if (vpx_image->fmt != VPX_IMG_FMT_I420) {
      ReportError();
      return;
    }
  }

  SB_DCHECK(vpx_image->stride[VPX_PLANE_Y] ==
            vpx_image->stride[VPX_PLANE_U] * 2);
  SB_DCHECK(vpx_image->stride[VPX_PLANE_U] == vpx_image->stride[VPX_PLANE_V]);
  SB_DCHECK(vpx_image->planes[VPX_PLANE_Y] < vpx_image->planes[VPX_PLANE_U]);
  SB_DCHECK(vpx_image->planes[VPX_PLANE_U] < vpx_image->planes[VPX_PLANE_V]);

  if (vpx_image->stride[VPX_PLANE_Y] != vpx_image->stride[VPX_PLANE_U] * 2 ||
      vpx_image->stride[VPX_PLANE_U] != vpx_image->stride[VPX_PLANE_V] ||
      vpx_image->planes[VPX_PLANE_Y] >= vpx_image->planes[VPX_PLANE_U] ||
      vpx_image->planes[VPX_PLANE_U] >= vpx_image->planes[VPX_PLANE_V]) {
    ReportError();
    return;
  }

  // Create a VideoFrame from decoded frame data. The data is in YV12 format.
  // Each component of a pixel takes one byte and they are in their own planes.
  // UV planes have half resolution both vertically and horizontally.
  scoped_refptr<CpuVideoFrame> frame = CpuVideoFrame::CreateYV12Frame(
      current_frame_width_, current_frame_height_,
      vpx_image->stride[VPX_PLANE_Y], pts, vpx_image->planes[VPX_PLANE_Y],
      vpx_image->planes[VPX_PLANE_U], vpx_image->planes[VPX_PLANE_V]);
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    UpdateDecodeTarget(frame);
  }

  decoder_status_cb_(kNeedMoreInput, frame);
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget VideoDecoder::GetCurrentDecodeTarget() {
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

}  // namespace vpx
}  // namespace shared
}  // namespace starboard
