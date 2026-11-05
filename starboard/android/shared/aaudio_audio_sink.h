// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_
#define STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_

#include <aaudio/AAudio.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "starboard/android/shared/aaudio_presented_frame_tracker.h"
#include "starboard/android/shared/audio_sink_android.h"
#include "starboard/audio_sink.h"
#include "starboard/common/pass_key.h"
#include "starboard/media.h"

namespace starboard {

// AaudioAudioSink is a pull-based audio sink implementation using the
// Android NDK AAudio data callback API.
//
// Audio frames are pulled directly by AAudio on its real-time OS audio thread
// whenever the audio hardware is ready for new data, minimizing startup
// and switch latency.
//
// Expected lifetime / ownership:
// Instances are created via AaudioAudioSink::Create() and owned by
// AudioRendererSinkAndroid on the player worker thread.
//
// The AAudio stream itself is owned by a ref-counted StreamController. All
// stream operations (open, start, reopen after AAUDIO_ERROR_DISCONNECTED, stop
// and close) of all sinks run serially on a single process-wide
// "aaudio_worker" thread, so the player worker thread never blocks on AAudio,
// and a stream is only opened after the streams of previously destroyed sinks
// are closed. The AAudio callbacks only hold a weak reference to the
// StreamController, so a late callback never touches a destroyed sink or
// controller.
//
// Threading model:
// - Control methods (SetVolume, SetPlaybackRate, SetStartTime, Flush, etc.)
//   are called on the Cobalt player worker thread.
// - OnAudioData() runs on the AAudio callback thread and never blocks (atomics
//   and the try-locked callback mutex of the StreamController).
// - Errors are reported to |callbacks_.error| from the AAudio error callback
//   thread or the "aaudio_worker" thread, never after the sink starts being
//   destroyed.
class AaudioAudioSink final : public AudioSinkAndroid {
 public:
  struct Callbacks {
    SbAudioSinkUpdateSourceStatusFunc update_source_status;
    SbAudioSinkPrivate::ConsumeFramesFunc consume_frames;
    SbAudioSinkPrivate::ErrorFunc error;
  };

  static bool IsSupported(SbMediaAudioSampleType sample_type);

  // Returns immediately without opening the AAudio stream; the stream is
  // opened and started asynchronously. Until the stream is running, no frames
  // are consumed. If the stream can't be opened, |callbacks.error| is called
  // with |capability_changed| set to true, and for a while later calls return
  // nullptr for the same |channels| and |sample_rate|, so the caller falls back
  // to another sink when the player is recreated. Also returns nullptr if the
  // AAudio worker thread is stuck in an AAudio call; sinks whose stream open
  // doesn't complete in time (e.g. queued behind such a call) report a
  // capability changed error instead.
  static std::unique_ptr<AaudioAudioSink> Create(
      int channels,
      int sample_rate,
      SbMediaAudioSampleType sample_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      Callbacks callbacks,
      bool is_web_audio,
      void* context);

  AaudioAudioSink(PassKey<AaudioAudioSink>,
                  int channels,
                  int sample_rate,
                  SbAudioSinkFrameBuffers frame_buffers,
                  int frames_per_channel,
                  Callbacks callbacks,
                  void* context);
  ~AaudioAudioSink() override;

  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(double volume) override;
  bool Flush() override;
  void SetStartTime(int64_t start_time_us) override;

  // Simulates an AAudio error callback for the current stream, e.g.
  // AAUDIO_ERROR_DISCONNECTED to exercise the reopen path. Unlike a real error
  // callback, it runs on the calling thread.
  void SimulateErrorForTesting(aaudio_result_t error);
  // Returns the number of streams successfully opened and started by this
  // sink.
  int GetStreamOpenCountForTesting() const;
  // Makes the next |count| attempts to open a stream fail.
  static void SetOpenFailuresForTesting(int count);
  // Returns the number of injected open failures not consumed yet.
  static int GetOpenFailuresForTesting();
  // Blocks the AAudio worker thread for |duration_us|, like a hung AAudio
  // call.
  static void StallWorkerForTesting(int64_t duration_us);
  static void ResetUnsupportedFormatsForTesting();
  // Waits until the stream operations that are due now on the AAudio worker
  // thread, e.g. closing the streams of destroyed sinks, are done. Doesn't wait
  // for delayed operations, e.g. open retries.
  static void WaitForPendingStreamOperationsForTesting();

 private:
  class StreamController;

  // Called by StreamController with its callback mutex held and only while
  // the sink is attached to it.
  // |stream_position| is the stream frame position of |audio_data|, and
  // |generation| identifies the stream.
  aaudio_data_callback_result_t OnAudioData(void* audio_data,
                                            int32_t num_frames,
                                            AAudioStream* stream,
                                            int generation,
                                            int64_t stream_position);
  // Reports the frames in flight that AAudio has presented as consumed. Falls
  // back to reporting frames when copied if AAudio provides no timestamps.
  void ConsumePresentedFrames(AAudioStream* stream, int64_t stream_position);
  void ReportError(bool capability_changed, const std::string& message);

  void CopyFromFrameBuffer(int offset_in_frames,
                           int num_frames,
                           float volume,
                           float* dest) const;
  void WriteSilence(float* dest, int num_frames) const;

  // How long timestamps may be unavailable while frames are in flight before
  // falling back to reporting frames as consumed when copied.
  static constexpr int64_t kTimestampTimeoutUs = 200'000;

  const int channels_;
  void* const frame_buffer_;
  const int frames_per_channel_;
  const Callbacks callbacks_;
  void* const context_;

  std::shared_ptr<StreamController> controller_;

  // Set by Flush() and cleared by SetStartTime(), i.e. when the renderer
  // resumes after a seek. While set, OnAudioData() outputs silence and doesn't
  // consume frames, so stale pre-seek frames are never consumed (frames already
  // queued in the AAudio stream, <= 2 bursts, still play). Guarded by the
  // callback mutex of |controller_|.
  bool is_flushed_ = false;

  // Frames are reported as consumed when AAudio reports them presented, so
  // that media time follows actual output, like AudioTrackAudioSink. The
  // following are only accessed with the callback mutex of |controller_| held
  // (OnAudioData() and Flush()).
  AaudioPresentedFrameTracker presented_frames_;
  // The generation of the stream the frames in flight were copied to.
  int generation_ = 0;
  // When AAudioStream_getTimestamp() started failing, 0 if it succeeds.
  int64_t timestamp_unavailable_since_us_ = 0;
  // Set if AAudio doesn't provide timestamps; frames are then reported as
  // consumed when copied.
  bool timestamps_unavailable_ = false;

  std::atomic<float> volume_ = 1.0f;
  std::atomic<float> playback_rate_ = 1.0f;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_
