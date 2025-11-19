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

#include <atomic>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/media_codec_bridge.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/thread.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {

// TODO: Better encapsulation the MediaCodecBridge so the decoders no longer
//       need to talk directly to the MediaCodecBridge.
class MediaCodecDecoder final : private MediaCodecBridge::Handler,
                                protected JobQueue::JobOwner {
 public:
  using FrameRenderedCB = std::function<void(int64_t)>;
  using FirstTunnelFrameReadyCB = std::function<void(void)>;

  // This class should be implemented by the users of MediaCodecDecoder to
  // receive various notifications.  Note that all such functions are called on
  // the decoder thread.
  // TODO: Replace this with std::function<> based callbacks.
  class Host {
   public:
    virtual void ProcessOutputBuffer(MediaCodecBridge* media_codec_bridge,
                                     const DequeueOutputResult& output) = 0;
    virtual void OnEndOfStreamWritten(MediaCodecBridge* media_codec_bridge) = 0;
    virtual void RefreshOutputFormat(MediaCodecBridge* media_codec_bridge) = 0;
    // This function gets called frequently on the decoding thread to give the
    // Host a chance to process when the MediaCodecDecoder is decoding.
    // TODO: Revise the scheduling logic to give the host a chance to process in
    //       a more elegant way.
    virtual bool Tick(MediaCodecBridge* media_codec_bridge) = 0;
    // This function gets called before calling Flush() on the contained
    // MediaCodecBridge so the host can have a chance to do necessary cleanups
    // before the MediaCodecBridge is flushed.
    virtual void OnFlushing() = 0;

    virtual bool IsBufferDecodeOnly(
        const scoped_refptr<InputBuffer>& input_buffer) = 0;

   protected:
    ~Host() {}
  };

  MediaCodecDecoder(Host* host,
                    const AudioStreamInfo& audio_stream_info,
                    SbDrmSystem drm_system);
  MediaCodecDecoder(Host* host,
                    SbMediaVideoCodec video_codec,
                    // `width_hint` and `height_hint` are used to create the
                    // Android video format, which don't have to be directly
                    // related to the resolution of the video.
                    int width_hint,
                    int height_hint,
                    std::optional<int> max_width,
                    std::optional<int> max_height,
                    int fps,
                    jobject j_output_surface,
                    SbDrmSystem drm_system,
                    const SbMediaColorMetadata* color_metadata,
                    bool require_software_codec,
                    const FrameRenderedCB& frame_rendered_cb,
                    const FirstTunnelFrameReadyCB& first_tunnel_frame_ready_cb,
                    int tunnel_mode_audio_session_id,
                    bool force_big_endian_hdr_metadata,
                    int max_video_input_size,
                    int64_t flush_delay_usec,
                    std::string* error_message);
  ~MediaCodecDecoder();

  void Initialize(const ErrorCB& error_cb);
  void WriteInputBuffers(const InputBuffers& input_buffers);
  void WriteEndOfStream();

  void SetPlaybackRate(double playback_rate);

  size_t GetNumberOfPendingInputs() const {
    return number_of_pending_inputs_.load();
  }

  bool is_valid() const { return media_codec_bridge_ != NULL; }

  bool Flush();

 private:
  // Holding inputs to be processed.  They are mostly InputBuffer objects, but
  // can also be codec configs or end of streams.
  struct PendingInput {
    enum Type {
      kInvalid,
      kWriteCodecConfig,
      kWriteInputBuffer,
      kWriteEndOfStream,
    };

    explicit PendingInput(Type type = kInvalid) : type(type) {
      SB_DCHECK_NE(type, kWriteInputBuffer);
      SB_DCHECK_NE(type, kWriteCodecConfig);
    }
    explicit PendingInput(const std::vector<uint8_t>& codec_config)
        : type(kWriteCodecConfig), codec_config(codec_config) {
      SB_DCHECK(!this->codec_config.empty());
    }
    explicit PendingInput(const scoped_refptr<InputBuffer>& input_buffer)
        : type(kWriteInputBuffer), input_buffer(input_buffer) {}

    Type type;
    scoped_refptr<InputBuffer> input_buffer;
    std::vector<uint8_t> codec_config;
  };

  // Holding a PendingInput and a DequeueInputResult when call to
  // QueueInputBuffer or QueueSecureInputBuffer fails so it can be retried
  // later.
  struct PendingInputToRetry {
    DequeueInputResult dequeue_input_result;
    PendingInput pending_input;
  };

  class DecoderThread;

  void DecoderThreadFunc();

  void TerminateDecoderThread();

  void CollectPendingData_Locked(
      std::deque<PendingInput>* pending_inputs,
      std::vector<int>* input_buffer_indices,
      std::vector<DequeueOutputResult>* dequeue_output_results);
  bool ProcessOneInputBuffer(std::deque<PendingInput>* pending_inputs,
                             std::vector<int>* input_buffer_indices);
  void HandleError(const char* action_name, jint status);
  void ReportError(const SbPlayerError error, const std::string error_message);

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
  void OnMediaCodecFrameRendered(int64_t frame_timestamp) override;
  void OnMediaCodecFirstTunnelFrameReady() override;

  ThreadChecker thread_checker_;

  const SbMediaType media_type_;
  Host* const host_;
  DrmSystem* const drm_system_;
  const FrameRenderedCB frame_rendered_cb_;
  const FirstTunnelFrameReadyCB first_tunnel_frame_ready_cb_;
  const bool tunnel_mode_enabled_;
  const int64_t flush_delay_usec_;

  ErrorCB error_cb_;

  bool error_occurred_ = false;
  SbPlayerError error_;
  std::string error_message_;

  std::atomic_bool stream_ended_{false};

  std::atomic_bool destroying_{false};

  std::optional<PendingInputToRetry> pending_input_to_retry_;

  std::atomic<int32_t> number_of_pending_inputs_{0};

  std::mutex mutex_;
  std::condition_variable condition_variable_;
  std::deque<PendingInput> pending_inputs_;
  std::vector<int> input_buffer_indices_;
  std::vector<DequeueOutputResult> dequeue_output_results_;

  bool is_output_restricted_ = false;
  bool first_call_on_handler_thread_ = true;

  // Working thread to avoid lengthy decoding work block the player thread.
  std::unique_ptr<Thread> decoder_thread_;
  std::unique_ptr<MediaCodecBridge> media_codec_bridge_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_DECODER_H_
