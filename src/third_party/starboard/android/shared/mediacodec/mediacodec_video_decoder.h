// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef STARBOARD_SHARED_MEDIACODEC_MEDIACODEC_VIDEO_DECODER_H_
#define STARBOARD_SHARED_MEDIACODEC_MEDIACODEC_VIDEO_DECODER_H_

#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/queue.h"
#include "third_party/starboard/android/shared/mediacodec/mediacodec_common.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/thread.h"

#include <media/NdkMediaCodec.h>

namespace starboard {
namespace shared {
namespace mediacodec {

class VideoDecoder : public starboard::player::filter::VideoDecoder {
 public:
  typedef starboard::player::InputBuffer InputBuffer;
  typedef starboard::player::VideoFrame VideoFrame;

  explicit VideoDecoder(SbMediaVideoCodec);
  ~VideoDecoder() SB_OVERRIDE;

  void SetHost(Host* host) SB_OVERRIDE;
  void WriteInputBuffer(const InputBuffer& input_buffer) SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;

  bool is_valid() const { return mediacodec_ != NULL; }

 private:
  ANativeWindow* vwindow_;
  AMediaCodec* mediacodec_;
  AMediaFormat* mediaformat_;

  bool GetMimeTypeFromCodecType(char *out, SbMediaVideoCodec type);

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

  bool InputBufferWork(Event event);
  SbMediaTime OutputBufferWork();

  void InitializeCodec();
  void TeardownCodec();

  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  Host* host_;

  Queue<Event> queue_;

  bool stream_ended_;
  bool error_occured_;

  // Working thread to avoid lengthy decoding work block the player thread.
  SbThread decoder_thread_;
};

}  // namespace mediacodec
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_MEDIACODEC_MEDIACODEC_VIDEO_DECODER_H_
