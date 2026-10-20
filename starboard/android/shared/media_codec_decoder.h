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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CODEC_DECODER_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CODEC_DECODER_H_

#include <atomic>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/media_codec.h"
#include "starboard/common/pass_key.h"
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
//
// Threading model
// ---------------
// The decoder runs the MediaCodec interaction off the player worker thread.
// Depending on the configuration it uses one of the following:
//
//  * A single "audio_decoder" / "video_decoder" thread
//    (AudioDecoderThreadFunc() / VideoDecoderThreadFunc()), which both feeds
//    input buffers to the codec and processes its output.
//  * Two threads for video when |use_dual_threads_| is true: "VidDecIn" feeds
//    input buffers and "VidDecOut" processes output buffers and drives the
//    renderer through Host::Tick().
//
// For the dual-thread case there are two implementations of each thread
// function, selected by |enable_codec_thread_improvements_| (the h5vcc setting
// "Media.EnableCodecThreadImprovements"):
//
//  * InputThreadFunc() / OutputThreadFunc(): the original implementation.  The
//    output thread polls Host::Tick() every 8ms and the input thread may be
//    woken up by producers even when it has nothing to do.
//  * InputThreadFuncV2() / OutputThreadFuncV2(): the output thread is driven by
//    output buffer callbacks and by the deadline of the next frame to render
//    (Host::GetNextTickWaitDurationUs()), the input thread is only signaled
//    when it has a complete (input, input buffer index) pair to work on, and
//    DRM key waits are event-driven instead of polled.
//
// All shared state between the player worker thread, the MediaCodec callback
// thread (OnMediaCodec*() below) and the decoder threads is guarded by
// |mutex_|; see the comments on the individual members.
class MediaCodecDecoder final : private MediaCodec::Handler,
                                protected JobQueue::JobOwner {
 public:
  using FrameRenderedCB = std::function<void(int64_t)>;
  using FirstTunnelFrameReadyCB = std::function<void(void)>;

  // How long the video output thread waits for new output buffers before it
  // calls Host::Tick() again when the host doesn't request otherwise.  This is
  // the polling interval of the original implementation and the lower bound of
  // the deadline-driven wait of OutputThreadFuncV2().
  static constexpr int64_t kDefaultWaitForNextFrameIntervalUs = 8'000;

  MediaCodec* media_codec() const { return media_codec_bridge_.get(); }

  // This class should be implemented by the users of MediaCodecDecoder to
  // receive various notifications.  Note that all such functions are called on
  // the decoder thread.
  // TODO: Replace this with std::function<> based callbacks.
  class Host {
   public:
    virtual void ProcessOutputBuffer(MediaCodec* media_codec,
                                     const DequeueOutputResult& output,
                                     int number_of_pending_inputs) = 0;
    virtual void OnEndOfStreamWritten(MediaCodec* media_codec) = 0;
    virtual void RefreshOutputFormat(MediaCodec* media_codec) = 0;
    // This function gets called frequently on the decoding thread to give the
    // Host a chance to process when the MediaCodecDecoder is decoding.
    // TODO: Revise the scheduling logic to give the host a chance to process in
    //       a more elegant way.
    virtual bool Tick(MediaCodec* media_codec) = 0;
    // Returns how long the video output thread may wait, in microseconds,
    // before calling Tick() again when no new output buffer arrives in the
    // meantime.  It is queried right after each Tick() on the same thread, so
    // the host can derive it from the work it just did (e.g. the release time
    // of the next frame it holds).  Only used by OutputThreadFuncV2(), i.e.
    // when the codec thread improvements are enabled; other hosts can keep the
    // default.
    virtual int64_t GetNextTickWaitDurationUs() const {
      return kDefaultWaitForNextFrameIntervalUs;
    }
    // This function gets called before calling Flush() on the contained
    // MediaCodecBridge so the host can have a chance to do necessary cleanups
    // before the MediaCodecBridge is flushed.
    virtual void OnFlushing() = 0;

    virtual bool IsBufferDecodeOnly(
        const scoped_refptr<InputBuffer>& input_buffer) = 0;

   protected:
    ~Host() {}
  };

  static NonNullResult<std::unique_ptr<MediaCodecDecoder>> CreateForAudio(
      MediaCodec::Factory& media_codec_factory,
      JobQueue* job_queue,
      Host* host,
      const AudioStreamInfo& audio_stream_info,
      SbDrmSystem drm_system);
  static NonNullResult<std::unique_ptr<MediaCodecDecoder>> CreateForVideo(
      MediaCodec::Factory& media_codec_factory,
      JobQueue* job_queue,
      Host* host,
      SbMediaVideoCodec video_codec,
      // `frame_size_hint` is used to create the Android video format, which
      // doesn't have to be directly related to the resolution of the video.
      const Size& frame_size_hint,
      const std::optional<Size>& max_frame_size,
      int fps,
      const jni_zero::JavaRef<jobject>& j_output_surface,
      SbDrmSystem drm_system,
      const SbMediaColorMetadata* color_metadata,
      bool require_software_codec,
      const FrameRenderedCB& frame_rendered_cb,
      const FirstTunnelFrameReadyCB& first_tunnel_frame_ready_cb,
      std::optional<int> tunnel_mode_audio_session_id,
      bool enable_frame_renderer_listener,
      int max_video_input_size,
      int64_t flush_delay_usec,
      bool use_dual_threads,
      bool skip_video_frames_over_60_fps,
      bool enable_ndk_video,
      bool enable_trivial_optimizations,
      bool enable_codec_thread_improvements);

  MediaCodecDecoder(PassKey<MediaCodecDecoder>,
                    MediaCodec::Factory& media_codec_factory,
                    JobQueue* job_queue,
                    Host* host,
                    const AudioStreamInfo& audio_stream_info,
                    SbDrmSystem drm_system,
                    std::string* error_message);
  MediaCodecDecoder(
      PassKey<MediaCodecDecoder>,
      MediaCodec::Factory& media_codec_factory,
      JobQueue* job_queue,
      Host* host,
      SbMediaVideoCodec video_codec,
      // `frame_size_hint` is used to create the Android video format, which
      // doesn't have to be directly related to the resolution of the video.
      const Size& frame_size_hint,
      const std::optional<Size>& max_frame_size,
      int fps,
      const jni_zero::JavaRef<jobject>& j_output_surface,
      SbDrmSystem drm_system,
      const SbMediaColorMetadata* color_metadata,
      bool require_software_codec,
      const FrameRenderedCB& frame_rendered_cb,
      const FirstTunnelFrameReadyCB& first_tunnel_frame_ready_cb,
      std::optional<int> tunnel_mode_audio_session_id,
      bool enable_frame_renderer_listener,
      int max_video_input_size,
      int64_t flush_delay_usec,
      bool use_dual_threads,
      bool skip_video_frames_over_60_fps,
      bool enable_ndk_video,
      bool enable_trivial_optimizations,
      bool enable_codec_thread_improvements,
      std::string* error_message);
  ~MediaCodecDecoder();

  void Initialize(const ErrorCB& error_cb);
  void WriteInputBuffers(const InputBuffers& input_buffers);
  void WriteEndOfStream();

  // Forwards the playback rate to the MediaCodec and, when the codec thread
  // improvements are enabled, wakes up the video output thread so that it
  // re-evaluates its frame release schedule right away (e.g. on resume from
  // pause, where it may otherwise be in a long wait).
  void SetPlaybackRate(double playback_rate);

  size_t GetNumberOfPendingInputs() const {
    return number_of_pending_inputs_.load();
  }

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
    explicit PendingInput(scoped_refptr<InputBuffer> input_buffer)
        : type(kWriteInputBuffer), input_buffer(std::move(input_buffer)) {}

    Type type;
    scoped_refptr<InputBuffer> input_buffer;
    std::vector<uint8_t> codec_config;
  };

  // Holding an |input_buffer_index| and a PendingInput when a call to
  // QueueInputBuffer() or QueueSecureInputBuffer() fails so it can be retried
  // later.
  struct PendingInputToRetry {
    int input_buffer_index = -1;
    PendingInput pending_input;
  };

  class DecoderThread;
  void AudioDecoderThreadFunc();
  void VideoDecoderThreadFunc();

  // TODO(b/329686979): Consider turning MediaDecoder into a class hierarchy to
  // simplify the handling of threading, including the difference of a/v
  // threading in the original implementation in DecoderThreadFunc() above.
  //
  // Thread functions of the dual-thread video configuration.  The "V2"
  // variants are used when |enable_codec_thread_improvements_| is true; see
  // the class comment and the definitions for details.
  void InputThreadFunc();
  void InputThreadFuncV2();

  void OutputThreadFunc();
  void OutputThreadFuncV2();

  void TerminateDecoderThread();

  // Move the pending inputs/input buffer indices/output results from the
  // shared queues into the thread-local queues owned by the decoder threads.
  // Must be called with |mutex_| held.
  void CollectPendingData_Locked(
      std::deque<PendingInput>* pending_inputs,
      std::vector<int>* input_buffer_indices,
      std::vector<DequeueOutputResult>* dequeue_output_results);
  void CollectPendingInputData_Locked(std::deque<PendingInput>* pending_inputs,
                                      std::vector<int>* input_buffer_indices);
  void CollectPendingOutputData_Locked(
      std::vector<DequeueOutputResult>* dequeue_output_results);
  // Queues the front of |pending_inputs| into the codec using the front of
  // |input_buffer_indices| (or retries |pending_input_to_retry_| if set).
  // Returns false when the input couldn't be queued, in which case it has
  // either been returned to |pending_inputs| (unusable buffer index) or
  // stored in |pending_input_to_retry_| (queue(Secure)InputBuffer() failure,
  // e.g. MEDIA_CODEC_NO_KEY while waiting for a DRM license).
  bool ProcessOneInputBuffer(std::deque<PendingInput>* pending_inputs,
                             std::vector<int>* input_buffer_indices);
  void HandleError(const char* action_name, jint status);
  void ReportError(const SbPlayerError error, const std::string error_message);
  // Called by |drm_system_| (on arbitrary threads) whenever its key state
  // changes, to wake up InputThreadFuncV2() from a DRM key wait.  Only
  // registered when the codec thread improvements are enabled.
  void OnDrmKeyStateChanged();

  // MediaCodec::Handler methods
  void OnMediaCodecError(bool is_recoverable,
                         bool is_transient,
                         const std::string& error_message) override;
  void OnMediaCodecInputBufferAvailable(int32_t buffer_index) override;
  void OnMediaCodecOutputBufferAvailable(int32_t buffer_index,
                                         int32_t flags,
                                         int32_t offset,
                                         int64_t presentation_time_us,
                                         int32_t size) override;
  void OnMediaCodecOutputFormatChanged() override;
  void OnMediaCodecFrameRendered(int64_t presentation_time_us) override;
  void OnMediaCodecFirstTunnelFrameReady() override;

  ThreadChecker thread_checker_;

  const SbMediaType media_type_;
  Host* const host_;
  DrmSystem* const drm_system_;
  const FrameRenderedCB frame_rendered_cb_;
  const FirstTunnelFrameReadyCB first_tunnel_frame_ready_cb_;
  const bool tunnel_mode_enabled_;
  const int64_t flush_delay_usec_;
  const int64_t video_decoder_poll_interval_us_;
  const bool use_dual_threads_ = false;
  const bool enable_trivial_optimizations_ = false;
  // Selects InputThreadFuncV2()/OutputThreadFuncV2() and the event-driven DRM
  // key wait.  Has no effect unless |use_dual_threads_| is also true.
  const bool enable_codec_thread_improvements_ = false;

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
  // TODO(b/329686979): Consider guarding input and output logic using two
  // mutexes.
  // Only used when |use_dual_threads_| is true.  Signaled by the producers of
  // |pending_inputs_| and |input_buffer_indices_|, by OnDrmKeyStateChanged()
  // and by TerminateDecoderThread().
  std::condition_variable video_input_condition_variable_;
  // Only used when |use_dual_threads_| is true.  Signaled by the producers of
  // |dequeue_output_results_|, by SetPlaybackRate() and by
  // TerminateDecoderThread().
  std::condition_variable video_output_condition_variable_;
  // Shared queues filled by the player worker thread (|pending_inputs_|) and
  // the MediaCodec callback thread (|input_buffer_indices_|,
  // |dequeue_output_results_|), and drained by the decoder threads.  Guarded
  // by |mutex_|.
  std::deque<PendingInput> pending_inputs_;
  std::vector<int> input_buffer_indices_;
  std::vector<DequeueOutputResult> dequeue_output_results_;
  // The number of pending inputs and input buffer indices held by the video
  // input thread that are waiting for their counterpart while the thread is
  // blocked on |video_input_condition_variable_|.  Producers use them to only
  // signal when a new item completes an (input, input buffer index) pair.
  // Guarded by |mutex_|.  Only used when |use_dual_threads_| and
  // |enable_codec_thread_improvements_| are true.
  size_t unmatched_input_thread_inputs_ = 0;
  size_t unmatched_input_thread_indices_ = 0;
  // Set to request an extra Tick() from the video output thread, e.g. when the
  // playback rate changes.  Guarded by |mutex_|.  Only used when
  // |use_dual_threads_| and |enable_codec_thread_improvements_| are true.
  bool output_wakeup_requested_ = false;

  bool is_output_restricted_ = false;
  bool first_call_on_handler_thread_ = true;

  // Working threads to avoid lengthy decoding work block the player thread.
  std::unique_ptr<Thread> decoder_thread_;
  // Only used when |use_dual_threads_| is true.
  std::unique_ptr<Thread> video_input_thread_;
  // Only used when |use_dual_threads_| is true.
  std::unique_ptr<Thread> video_output_thread_;

  std::unique_ptr<MediaCodec> media_codec_bridge_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_DECODER_H_
