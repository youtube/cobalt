// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/libdav1d/dav1d_video_decoder.h"

#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/linux/shared/decode_target_internal.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/player/filter/cpu_video_frame.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "third_party/libdav1d/include/dav1d/common.h"
#include "third_party/libdav1d/include/dav1d/data.h"
#include "third_party/libdav1d/include/dav1d/headers.h"
#include "third_party/libdav1d/include/dav1d/picture.h"

namespace starboard {
namespace shared {
namespace libdav1d {

namespace {

using starboard::player::InputBuffer;
using starboard::player::JobThread;
using starboard::player::filter::CpuVideoFrame;

int AllocatePicture(Dav1dPicture* picture, void* context) {
  SB_DCHECK(picture);
  SB_DCHECK(context);

  VideoDecoder* decoder = static_cast<VideoDecoder*>(context);
  return decoder->AllocatePicture(picture);
}

void ReleasePicture(Dav1dPicture* picture, void* context) {
  SB_DCHECK(picture);
  SB_DCHECK(context);

  VideoDecoder* decoder = static_cast<VideoDecoder*>(context);
  decoder->ReleasePicture(picture);
}

void ReleaseInputBuffer(const uint8_t* buf, void* context) {
  SB_DCHECK(context);
  SB_DCHECK(buf);

  InputBuffer* input_buffer = static_cast<InputBuffer*>(context);
  SB_DCHECK(input_buffer->data() == buf);

  input_buffer->Release();
}

}  // namespace

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec,
                           SbPlayerOutputMode output_mode,
                           SbDecodeTargetGraphicsContextProvider*
                               decode_target_graphics_context_provider)
    : output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      decode_target_(kSbDecodeTargetInvalid) {
  SB_DCHECK(video_codec == kSbMediaVideoCodecAv1);
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
    ReportError("WriteInputBuffer() was called after WriteEndOfStream().");
    return;
  }

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("dav1d_video_decoder"));
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

  decoder_thread_->job_queue()->Schedule(std::bind(
      &VideoDecoder::DecodeEndOfStream, this, 100 * kSbTimeMillisecond));
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
  frames_being_decoded_ = 0;

  ScopedLock lock(decode_target_mutex_);
  frames_ = std::queue<scoped_refptr<CpuVideoFrame>>();
}

int VideoDecoder::AllocatePicture(Dav1dPicture* picture) const {
  SB_DCHECK(decoder_thread_);
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());
  SB_DCHECK(picture->data[0] == NULL);
  SB_DCHECK(picture->data[1] == NULL);
  SB_DCHECK(picture->data[2] == NULL);

  // dav1d requires that the allocated width and height is a multiple of 128.
  // NOTE: UV resolution is half that of Y resolution.
  int uv_width = (((picture->p.w + 127) / 128) * 128) / 2;
  int uv_height = (((picture->p.h + 127) / 128) * 128) / 2;
  // dav1d requires DAV1D_PICTURE_ALIGNMENT padded to allocated memory areas.
  int uv_size = uv_width * uv_height + DAV1D_PICTURE_ALIGNMENT;

  // This guarantees that the Y dimension is double the UV dimension.
  int y_width = uv_width * 2;
  int y_height = uv_height * 2;
  int y_size = y_width * y_height + DAV1D_PICTURE_ALIGNMENT;

  picture->stride[0] = y_width;
  picture->stride[1] = uv_width;

  void* ptr =
      SbMemoryAllocateAligned(DAV1D_PICTURE_ALIGNMENT, y_size + uv_size * 2);
  if (ptr == NULL) {
    return DAV1D_ERR(ENOMEM);
  }
  picture->data[0] = ptr;
  picture->data[1] = static_cast<uint8_t*>(ptr) + y_size;
  picture->data[2] = static_cast<uint8_t*>(ptr) + y_size + uv_size;
  return 0;
}

void VideoDecoder::ReleasePicture(Dav1dPicture* picture) const {
  SB_DCHECK(picture->data[0]);
  SB_DCHECK(picture->data[1]);
  SB_DCHECK(picture->data[2]);
  SB_DCHECK(picture->data[0] < picture->data[1]);
  SB_DCHECK(picture->data[1] < picture->data[2]);

  SbMemoryDeallocateAligned(picture->data[0]);
  for (int i = 0; i < 3; ++i) {
    picture->data[i] = NULL;
  }
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
  SB_DCHECK(dav1d_context_ == NULL);

  Dav1dSettings dav1d_settings{0};
  dav1d_default_settings(&dav1d_settings);
  // TODO: Verify this setting is optimal.
  dav1d_settings.n_threads = 8;

  Dav1dPicAllocator allocator;
  allocator.cookie = this;  // dav1d refers to context pointers as "cookie".
  allocator.alloc_picture_callback =
      &::starboard::shared::libdav1d::AllocatePicture;
  allocator.release_picture_callback =
      &::starboard::shared::libdav1d::ReleasePicture;
  dav1d_settings.allocator = allocator;

  int result = dav1d_open(&dav1d_context_, &dav1d_settings);
  if (result != kDav1dSuccess) {
    ReportError(FormatString("|dav1d_open| failed with code %d.", result));
    dav1d_context_ = NULL;
  }
}

void VideoDecoder::TeardownCodec() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  if (dav1d_context_) {
    dav1d_close(&dav1d_context_);
    dav1d_context_ = NULL;
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
  if (!dav1d_context_ || sample_info.frame_width != current_frame_width_ ||
      sample_info.frame_height != current_frame_height_) {
    current_frame_width_ = sample_info.frame_width;
    current_frame_height_ = sample_info.frame_height;
    TeardownCodec();
    InitializeCodec();
  }

  SB_DCHECK(dav1d_context_);

  Dav1dData dav1d_data;
  int result =
      dav1d_data_wrap(&dav1d_data, input_buffer->data(), input_buffer->size(),
                      &ReleaseInputBuffer, input_buffer.get());
  if (result != kDav1dSuccess) {
    ReportError(FormatString("|dav1d_data_wrap| failed with code %d.", result));
    return;
  }

  // Increment the refcount for |input_buffer| so that its data buffer persists.
  // This allows the buffer to be accessed by the dav1d decoder while decoding,
  // without it being destructed asynchronously.
  // When the dav1d decoder is done with the buffer, it will call the
  // ReleaseInputBuffer callback, which will call InputBuffer::Release(),
  // and decrement the reference count.
  input_buffer->AddRef();

  dav1d_data.m.timestamp = input_buffer->timestamp();

  // Sometimes |dav1d_send_data| can fail with EAGAIN error code. In this case,
  // we need to output frames from dav1d before continuing to send data packets
  // to it. The following loop retries sending data after outputting frames.
  for (int i = 0; i < 2; i++) {
    result = dav1d_send_data(dav1d_context_, &dav1d_data);
    if (result != kDav1dSuccess && result != DAV1D_ERR(EAGAIN)) {
      break;
    }
    if (result == kDav1dSuccess) {
      SB_DCHECK(dav1d_data.sz == 0);  // Check if all data has been consumed.
      ++frames_being_decoded_;
    }
    if (!TryToOutputFrames()) {
      return;
    }
    if (result == kDav1dSuccess) {
      Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, nullptr));
      return;
    }
  }
  // Encountered a fatal error in dav1d.
  dav1d_data_unref(&dav1d_data);
  ReportError(FormatString("|dav1d_send_data| failed with code %d.", result));
}

void VideoDecoder::DecodeEndOfStream(SbTime timeout) {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  if (!TryToOutputFrames()) {
    return;
  }
  if (frames_being_decoded_ > 0 && timeout > 0) {
    const SbTime delay_period = 5 * kSbTimeMillisecond;
    decoder_thread_->job_queue()->Schedule(
        std::bind(&VideoDecoder::DecodeEndOfStream, this,
                  timeout - delay_period),
        delay_period);
    return;
  } else {
    SB_LOG_IF(WARNING, frames_being_decoded_ > 0)
        << "Timed out waiting to output frames on end of stream.";
  }

  Schedule(
      std::bind(decoder_status_cb_, kBufferFull, VideoFrame::CreateEOSFrame()));
}

bool VideoDecoder::TryToOutputFrames() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  bool error_occurred = false;

  // Helper lambda to extract a frame from dav1d decoder.
  // Returns a nullptr if no frame is available, or an error occurred, or more
  // input is required.
  const auto get_frame_from_dav1d =
      [this, &error_occurred]() -> scoped_refptr<CpuVideoFrame> {
    error_occurred = false;
    Dav1dPicture dav1d_picture{0};
    int dav1d_result = dav1d_get_picture(dav1d_context_, &dav1d_picture);
    if (dav1d_result != kDav1dSuccess) {
      // NOTE: dav1d_get_pictures returns EAGAIN when a frame is not ready to be
      // decoded, or when more input data is needed.
      if (dav1d_result != DAV1D_ERR(EAGAIN)) {
        ReportError(FormatString("|dav1d_get_picture| failed with code %d.",
                                 dav1d_result));
        error_occurred = true;
      }
      return nullptr;
    }

    if (dav1d_picture.p.layout != DAV1D_PIXEL_LAYOUT_I420) {
      ReportError(FormatString("|dav1d_picture| has unexpected layout %d.",
                               static_cast<int>(dav1d_picture.p.layout)));
      dav1d_picture_unref(&dav1d_picture);
      error_occurred = true;
      return nullptr;
    }

    if (dav1d_picture.p.bpc != 8 && dav1d_picture.p.bpc != 10) {
      ReportError(FormatString("|dav1d_picture| has unexpected bitdepth %d.",
                               dav1d_picture.p.bpc));
      dav1d_picture_unref(&dav1d_picture);
      error_occurred = true;
      return nullptr;
    }

    SB_DCHECK(dav1d_picture.stride[kDav1dPlaneY] ==
              dav1d_picture.stride[kDav1dPlaneU] * 2);
    SB_DCHECK(dav1d_picture.data[kDav1dPlaneY] <
              dav1d_picture.data[kDav1dPlaneU]);
    SB_DCHECK(dav1d_picture.data[kDav1dPlaneU] <
              dav1d_picture.data[kDav1dPlaneV]);

    if (dav1d_picture.stride[kDav1dPlaneY] !=
            dav1d_picture.stride[kDav1dPlaneU] * 2 ||
        dav1d_picture.data[kDav1dPlaneY] >= dav1d_picture.data[kDav1dPlaneU] ||
        dav1d_picture.data[kDav1dPlaneU] >= dav1d_picture.data[kDav1dPlaneV]) {
      ReportError("Unsupported yuv plane format.");
      error_occurred = true;
      return nullptr;
    }

    scoped_refptr<CpuVideoFrame> frame = CpuVideoFrame::CreateYV12Frame(
        dav1d_picture.p.bpc, dav1d_picture.p.w, dav1d_picture.p.h,
        dav1d_picture.stride[0], dav1d_picture.m.timestamp,
        static_cast<uint8_t*>(dav1d_picture.data[0]),
        static_cast<uint8_t*>(dav1d_picture.data[1]),
        static_cast<uint8_t*>(dav1d_picture.data[2]));
    dav1d_picture_unref(&dav1d_picture);
    SB_DCHECK(!error_occurred);
    return frame;
  };

  auto frame = get_frame_from_dav1d();
  while (frame && !error_occurred) {
    SB_DCHECK(frames_being_decoded_ > 0);
    --frames_being_decoded_;
    if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
      ScopedLock lock(decode_target_mutex_);
      frames_.push(frame);
    }
    Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, frame));
    frame = get_frame_from_dav1d();
  }
  return true;
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

}  // namespace libdav1d
}  // namespace shared
}  // namespace starboard
