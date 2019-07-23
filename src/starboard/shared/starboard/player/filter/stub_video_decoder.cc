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

#include "starboard/shared/starboard/player/filter/stub_video_decoder.h"

#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

void StubVideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                                  const ErrorCB& error_cb) {
  SB_UNREFERENCED_PARAMETER(error_cb);
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  decoder_status_cb_ = decoder_status_cb;
}

size_t StubVideoDecoder::GetPrerollFrameCount() const {
  return 1;
}

SbTime StubVideoDecoder::GetPrerollTimeout() const {
  return kSbTimeMax;
}

size_t StubVideoDecoder::GetMaxNumberOfCachedFrames() const {
  return 12;
}

void StubVideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(input_buffer);

  auto& video_sample_info = input_buffer->video_sample_info();
  if (video_sample_info.is_key_frame) {
    if (!video_sample_info_ ||
        video_sample_info_.value() != video_sample_info) {
      SB_LOG(INFO) << "New video sample info: " << video_sample_info;
      video_sample_info_ = video_sample_info;
    }
  }

  decoder_status_cb_(kNeedMoreInput, new VideoFrame(input_buffer->timestamp()));
}

void StubVideoDecoder::WriteEndOfStream() {
  decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
}

void StubVideoDecoder::Reset() {}

SbDecodeTarget StubVideoDecoder::GetCurrentDecodeTarget() {
  return kSbDecodeTargetInvalid;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
