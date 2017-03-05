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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_DECODER_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_DECODER_H_

#include <jni.h>

#include <deque>

#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_codec_bridge.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/decode_target.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/queue.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/thread.h"

namespace starboard {
namespace android {
namespace shared {

class VideoDecoder
    : public ::starboard::shared::starboard::player::filter::VideoDecoder {
 public:
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;
  typedef ::starboard::shared::starboard::player::VideoFrame VideoFrame;

  explicit VideoDecoder(SbMediaVideoCodec video_codec,
                        SbPlayerOutputMode output_mode,
                        SbDecodeTargetProvider* decode_target_provider);
  ~VideoDecoder() SB_OVERRIDE;

  void SetHost(Host* host) SB_OVERRIDE;
  void WriteInputBuffer(const InputBuffer& input_buffer) SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;
  void SetCurrentTime(SbMediaTime current_time) SB_OVERRIDE {
    current_time_ = ConvertSbMediaTimeToMicroseconds(current_time);
    current_time_set_at_ = SbTimeGetNow();
  }

  SbDecodeTarget GetCurrentDecodeTarget() SB_OVERRIDE;

  // Get |current_time_| accounting for the time that has elapsed since it was
  // last set.
  SbTime GetAdjustedCurrentTime() const {
    return current_time_ + SbTimeGetNow() - current_time_set_at_;
  }
  bool is_valid() const { return media_codec_bridge_ != NULL; }

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

  // Pending work can be either an input buffer or EOS flag to enqueue.
  struct InputBufferOrEos {
    struct CreateEos {};
    InputBufferOrEos() = delete;
    InputBufferOrEos(const CreateEos&) : is_eos(true) {}
    InputBufferOrEos(const InputBuffer& input_buffer)
        : input_buffer(input_buffer), is_eos(false) {}
    InputBuffer input_buffer;
    bool is_eos;
  };

  static void* ThreadEntryPoint(void* context);
  void DecoderThreadFunc();

  // Attempt to initialize the codec.  Returns whether initialization was
  // successful.
  bool InitializeCodec();
  void TeardownCodec();

  // Attempt to enqueue the front of |pending_work_| into a MediaCodec input
  // buffer.  Returns true if a buffer was queued.
  bool ProcessOneInputBuffer();
  // Attempt to dequeue a media codec output buffer.  Returns whether a buffer
  // was dequeued.
  bool ProcessOneOutputBuffer();

  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  Host* host_;

  Queue<Event> event_queue_;

  bool stream_ended_;
  bool error_occured_;

  // Working thread to avoid lengthy decoding work block the player thread.
  SbThread decoder_thread_;

  scoped_ptr<MediaCodecBridge> media_codec_bridge_;

  SbTime current_time_;
  SbTime current_time_set_at_;

  std::deque<InputBufferOrEos> pending_work_;

  SbPlayerOutputMode output_mode_;

  SbDecodeTargetProvider* decode_target_provider_;
  // If decode-to-texture is enabled, then we store the decode target texture
  // inside of this |decode_target_| member.
  SbDecodeTarget decode_target_;

  // Since GetCurrentDecodeTarget() needs to be called from an arbitrary thread
  // to obtain the current decode target (which ultimately ends up being a
  // copy of |decode_target_|), we need to safe-guard access to |decode_target_|
  // and we do so through this mutex.
  starboard::Mutex decode_target_mutex_;

  // The width and height of the latest decoded frame.
  int32_t frame_width_;
  int32_t frame_height_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_DECODER_H_
