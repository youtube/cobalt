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

#include "starboard/shared/libde265/de265_video_decoder.h"

#include "starboard/common/check_op.h"
#include "starboard/common/string.h"
#include "starboard/linux/shared/decode_target_internal.h"
#include "starboard/shared/libde265/de265_library_loader.h"
#include "starboard/thread.h"

namespace starboard {

De265VideoDecoder::De265VideoDecoder(
    JobQueue* job_queue,
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider)
    : JobOwner(job_queue),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider) {
  SB_DCHECK_EQ(video_codec, kSbMediaVideoCodecH265);
  SB_DCHECK(is_de265_supported());
}

De265VideoDecoder::~De265VideoDecoder() {
  SB_CHECK(BelongsToCurrentThread());
  Reset();
}

void De265VideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                                   const ErrorCB& error_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;
}

void De265VideoDecoder::WriteInputBuffers(const InputBuffers& input_buffers) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK_EQ(input_buffers.size(), 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(decoder_status_cb_);

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("de265_video_decoder"));
    SB_DCHECK(decoder_thread_);
  }

  const auto& input_buffer = input_buffers[0];
  decoder_thread_->Schedule(
      std::bind(&De265VideoDecoder::DecodeOneBuffer, this, input_buffer));
}

void De265VideoDecoder::WriteEndOfStream() {
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
      std::bind(&De265VideoDecoder::DecodeEndOfStream, this));
}

void De265VideoDecoder::Reset() {
  SB_CHECK(BelongsToCurrentThread());

  if (decoder_thread_) {
    // Wait to ensure all tasks are done before decoder_thread_ reset.
    decoder_thread_->ScheduleAndWait(
        std::bind(&De265VideoDecoder::TeardownCodec, this));

    decoder_thread_.reset();
  }

  error_occurred_ = false;
  stream_ended_ = false;

  CancelPendingJobs();

  std::lock_guard lock(decode_target_mutex_);
  frames_ = std::queue<scoped_refptr<CpuVideoFrame>>();
}

void De265VideoDecoder::UpdateDecodeTarget_Locked(
    const scoped_refptr<CpuVideoFrame>& frame) {
  SbDecodeTarget decode_target = DecodeTargetCreate(
      decode_target_graphics_context_provider_, frame, decode_target_);

  // Lock only after the post to the renderer thread, to prevent deadlock.
  decode_target_ = decode_target;

  if (!SbDecodeTargetIsValid(decode_target)) {
    SB_LOG(ERROR) << "Could not acquire a decode target from provider.";
  }
}

void De265VideoDecoder::ReportError(const std::string& error_message) {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());

  error_occurred_ = true;
  Schedule(std::bind(error_cb_, kSbPlayerErrorDecode, error_message));
}

void De265VideoDecoder::InitializeCodec() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  SB_DCHECK(!context_);

  context_ = de265_new_decoder();
  SB_DCHECK(context_);

  const int kNumberOfThreads = 8;
  de265_error error = de265_start_worker_threads(context_, kNumberOfThreads);
  SB_DCHECK_EQ(error, DE265_OK);
}

void De265VideoDecoder::TeardownCodec() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());

  if (context_) {
    de265_error error = de265_free_decoder(context_);
    SB_DCHECK_EQ(error, DE265_OK);
    context_ = nullptr;
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

void De265VideoDecoder::DecodeOneBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());

  if (!context_) {
    InitializeCodec();
  }

  SB_DCHECK(context_);

  int64_t timestamp = input_buffer->timestamp();
  de265_error status = de265_push_data(context_, input_buffer->data(),
                                       input_buffer->size(), timestamp, 0);
  if (status != DE265_OK) {
    SB_DLOG(ERROR) << "de265_push_data() failed, status=" << status;
    ReportError(
        FormatString("de265_push_data() failed with status %d.", status));
    return;
  }

  ProcessDecodedImage(false);
}

void De265VideoDecoder::DecodeEndOfStream() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());

  auto status = de265_flush_data(context_);
  SB_DCHECK_EQ(status, DE265_OK);

  ProcessDecodedImage(true);
}

void De265VideoDecoder::ProcessDecodedImage(bool flushing) {
  int more;
  auto status = de265_decode(context_, &more);
  if (status == DE265_OK && more) {
    // Produced decoded image
  } else if (status != DE265_OK && more) {
    SB_DCHECK(!flushing);
    Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, nullptr));
    return;
  } else if (status == DE265_OK && !more) {
    // End of stream
    Schedule(std::bind(decoder_status_cb_, kBufferFull,
                       VideoFrame::CreateEOSFrame()));
    return;
  } else {
    SB_DLOG(ERROR) << "de265_decode() failed, status=" << status;
    ReportError(FormatString("de265_decode() failed with status %d.", status));
    return;
  }

  const de265_image* image = de265_get_next_picture(context_);
  if (!image) {
    if (flushing) {
      Schedule(std::bind(&De265VideoDecoder::ProcessDecodedImage, this, true));
    } else {
      Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, nullptr));
    }
    return;
  }

  de265_chroma chroma = de265_get_chroma_format(image);
  if (chroma != de265_chroma_420) {
    SB_DLOG(ERROR) << "Invalid chroma format " << chroma;
    ReportError(FormatString("Invalid chroma format %d.", chroma));
    return;
  }
  enum { kYPlane, kUPlane, kVPlane, kImagePlanes };
  int widths[kImagePlanes], heights[kImagePlanes];
  const uint8_t* planes[kImagePlanes] = {};
  int strides[kImagePlanes];

  auto bit_depth = de265_get_bits_per_pixel(image, 0);
  if (bit_depth != 8 && bit_depth != 10 && bit_depth != 12) {
    SB_DLOG(ERROR) << "Unsupported bit depth " << bit_depth;
    ReportError(FormatString("Unsupported bit depth %d.", bit_depth));
    return;
  }

  for (int i = 0; i < kImagePlanes; ++i) {
    SB_DCHECK_EQ(bit_depth, de265_get_bits_per_pixel(image, i));

    widths[i] = de265_get_image_width(image, i);
    heights[i] = de265_get_image_height(image, i);
    planes[i] = de265_get_image_plane(image, i, strides + i);
    SB_DCHECK(planes[i]);
  }

  SB_DCHECK_EQ(widths[kYPlane], widths[kUPlane] * 2);
  SB_DCHECK_EQ(widths[kUPlane], widths[kVPlane]);
  SB_DCHECK_EQ(strides[kUPlane], strides[kVPlane]);

  // Create a VideoFrame from decoded frame data. The data is in YV12 format.
  // Each component of a pixel takes one byte and they are in their own planes.
  // UV planes have half resolution both vertically and horizontally.
  scoped_refptr<CpuVideoFrame> frame = CpuVideoFrame::CreateYV12Frame(
      bit_depth, widths[kYPlane], heights[kYPlane], strides[kYPlane],
      strides[kUPlane], de265_get_image_PTS(image), planes[kYPlane],
      planes[kUPlane], planes[kVPlane]);
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    std::lock_guard lock(decode_target_mutex_);
    frames_.push(frame);
  }

  if (flushing) {
    Schedule(std::bind(decoder_status_cb_, kBufferFull, frame));
    Schedule(std::bind(&De265VideoDecoder::ProcessDecodedImage, this, true));
  } else {
    Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, frame));
  }
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget De265VideoDecoder::GetCurrentDecodeTarget() {
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
