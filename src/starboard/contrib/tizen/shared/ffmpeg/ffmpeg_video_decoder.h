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

#ifndef STARBOARD_CONTRIB_TIZEN_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_
#define STARBOARD_CONTRIB_TIZEN_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_

#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/queue.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/thread.h"
#include "starboard/contrib/tizen/shared/ffmpeg/ffmpeg_common.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class VideoDecoder : public starboard::player::filter::HostedVideoDecoder {
 public:
  typedef starboard::player::InputBuffer InputBuffer;
  typedef starboard::player::VideoFrame VideoFrame;

  explicit VideoDecoder(SbMediaVideoCodec video_codec);
  ~VideoDecoder() override;

  void SetHost(Host* host) override;
  void WriteInputBuffer(const InputBuffer& input_buffer) override;
  void WriteEndOfStream() override;
  void Reset() override;

  bool is_valid() const { return codec_context_ != NULL; }

 private:
  enum EventType {
    kInvalid,
    kReset,
    kWriteInputBuffer,
    kWriteEndOfStream,
  };

  struct Event {
    EventType type;
    // |input_buffer| is only used when |type| is kWriteInputBuffer.
    InputBuffer input_buffer;

    explicit Event(EventType type = kInvalid) : type(type) {
      SB_DCHECK(type != kWriteInputBuffer);
    }

    explicit Event(InputBuffer input_buffer)
        : type(kWriteInputBuffer), input_buffer(input_buffer) {}
  };

  static void* ThreadEntryPoint(void* context);
  void DecoderThreadFunc();

  bool DecodePacket(AVPacket* packet);
  void InitializeCodec();
  void TeardownCodec();

  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  Host* host_;

  Queue<Event> queue_;

  // The AV related classes will only be created and accessed on the decoder
  // thread.
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  bool stream_ended_;
  bool error_occured_;

  // Working thread to avoid lengthy decoding work block the player thread.
  SbThread decoder_thread_;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_CONTRIB_TIZEN_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_
