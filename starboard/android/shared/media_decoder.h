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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_DECODER_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_DECODER_H_

#include <jni.h>

#include <deque>
#include <queue>

#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/media_codec_bridge.h"
#include "starboard/atomic.h"
#include "starboard/common/optional.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/closure.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace android {
namespace shared {

// TODO: Better encapsulation the MediaCodecBridge so the decoders no longer
//       need to talk directly to the MediaCodecBridge.
class MediaDecoder {
 public:
  typedef ::starboard::shared::starboard::player::Closure Closure;
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;

  // This class should be implemented by the users of MediaDecoder to receive
  // various notifications.  Note that all such functions are called on the
  // decoder thread.
  class Host {
   public:
    virtual void ProcessOutputBuffer(MediaCodecBridge* media_codec_bridge,
                                     const DequeueOutputResult& output) = 0;
    virtual void RefreshOutputFormat(MediaCodecBridge* media_codec_bridge) = 0;
    // This function gets called immediately after ProcessOutputBuffer() or
    // RefreshOutputFormat() is called to give the Host (especially the video
    // decoder) a chance to process.
    // TODO: Revise the scheduling logic to give the host a chance to process in
    //       a more elegant way.
    virtual bool Tick(MediaCodecBridge* media_codec_bridge) = 0;
    // This function gets called before calling Flush() on the contained
    // MediaCodecBridge so the host can have a chance to do necessary cleanups
    // before the MediaCodecBridge is flushed.
    virtual void OnFlushing() = 0;

   protected:
    ~Host() {}
  };

  MediaDecoder(Host* host,
               SbMediaAudioCodec audio_codec,
               const SbMediaAudioHeader& audio_header,
               SbDrmSystem drm_system);
  MediaDecoder(Host* host,
               SbMediaVideoCodec video_codec,
               int width,
               int height,
               jobject j_output_surface,
               SbDrmSystem drm_system,
               const SbMediaColorMetadata* color_metadata);
  ~MediaDecoder();

  void Initialize(const Closure& error_cb);
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer);
  void WriteEndOfStream();

  size_t GetNumberOfPendingTasks() const { return event_queue_.size(); }

  bool is_valid() const { return media_codec_bridge_ != NULL; }

 private:
  struct Event {
    enum Type {
      kInvalid,
      kWriteCodecConfig,
      kWriteInputBuffer,
      kWriteEndOfStream,
    };

    explicit Event(Type type = kInvalid) : type(type) {
      SB_DCHECK(type != kWriteInputBuffer && type != kWriteCodecConfig);
    }
    Event(const int8_t* codec_config, int16 codec_config_size)
        : type(kWriteCodecConfig),
          codec_config(codec_config),
          codec_config_size(codec_config_size) {}
    explicit Event(const scoped_refptr<InputBuffer>& input_buffer)
        : type(kWriteInputBuffer), input_buffer(input_buffer) {}

    Type type;
    scoped_refptr<InputBuffer> input_buffer;
    const int8_t* codec_config;
    int16 codec_config_size;
  };

  struct QueueInputBufferTask {
    DequeueInputResult dequeue_input_result;
    Event event;
  };

  static void* ThreadEntryPoint(void* context);
  void DecoderThreadFunc();
  void JoinOnDecoderThread();

  void TeardownCodec();

  bool ProcessOneInputBuffer(std::deque<Event>* pending_work);
  // Attempt to dequeue a media codec output buffer.  Returns whether the
  // processing should continue.  If a valid buffer is dequeued, it will call
  // ProcessOutputBuffer() on host internally.  It is the responsibility of
  // ProcessOutputBuffer() to release the output buffer back to the system.
  bool DequeueAndProcessOutputBuffer();

  void HandleError(const char* action_name, jint status);

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  const SbMediaType media_type_;
  Host* host_;
  Closure error_cb_;

  // Working thread to avoid lengthy decoding work block the player thread.
  SbThread decoder_thread_;
  scoped_ptr<MediaCodecBridge> media_codec_bridge_;

  bool stream_ended_;

  DrmSystem* drm_system_;

  // Events are processed in a queue, except for when handling events of type
  // |kReset|, which are allowed to cut to the front.
  EventQueue<Event> event_queue_;
  atomic_bool destroying_;

  optional<QueueInputBufferTask> pending_queue_input_buffer_task_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_DECODER_H_
