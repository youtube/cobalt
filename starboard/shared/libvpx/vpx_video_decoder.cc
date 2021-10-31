// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/string.h"
#include "starboard/linux/shared/decode_target_internal.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace vpx {

using starboard::player::JobThread;

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec,
                           SbPlayerOutputMode output_mode,
                           SbDecodeTargetGraphicsContextProvider*
                               decode_target_graphics_context_provider)
    : current_frame_width_(0),
      current_frame_height_(0),
      stream_ended_(false),
      error_occurred_(false),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      decode_target_(kSbDecodeTargetInvalid) {
  SB_DCHECK(video_codec == kSbMediaVideoCodecVp9);
}

VideoDecoder::~VideoDecoder() {
  SB_DCHECK(BelongsToCurrentThread());
  Reset();
}

void VideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                              const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;
}

void VideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(decoder_status_cb_);

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("vpx_video_decoder"));
    SB_DCHECK(decoder_thread_);
  }

  decoder_thread_->job_queue()->Schedule(
      std::bind(&VideoDecoder::DecodeOneBuffer, this, input_buffer));
}

void VideoDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb_);

  // We have to flush the decoder to decode the rest frames and to ensure that
  // Decode() is not called when the stream is ended.
  stream_ended_ = true;

  if (!decoder_thread_) {
    // In case there is no WriteInputBuffer() call before WriteEndOfStream(),
    // don't create the decoder thread and send the EOS frame directly.
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
    return;
  }

  decoder_thread_->job_queue()->Schedule(
      std::bind(&VideoDecoder::DecodeEndOfStream, this));
}

void VideoDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  if (decoder_thread_) {
    decoder_thread_->job_queue()->Schedule(
        std::bind(&VideoDecoder::TeardownCodec, this));

    // Join the thread to ensure that all callbacks in process are finished.
    decoder_thread_.reset();
  }

  error_occurred_ = false;
  stream_ended_ = false;

  CancelPendingJobs();

  ScopedLock lock(decode_target_mutex_);
  frames_ = std::queue<scoped_refptr<CpuVideoFrame>>();
}

void VideoDecoder::UpdateDecodeTarget_Locked(
    const scoped_refptr<CpuVideoFrame>& frame) {
  SbDecodeTarget decode_target = DecodeTargetCreate(
      decode_target_graphics_context_provider_, frame, decode_target_);

  // Lock only after the post to the renderer thread, to prevent deadlock.
  decode_target_ = decode_target;

  if (!SbDecodeTargetIsValid(decode_target)) {
    SB_LOG(ERROR) << "Could not acquire a decode target from provider.";
  }
}

void VideoDecoder::ReportError(const std::string& error_message) {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  error_occurred_ = true;
  Schedule(std::bind(error_cb_, kSbPlayerErrorDecode, error_message));
}

void VideoDecoder::InitializeCodec() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  context_.reset(new vpx_codec_ctx);
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_config.w = current_frame_width_;
  vpx_config.h = current_frame_height_;
  vpx_config.threads = 8;

  vpx_codec_err_t status =
      vpx_codec_dec_init(context_.get(), vpx_codec_vp9_dx(), &vpx_config, 0);
  if (status != VPX_CODEC_OK) {
    SB_LOG(ERROR) << "vpx_codec_dec_init() failed with " << status;
    ReportError(
        FormatString("vpx_codec_dec_init() failed with status %d.", status));
    context_.reset();
  }
}

void VideoDecoder::TeardownCodec() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

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
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());
  SB_DCHECK(input_buffer);

  const SbMediaVideoSampleInfo& sample_info = input_buffer->video_sample_info();
  if (!context_ || sample_info.frame_width != current_frame_width_ ||
      sample_info.frame_height != current_frame_height_) {
    current_frame_width_ = sample_info.frame_width;
    current_frame_height_ = sample_info.frame_height;
    TeardownCodec();
    InitializeCodec();
  }

  SB_DCHECK(context_);

  SbTime timestamp = input_buffer->timestamp();
  vpx_codec_err_t status =
      vpx_codec_decode(context_.get(), input_buffer->data(),
                       input_buffer->size(), &timestamp, 0);
  if (status != VPX_CODEC_OK) {
    SB_DLOG(ERROR) << "vpx_codec_decode() failed, status=" << status;
    ReportError(
        FormatString("vpx_codec_decode() failed with status %d.", status));
    return;
  }

  // Gets pointer to decoded data.
  vpx_codec_iter_t dummy = NULL;
  const vpx_image_t* vpx_image = vpx_codec_get_frame(context_.get(), &dummy);
  if (!vpx_image) {
    return;
  }

  if (vpx_image->user_priv != &timestamp) {
    SB_DLOG(ERROR) << "Invalid output timestamp.";
    ReportError("Invalid output timestamp.");
    return;
  }

  if (vpx_image->fmt != VPX_IMG_FMT_YV12) {
    SB_DCHECK(vpx_image->fmt == VPX_IMG_FMT_I420 ||
              vpx_image->fmt == VPX_IMG_FMT_I42016)
        << "Unsupported vpx_image->fmt: " << vpx_image->fmt;
    if (vpx_image->fmt != VPX_IMG_FMT_I420 &&
        vpx_image->fmt != VPX_IMG_FMT_I42016) {
      ReportError(
          FormatString("Unsupported vpx_image->fmt: %d.", vpx_image->fmt));
      return;
    }
  }

  if (vpx_image->bit_depth != 8 && vpx_image->bit_depth != 10 &&
      vpx_image->bit_depth != 12) {
    SB_DLOG(ERROR) << "Unsupported bit depth " << vpx_image->bit_depth;
    ReportError(
        FormatString("Unsupported bit depth %d.", vpx_image->bit_depth));
    return;
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
    ReportError("Unsupported yuv plane format.");
    return;
  }

  // Create a VideoFrame from decoded frame data. The data is in YV12 format.
  // Each component of a pixel takes one byte and they are in their own planes.
  // UV planes have half resolution both vertically and horizontally.
  scoped_refptr<CpuVideoFrame> frame = CpuVideoFrame::CreateYV12Frame(
      vpx_image->bit_depth, current_frame_width_, current_frame_height_,
      vpx_image->stride[VPX_PLANE_Y], timestamp, vpx_image->planes[VPX_PLANE_Y],
      vpx_image->planes[VPX_PLANE_U], vpx_image->planes[VPX_PLANE_V]);
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    ScopedLock lock(decode_target_mutex_);
    frames_.push(frame);
  }

  Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, frame));
}

void VideoDecoder::DecodeEndOfStream() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  // TODO: Flush the frames inside the decoder, though this is not required
  //       for vp9 in most cases.
  Schedule(
      std::bind(decoder_status_cb_, kBufferFull, VideoFrame::CreateEOSFrame()));
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget VideoDecoder::GetCurrentDecodeTarget() {
  SB_DCHECK(output_mode_ == kSbPlayerOutputModeDecodeToTexture);

  // We must take a lock here since this function can be called from a
  // separate thread.
  ScopedLock lock(decode_target_mutex_);
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

}  // namespace vpx
}  // namespace shared
}  // namespace starboard
