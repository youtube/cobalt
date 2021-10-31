// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include <string>
#include <vector>

#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/media_codec_bridge.h"
#include "starboard/atomic.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/common/optional.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace android {
namespace shared {

// TODO: Better encapsulation the MediaCodecBridge so the decoders no longer
//       need to talk directly to the MediaCodecBridge.
class MediaDecoder : private MediaCodecBridge::Handler {
 public:
  typedef ::starboard::shared::starboard::player::filter::ErrorCB ErrorCB;
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;
  typedef std::function<void(SbTime)> FrameRenderedCB;

  // This class should be implemented by the users of MediaDecoder to receive
  // various notifications.  Note that all such functions are called on the
  // decoder thread.
  // TODO: Replace this with std::function<> based callbacks.
  class Host {
   public:
    virtual void ProcessOutputBuffer(MediaCodecBridge* media_codec_bridge,
                                     const DequeueOutputResult& output) = 0;
    virtual void OnEndOfStreamWritten(MediaCodecBridge* media_codec_bridge) = 0;
    virtual void RefreshOutputFormat(MediaCodecBridge* media_codec_bridge) = 0;
    // This function gets called frequently on the decoding thread to give the
    // Host a chance to process when the MediaDecoder is decoding.
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
               const SbMediaAudioSampleInfo& audio_sample_info,
               SbDrmSystem drm_system);
  MediaDecoder(Host* host,
               SbMediaVideoCodec video_codec,
               int width,
               int height,
               int fps,
               jobject j_output_surface,
               SbDrmSystem drm_system,
               const SbMediaColorMetadata* color_metadata,
               bool require_software_codec,
               const FrameRenderedCB& frame_rendered_cb,
               int tunnel_mode_audio_session_id,
               bool force_big_endian_hdr_metadata,
               std::string* error_message);
  ~MediaDecoder();

  void Initialize(const ErrorCB& error_cb);
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer);
  void WriteEndOfStream();

  void SetPlaybackRate(double playback_rate);

  size_t GetNumberOfPendingTasks() const {
    return number_of_pending_tasks_.load();
  }

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
    Event(const int8_t* codec_config, int16_t codec_config_size)
        : type(kWriteCodecConfig),
          codec_config(codec_config),
          codec_config_size(codec_config_size) {}
    explicit Event(const scoped_refptr<InputBuffer>& input_buffer)
        : type(kWriteInputBuffer), input_buffer(input_buffer) {}

    Type type;
    scoped_refptr<InputBuffer> input_buffer;
    const int8_t* codec_config;
    int16_t codec_config_size;
  };

  struct QueueInputBufferTask {
    DequeueInputResult dequeue_input_result;
    Event event;
  };

  static void* DecoderThreadEntryPoint(void* context);
  void DecoderThreadFunc();

  void TeardownCodec();

  void CollectPendingData_Locked(
      std::deque<Event>* pending_tasks,
      std::vector<int>* input_buffer_indices,
      std::vector<DequeueOutputResult>* dequeue_output_results);
  bool ProcessOneInputBuffer(std::deque<Event>* pending_tasks,
                             std::vector<int>* input_buffer_indices);
  void HandleError(const char* action_name, jint status);

  // MediaCodecBridge::Handler methods
  // Note that these methods are called from the default looper and is not on
  // the decoder thread.
  void OnMediaCodecError(bool is_recoverable,
                         bool is_transient,
                         const std::string& diagnostic_info) override;
  void OnMediaCodecInputBufferAvailable(int buffer_index) override;
  void OnMediaCodecOutputBufferAvailable(int buffer_index,
                                         int flags,
                                         int offset,
                                         int64_t presentation_time_us,
                                         int size) override;
  void OnMediaCodecOutputFormatChanged() override;
  void OnMediaCodecFrameRendered(SbTime frame_timestamp) override;

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  const SbMediaType media_type_;
  Host* host_;
  DrmSystem* const drm_system_;
  const FrameRenderedCB frame_rendered_cb_;
  const bool tunnel_mode_enabled_;

  ErrorCB error_cb_;

  atomic_bool stream_ended_;

  atomic_bool destroying_;

  optional<QueueInputBufferTask> pending_queue_input_buffer_task_;

  atomic_int32_t number_of_pending_tasks_;

  Mutex mutex_;
  ConditionVariable condition_variable_;
  std::deque<Event> pending_tasks_;
  std::vector<int> input_buffer_indices_;
  std::vector<DequeueOutputResult> dequeue_output_results_;

  bool is_output_restricted_ = false;
  bool first_call_on_handler_thread_ = true;

  // Working thread to avoid lengthy decoding work block the player thread.
  SbThread decoder_thread_ = kSbThreadInvalid;
  scoped_ptr<MediaCodecBridge> media_codec_bridge_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_DECODER_H_
