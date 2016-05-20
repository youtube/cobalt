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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_

#include <queue>

#include "starboard/media.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/video_frame_internal.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class VideoDecoder {
 public:
  // FFmpeg requires its decoding buffers to align with platform alignment.  It
  // mentions inside
  // http://ffmpeg.org/doxygen/trunk/structAVFrame.html#aa52bfc6605f6a3059a0c3226cc0f6567
  // that the alignment on most modern desktop systems are 16 or 32.
  static const int kAlignment = 32;

  typedef shared::starboard::VideoFrame Frame;

  explicit VideoDecoder(SbMediaVideoCodec video_codec);
  ~VideoDecoder();

  bool is_valid() { return codec_context_ != NULL; }
  void WriteEncodedFrame(const void* sample_buffer,
                         int sample_buffer_size,
                         SbMediaTime sample_pts);
  void WriteEndOfStream();
  bool ReadDecodedFrame(Frame* frame);
  // Clear any cached buffer of the codec and reset the state of the codec.
  // This function will be called during seek to ensure that the left over
  // data from previous buffers are cleared.
  void Reset();

 private:
  void DecodePacket(AVPacket* packet);
  void InitializeCodec();
  void TeardownCodec();

  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  bool stream_ended_;

  std::queue<Frame> frames_;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_
