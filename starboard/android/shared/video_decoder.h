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

#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_codec_bridge.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/common/optional.h"
#include "starboard/common/ref_counted.h"
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

class VideoRenderer;

class VideoDecoder
    : public ::starboard::shared::starboard::player::filter::VideoDecoder {
 public:
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;
  typedef ::starboard::shared::starboard::player::VideoFrame VideoFrame;

  VideoDecoder(SbMediaVideoCodec video_codec,
               SbDrmSystem drm_system,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider*
                   decode_target_graphics_context_provider);
  ~VideoDecoder() SB_OVERRIDE;

  void Initialize(const Closure& error_cb) SB_OVERRIDE;
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer)
      SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;
  SbDecodeTarget GetCurrentDecodeTarget() SB_OVERRIDE;

  void SetHost(VideoRenderer* host);
  bool is_valid() const { return media_codec_bridge_ != NULL; }

 private:
  struct Event {
    enum Type {
      kInvalid,
      kReset,
      kWriteInputBuffer,
      kWriteEndOfStream,
    };

    Type type;
    // |input_buffer| is only used when |type| is |kWriteInputBuffer|.
    scoped_refptr<InputBuffer> input_buffer;

    explicit Event(Type type = kInvalid) : type(type) {
      SB_DCHECK(type != kWriteInputBuffer);
    }

    explicit Event(const scoped_refptr<InputBuffer>& input_buffer)
        : type(kWriteInputBuffer), input_buffer(input_buffer) {}
  };

  struct QueueInputBufferTask {
    DequeueInputResult dequeue_input_result;
    Event event;
  };

  static void* ThreadEntryPoint(void* context);
  void DecoderThreadFunc();
  void JoinOnDecoderThread();

  // Attempt to initialize the codec.  Returns whether initialization was
  // successful.
  bool InitializeCodec();
  void TeardownCodec();

  // Attempt to enqueue the front of |pending_work| into a MediaCodec input
  // buffer.  Returns true if a buffer was queued.
  bool ProcessOneInputBuffer(std::deque<Event>* pending_work);
  // Attempt to dequeue a media codec output buffer.  Returns whether the
  // processing should continue.  If a valid buffer is dequeued, it will call
  // ProcessOutputBuffer() internally.  It is the responsibility of
  // ProcessOutputBuffer() to release the output buffer back to the system.
  bool DequeueAndProcessOutputBuffer();
  void ProcessOutputBuffer(const DequeueOutputResult& output);
  void RefreshOutputFormat();
  void HandleError(const char* action_name, jint status);

  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  Closure error_cb_;
  VideoRenderer* host_;
  DrmSystem* drm_system_;

  // Events are processed in a queue, except for when handling events of type
  // |kReset|, which are allowed to cut to the front.
  EventQueue<Event> event_queue_;

  bool stream_ended_;

  // Working thread to avoid lengthy decoding work block the player thread.
  SbThread decoder_thread_;

  scoped_ptr<MediaCodecBridge> media_codec_bridge_;

  SbPlayerOutputMode output_mode_;

  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;

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

  optional<QueueInputBufferTask> pending_queue_input_buffer_task_;

  // The last enqueued |SbMediaColorMetadata|.
  optional<SbMediaColorMetadata> previous_color_metadata_;

  // Helper value to keep track of whether we have enqueued our first input
  // buffer or not.  This is used to decide whether or not we need to
  // reinitialize upon receiving the first input buffer, since HDR metadata is
  // unforunately not provided to us at initialization time.
  bool has_written_buffer_since_reset_;

  // A queue of media codec output buffers that we have taken from the media
  // codec bridge.  It is only accessed from the |decoder_thread_|.
  std::deque<DequeueOutputResult> dequeue_output_results_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_DECODER_H_
