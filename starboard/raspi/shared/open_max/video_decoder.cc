// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/raspi/shared/open_max/video_decoder.h"

#include "starboard/log.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

using starboard::shared::starboard::player::VideoFrame;

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec)
    : host_(NULL), stream_ended_(false) {
  SB_DCHECK(video_codec == kSbMediaVideoCodecH264);

  component_.Start();
}

VideoDecoder::~VideoDecoder() {}

void VideoDecoder::SetHost(Host* host) {
  SB_DCHECK(host != NULL);
  SB_DCHECK(host_ == NULL);
  host_ = host;
}

void VideoDecoder::WriteInputBuffer(const InputBuffer& input_buffer) {
  SB_DCHECK(host_ != NULL);

  if (stream_ended_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after WriteEndOfStream().";
    return;
  }
  component_.WriteData(input_buffer.data(), input_buffer.size(),
                       input_buffer.pts() * kSbTimeSecond / kSbMediaTimeSecond);
  if (scoped_refptr<VideoFrame> frame = component_.ReadVideoFrame()) {
    host_->OnDecoderStatusUpdate(kNeedMoreInput, frame);
  } else {
    // Call the callback with NULL frame to ensure that the host know that more
    // data is expected.
    host_->OnDecoderStatusUpdate(kNeedMoreInput, NULL);
  }
}

void VideoDecoder::WriteEndOfStream() {
  SB_DCHECK(!stream_ended_);
  stream_ended_ = true;
  component_.WriteEOS();
}

void VideoDecoder::Reset() {
  stream_ended_ = false;
  component_.Flush();
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi

namespace shared {
namespace starboard {
namespace player {
namespace filter {

// static
VideoDecoder* VideoDecoder::Create(SbMediaVideoCodec video_codec) {
  return new raspi::shared::open_max::VideoDecoder(video_codec);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared

}  // namespace starboard
