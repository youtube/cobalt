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

#include "starboard/common/check_op.h"
#include "starboard/common/string.h"
#include "starboard/linux/shared/decode_target_internal.h"
#include "starboard/thread.h"

namespace starboard {

VpxVideoDecoder::VpxVideoDecoder(SbMediaVideoCodec video_codec,
                                 SbPlayerOutputMode output_mode,
                                 SbDecodeTargetGraphicsContextProvider*
                                     decode_target_graphics_context_provider)
    : stream_ended_(false),
      error_occurred_(false),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      decode_target_(kSbDecodeTargetInvalid) {
  SB_DCHECK_EQ(video_codec, kSbMediaVideoCodecVp9);
}

VpxVideoDecoder::~VpxVideoDecoder() {
  SB_CHECK(BelongsToCurrentThread());
  Reset();
}

void VpxVideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                                 const ErrorCB& error_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;
}

void VpxVideoDecoder::WriteInputBuffers(const InputBuffers& input_buffers) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK_EQ(input_buffers.size(), 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(decoder_status_cb_);

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("vpx_video_decoder"));
    SB_DCHECK(decoder_thread_);
  }

  const auto& input_buffer = input_buffers[0];
  decoder_thread_->Schedule(
      std::bind(&VpxVideoDecoder::DecodeOneBuffer, this, input_buffer));
}

void VpxVideoDecoder::WriteEndOfStream() {
  SB_CHECK(BelongsToCurrentThread());
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

  decoder_thread_->Schedule(
      std::bind(&VpxVideoDecoder::DecodeEndOfStream, this));
}

void VpxVideoDecoder::Reset() {
  SB_CHECK(BelongsToCurrentThread());

  if (decoder_thread_) {
    // Wait to ensure all tasks are done before decoder_thread_ reset.
    decoder_thread_->ScheduleAndWait(
        std::bind(&VpxVideoDecoder::TeardownCodec, this));

    decoder_thread_.reset();
  }

  error_occurred_ = false;
  stream_ended_ = false;

  CancelPendingJobs();

  std::lock_guard lock(decode_target_mutex_);
  frames_ = std::queue<scoped_refptr<CpuVideoFrame>>();
}

void VpxVideoDecoder::UpdateDecodeTarget_Locked(
    const scoped_refptr<CpuVideoFrame>& frame) {
  SbDecodeTarget decode_target = DecodeTargetCreate(
      decode_target_graphics_context_provider_, frame, decode_target_);

  // Lock only after the post to the renderer thread, to prevent deadlock.
  decode_target_ = decode_target;

  if (!SbDecodeTargetIsValid(decode_target)) {
    SB_LOG(ERROR) << "Could not acquire a decode target from provider.";
  }
}

void VpxVideoDecoder::ReportError(const std::string& error_message) {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());

  error_occurred_ = true;
  Schedule(std::bind(error_cb_, kSbPlayerErrorDecode, error_message));
}

void VpxVideoDecoder::InitializeCodec() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());

  context_.reset(new vpx_codec_ctx);
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_config.w = current_frame_size_.width;
  vpx_config.h = current_frame_size_.height;
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

void VpxVideoDecoder::TeardownCodec() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());

  if (context_) {
    vpx_codec_destroy(context_.get());
    context_.reset();
  }

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    SbDecodeTarget decode_target_to_release;
    {
      std::lock_guard lock(decode_target_mutex_);
      decode_target_to_release = decode_target_;
      decode_target_ = kSbDecodeTargetInvalid;
    }

    if (SbDecodeTargetIsValid(decode_target_to_release)) {
      DecodeTargetRelease(decode_target_graphics_context_provider_,
                          decode_target_to_release);
    }
  }
}

void VpxVideoDecoder::DecodeOneBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  SB_DCHECK(input_buffer);

  const auto& stream_info = input_buffer->video_stream_info();
  if (!context_ || stream_info.frame_size != current_frame_size_) {
    current_frame_size_ = stream_info.frame_size;
    TeardownCodec();
    InitializeCodec();
  }

  SB_DCHECK(context_);

  int64_t timestamp = input_buffer->timestamp();
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

  SB_DCHECK_EQ(vpx_image->stride[VPX_PLANE_U], vpx_image->stride[VPX_PLANE_V]);
  SB_DCHECK_LT(vpx_image->planes[VPX_PLANE_Y], vpx_image->planes[VPX_PLANE_U]);
  SB_DCHECK_LT(vpx_image->planes[VPX_PLANE_U], vpx_image->planes[VPX_PLANE_V]);

  if (vpx_image->stride[VPX_PLANE_U] != vpx_image->stride[VPX_PLANE_V] ||
      vpx_image->planes[VPX_PLANE_Y] >= vpx_image->planes[VPX_PLANE_U] ||
      vpx_image->planes[VPX_PLANE_U] >= vpx_image->planes[VPX_PLANE_V]) {
    ReportError("Unsupported yuv plane format.");
    return;
  }

  // Create a VideoFrame from decoded frame data. The data is in YV12 format.
  // Each component of a pixel takes one byte and they are in their own planes.
  // UV planes have half resolution both vertically and horizontally.
  scoped_refptr<CpuVideoFrame> frame = CpuVideoFrame::CreateYV12Frame(
      vpx_image->bit_depth, current_frame_size_.width,
      current_frame_size_.height, vpx_image->stride[VPX_PLANE_Y],
      vpx_image->stride[VPX_PLANE_U], timestamp, vpx_image->planes[VPX_PLANE_Y],
      vpx_image->planes[VPX_PLANE_U], vpx_image->planes[VPX_PLANE_V]);
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    std::lock_guard lock(decode_target_mutex_);
    frames_.push(frame);
  }

  Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, frame));
}

void VpxVideoDecoder::DecodeEndOfStream() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());

  // TODO: Flush the frames inside the decoder, though this is not required
  //       for vp9 in most cases.
  Schedule(
      std::bind(decoder_status_cb_, kBufferFull, VideoFrame::CreateEOSFrame()));
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget VpxVideoDecoder::GetCurrentDecodeTarget() {
  SB_DCHECK_EQ(output_mode_, kSbPlayerOutputModeDecodeToTexture);

  // We must take a lock here since this function can be called from a
  // separate thread.
  std::lock_guard lock(decode_target_mutex_);
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

}  // namespace starboard
