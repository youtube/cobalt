// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/libaom/aom_video_decoder.h"

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/linux/shared/decode_target_internal.h"
#include "starboard/shared/libaom/aom_library_loader.h"

namespace starboard {
namespace shared {
namespace aom {

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
  SB_DCHECK(video_codec == kSbMediaVideoCodecAv1);
  SB_DCHECK(is_aom_supported());
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

void VideoDecoder::WriteInputBuffers(const InputBuffers& input_buffers) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(decoder_status_cb_);

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("aom_video_decoder"));
    SB_DCHECK(decoder_thread_);
  }

  auto input_buffer = input_buffers[0];
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

  aom_codec_dec_cfg_t aom_config = {0};
  aom_config.threads = 8;
  aom_config.allow_lowbitdepth = 1;

  context_.reset(new aom_codec_ctx_t());
  aom_codec_err_t status =
      aom_codec_dec_init(context_.get(), aom_codec_av1_dx(), &aom_config, 0);
  if (status != AOM_CODEC_OK) {
    SB_LOG(ERROR) << "aom_codec_dec_init() failed with " << status;
    ReportError(
        FormatString("aom_codec_dec_init() failed with status %d.", status));
    context_.reset();
  }
}

void VideoDecoder::TeardownCodec() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  if (context_) {
    aom_codec_destroy(context_.get());
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

  const auto& stream_info = input_buffer->video_stream_info();
  if (!context_ || stream_info.frame_width != current_frame_width_ ||
      stream_info.frame_height != current_frame_height_) {
    current_frame_width_ = stream_info.frame_width;
    current_frame_height_ = stream_info.frame_height;
    TeardownCodec();
    InitializeCodec();
  }

  SB_DCHECK(context_);

  int64_t timestamp = input_buffer->timestamp();
  aom_codec_err_t status = aom_codec_decode(
      context_.get(), input_buffer->data(), input_buffer->size(), &timestamp);
  if (status != AOM_CODEC_OK) {
    SB_DLOG(ERROR) << "aom_codec_decode() failed, status=" << status;
    ReportError(
        FormatString("aom_codec_decode() failed with status %d.", status));
    return;
  }

  // Gets pointer to decoded data.
  aom_codec_iter_t dummy = NULL;
  const aom_image_t* aom_image = aom_codec_get_frame(context_.get(), &dummy);
  if (!aom_image) {
    return;
  }

  if (aom_image->user_priv != &timestamp) {
    SB_DLOG(ERROR) << "Invalid output timestamp.";
    ReportError("Invalid output timestamp.");
    return;
  }

  if (aom_image->fmt != AOM_IMG_FMT_YV12) {
    SB_DCHECK(aom_image->fmt == AOM_IMG_FMT_I420 ||
              aom_image->fmt == AOM_IMG_FMT_I42016)
        << "Unsupported aom_image->fmt: " << aom_image->fmt;
    if (aom_image->fmt != AOM_IMG_FMT_I420 &&
        aom_image->fmt != AOM_IMG_FMT_I42016) {
      ReportError(
          FormatString("Unsupported aom_image->fmt: %d.", aom_image->fmt));
      return;
    }
  }

  if (aom_image->bit_depth != 8 && aom_image->bit_depth != 10 &&
      aom_image->bit_depth != 12) {
    SB_DLOG(ERROR) << "Unsupported bit depth " << aom_image->bit_depth;
    ReportError(
        FormatString("Unsupported bit depth %d.", aom_image->bit_depth));
    return;
  }

  SB_DCHECK(aom_image->stride[AOM_PLANE_U] == aom_image->stride[AOM_PLANE_V]);
  SB_DCHECK(aom_image->planes[AOM_PLANE_Y] < aom_image->planes[AOM_PLANE_U]);
  SB_DCHECK(aom_image->planes[AOM_PLANE_U] < aom_image->planes[AOM_PLANE_V]);

  if (aom_image->stride[AOM_PLANE_U] != aom_image->stride[AOM_PLANE_V] ||
      aom_image->planes[AOM_PLANE_Y] >= aom_image->planes[AOM_PLANE_U] ||
      aom_image->planes[AOM_PLANE_U] >= aom_image->planes[AOM_PLANE_V]) {
    ReportError("Unsupported yuv plane format.");
    return;
  }

  // Create a VideoFrame from decoded frame data. The data is in YV12 format.
  // Each component of a pixel takes one byte and they are in their own planes.
  // UV planes have half resolution both vertically and horizontally.
  scoped_refptr<CpuVideoFrame> frame = CpuVideoFrame::CreateYV12Frame(
      aom_image->bit_depth, current_frame_width_, current_frame_height_,
      aom_image->stride[AOM_PLANE_Y], aom_image->stride[AOM_PLANE_U], timestamp,
      aom_image->planes[AOM_PLANE_Y], aom_image->planes[AOM_PLANE_U],
      aom_image->planes[AOM_PLANE_V]);
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

}  // namespace aom
}  // namespace shared
}  // namespace starboard
