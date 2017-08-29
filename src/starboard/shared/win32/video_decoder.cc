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

#include "starboard/shared/win32/video_decoder.h"

#include "starboard/decode_target.h"
#include "starboard/log.h"
#include "starboard/shared/win32/decode_target_internal.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"

namespace starboard {
namespace shared {
namespace win32 {

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec,
                           SbPlayerOutputMode output_mode,
                           SbDecodeTargetGraphicsContextProvider*
                               decode_target_graphics_context_provider,
                           SbDrmSystem drm_system)
    : video_codec_(video_codec),
      drm_system_(drm_system),
      host_(NULL),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider) {
  impl_ = AbstractWin32VideoDecoder::Create(video_codec_, drm_system_);
  video_decoder_thread_.reset(new VideoDecoderThread(impl_.get(), this));
}

VideoDecoder::~VideoDecoder() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  video_decoder_thread_.reset(nullptr);
  impl_.reset(nullptr);
}

void VideoDecoder::SetHost(Host* host) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(host != NULL);
  SB_DCHECK(host_ == NULL);
  host_ = host;
}

void VideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(host_);
  const bool can_accept_more_input =
      video_decoder_thread_->QueueInput(input_buffer);

  if (can_accept_more_input) {
    host_->OnDecoderStatusUpdate(kNeedMoreInput, NULL);
  }
}

void VideoDecoder::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(host_);
  scoped_refptr<InputBuffer> empty;
  video_decoder_thread_->QueueInput(empty);
}

void VideoDecoder::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(host_);
  video_decoder_thread_.reset(nullptr);
  impl_->Reset();
  decoded_data_.Clear();
  video_decoder_thread_.reset(new VideoDecoderThread(impl_.get(), this));
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget VideoDecoder::GetCurrentDecodeTarget() {
  SB_NOTIMPLEMENTED()
      << "VideoRendererImpl::GetCurrentDecodeTarget() should be used instead.";
  return kSbDecodeTargetInvalid;
}

void VideoDecoder::OnVideoDecoded(VideoFramePtr data) {
  Status sts = (data && data->IsEndOfStream()) ? kBufferFull : kNeedMoreInput;
  host_->OnDecoderStatusUpdate(sts, data);
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// static
bool VideoDecoder::OutputModeSupported(SbPlayerOutputMode output_mode,
                                       SbMediaVideoCodec codec,
                                       SbDrmSystem drm_system) {
  SB_UNREFERENCED_PARAMETER(codec);
  SB_UNREFERENCED_PARAMETER(drm_system);
  return output_mode == kSbPlayerOutputModeDecodeToTexture;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
