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

#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/queue.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/video_frame_internal.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class VideoDecoder {
 public:
  enum Status { kNeedMoreInput, kBufferFull, kFatalError };

  typedef starboard::player::InputBuffer InputBuffer;
  typedef shared::starboard::VideoFrame Frame;
  // |frame| can contain a decoded frame or be NULL when |status| is not
  // kFatalError.   When status is kFatalError, |frame| will be NULL.  Its user
  // should only call WriteInputFrame() when |status| is kNeedMoreInput or when
  // the instance is just created.  Also note that calling Reset() or dtor from
  // this callback will result in deadlock.
  typedef void (*DecoderStatusFunc)(Status status, Frame* frame, void* context);

  VideoDecoder(SbMediaVideoCodec video_codec,
               DecoderStatusFunc decoder_status_func,
               void* context);
  ~VideoDecoder();

  // After this call, the VideoDecoder instance owns |input_buffer|.
  void WriteInputBuffer(InputBuffer* input_buffer);
  void WriteEndOfStream();
  // Clear any cached buffer of the codec and reset the state of the codec.
  // This function will be called during seek to ensure that there is no left
  // over data from previous buffers.  No DecoderStatusFunc call will be made
  // after this function returns unless WriteInputFrame() or WriteEndOfStream()
  // is called again.
  void Reset();

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
    InputBuffer* input_buffer;

    explicit Event(EventType type = kInvalid, InputBuffer* input_buffer = NULL)
        : type(type), input_buffer(input_buffer) {
      if (input_buffer) {
        SB_DCHECK(type == kWriteInputBuffer);
      } else {
        SB_DCHECK(type != kWriteInputBuffer);
      }
    }
  };

  static void* ThreadEntryPoint(void* context);
  void DecoderThreadFunc();

  bool DecodePacket(AVPacket* packet);
  void InitializeCodec();
  void TeardownCodec();

  // These variables will be initialized inside ctor and will not be changed
  // during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  const DecoderStatusFunc decoder_status_func_;
  void* const context_;

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

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_
