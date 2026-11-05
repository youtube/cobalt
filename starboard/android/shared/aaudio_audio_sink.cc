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

#include "starboard/android/shared/aaudio_audio_sink.h"

#include <android/api-level.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <utility>

#include "starboard/android/shared/aaudio_loader.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/no_destructor.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {
namespace {

// Delays before retrying to open and start a stream, initially and after
// AAUDIO_ERROR_DISCONNECTED, before giving up and reporting an error. Reopens
// back off longer (about 1.75 s in total), as the output may be unavailable
// for a while during a route change (e.g. HDMI hot plug, AVR/TV input switch).
constexpr int64_t kOpenRetryDelaysUs[] = {100'000, 200'000};
constexpr int64_t kReopenRetryDelaysUs[] = {250'000, 500'000, 1'000'000};
// Upper bound of consecutive in-sink reopens, to avoid an endless
// close/reopen loop on a route that disconnects every stream. Reopens are
// consecutive if each happens within |kReopenCountResetIntervalUs| of the
// previous one, so that occasional route changes during long playbacks (e.g.
// live streams) never hit the limit.
constexpr int kMaxReopens = 10;
constexpr int64_t kReopenCountResetIntervalUs = 60'000'000;
// How long AAudio isn't used for a format after it failed to open a stream.
constexpr int64_t kUnsupportedFormatRetryIntervalUs = 5 * 60 * 1'000'000LL;
// AAudio calls on the worker thread taking longer than this are logged.
constexpr int64_t kSlowStreamOperationUs = 1'000'000;
// If an AAudio call on the worker thread takes longer than this (e.g. a hung
// AAudioStream_close()), the worker is considered stalled and new sinks aren't
// created. Also, a sink whose stream open or reopen doesn't complete within
// this time after it's due (because it hangs itself or is queued behind a hung
// call) reports a capability changed error, so that the player is recreated
// (with AudioTrack while the worker is stalled).
constexpr int64_t kStalledWorkerThresholdUs = 3'000'000;
// How often the watchdog checks sinks waiting for the worker.
constexpr int64_t kWatchdogIntervalUs = 1'000'000;
// How long the AAudio callback user data is kept after its stream is closed,
// as callbacks may still run after AAudioStream_close() before Android 12.
constexpr int64_t kCallbackHandleReleaseDelayUs = 10'000'000;

std::atomic_int g_open_failures_for_testing = 0;

int64_t ElapsedMs(int64_t start_us) {
  return (CurrentMonotonicTime() - start_us) / 1'000;
}

enum class FallbackReason {
  // AAudio failed to open a stream (the format is marked unsupported).
  kOpenFailed,
  // The stream was disconnected too often (the format is marked unsupported).
  kDisconnectLoop,
  // The worker thread is stuck in an AAudio call.
  kWorkerStalled,
  // Create() refused a format marked unsupported.
  kUnsupportedFormat,
  kCount,
};

// Logs falling back from AAudio to AudioTrack with a stable tag, once per
// reason and process, so the fallback rate can be found in field logs.
void LogFallbackOnce(FallbackReason reason) {
  static constexpr const char* kNames[] = {
      "open_failed", "disconnect_loop", "worker_stalled", "unsupported_format"};
  static_assert(std::size(kNames) ==
                static_cast<size_t>(FallbackReason::kCount));
  static std::atomic_bool logged[static_cast<size_t>(FallbackReason::kCount)];
  const auto index = static_cast<size_t>(reason);
  if (!logged[index].exchange(true)) {
    SB_LOG(ERROR) << "AAUDIO_FALLBACK reason=" << kNames[index];
  }
}

// Formats (channels, sample rate) for which AAudio recently failed to open or
// start a stream. As the stream is opened asynchronously, the sink can't fall
// back to AudioTrack itself, so the failure is reported as a capability change
// (which recreates the player) and Create() returns nullptr for the format for
// |kUnsupportedFormatRetryIntervalUs|, so that AudioRendererSinkAndroid uses
// AudioTrack instead. A successful open of the format clears it.
class UnsupportedFormats {
 public:
  static UnsupportedFormats* GetInstance() {
    static NoDestructor<UnsupportedFormats> instance;
    return instance.get();
  }

  void MarkFailed(int channels, int sample_rate) {
    std::lock_guard lock(mutex_);
    failure_times_us_[{channels, sample_rate}] = CurrentMonotonicTime();
  }

  void MarkSucceeded(int channels, int sample_rate) {
    std::lock_guard lock(mutex_);
    failure_times_us_.erase({channels, sample_rate});
  }

  bool IsUnsupported(int channels, int sample_rate) {
    std::lock_guard lock(mutex_);
    auto iter = failure_times_us_.find({channels, sample_rate});
    if (iter == failure_times_us_.end()) {
      return false;
    }
    if (CurrentMonotonicTime() - iter->second >=
        kUnsupportedFormatRetryIntervalUs) {
      failure_times_us_.erase(iter);
      return false;
    }
    return true;
  }

  void Reset() {
    std::lock_guard lock(mutex_);
    failure_times_us_.clear();
  }

 private:
  std::mutex mutex_;
  // Guarded by |mutex_|.
  std::map<std::pair<int, int>, int64_t> failure_times_us_;
};

// Runs the AAudio stream operations of all sinks serially on one thread. This
// keeps AAudio calls off the player threads, bounds the number of threads, and
// orders opening a stream after closing the streams of previous sinks (e.g.
// when the player is recreated), which may compete for the same output.
//
// The threads are never stopped, as the worker is owned by a NoDestructor; at
// process exit, a task may still be running. Tasks must only touch heap or
// NoDestructor state.
class StreamWorker {
 public:
  static StreamWorker* GetInstance() {
    static NoDestructor<StreamWorker> instance;
    return instance.get();
  }

  StreamWorker()
      : thread_(JobThread::Create("aaudio_worker")),
        watchdog_thread_(JobThread::Create("aaudio_watchdog")) {}

  void Post(const char* name,
            std::function<void()> task,
            int64_t delay_us = 0) {
    thread_->Schedule(
        [this, name, task = std::move(task)] {
          const int64_t start_us = CurrentMonotonicTime();
          task_name_.store(name);
          task_start_us_.store(start_us);
          task();
          task_start_us_.store(0);
          const int64_t elapsed_us = CurrentMonotonicTime() - start_us;
          SB_LOG_IF(WARNING, elapsed_us > kSlowStreamOperationUs)
              << "AAudio stream operation \"" << name << "\" took "
              << elapsed_us / 1'000 << " ms.";
        },
        delay_us);
  }

  // Runs |task| on the watchdog thread, which never calls AAudio.
  void PostWatchdog(std::function<void()> task, int64_t delay_us) {
    watchdog_thread_->Schedule(std::move(task), delay_us);
  }

  // Returns true if the current stream operation has been running for too
  // long, e.g. because AAudio hangs.
  bool IsStalled() {
    const int64_t start_us = task_start_us_.load();
    const bool stalled = start_us != 0 && CurrentMonotonicTime() - start_us >
                                              kStalledWorkerThresholdUs;
    if (stalled && logged_stall_start_us_.exchange(start_us) != start_us) {
      SB_LOG(ERROR) << "AAudio stream operation \"" << task_name_.load()
                    << "\" has been running for " << ElapsedMs(start_us)
                    << " ms.";
    }
    return stalled;
  }

  // Waits for the tasks that are due now (not the delayed ones, e.g. retries).
  void WaitForTasksDueNow() {
    thread_->ScheduleAndWait([] {});
  }

 private:
  const std::unique_ptr<JobThread> thread_;
  const std::unique_ptr<JobThread> watchdog_thread_;
  // Start time of the running task, 0 if idle.
  std::atomic<int64_t> task_start_us_ = 0;
  std::atomic<const char*> task_name_ = "";
  std::atomic<int64_t> logged_stall_start_us_ = 0;
};

struct AAudioStreamBuilderDeleter {
  void operator()(AAudioStreamBuilder* builder) const {
    if (!builder) {
      return;
    }
    AAudio::StreamBuilder_Delete(builder);
  }
};

void CopyWithVolume(const float* src,
                    int num_samples,
                    float volume,
                    float* dest) {
  if (volume == 1.0f) {
    std::memcpy(dest, src, num_samples * sizeof(float));
    return;
  }
  for (int i = 0; i < num_samples; ++i) {
    dest[i] = src[i] * volume;
  }
}

// Same as Oboe's AudioStream::calculateDefaultDelayBeforeCloseMillis(): one
// burst plus 1 ms of margin, clamped to [10, 100] ms.
int64_t GetDelayBeforeCloseUs(int burst_frames, int sample_rate) {
  constexpr int64_t kMinDelayBeforeCloseMs = 10;
  constexpr int64_t kMaxDelayBeforeCloseMs = 100;
  const int64_t burst_ms =
      sample_rate > 0 ? burst_frames * 1'000LL / sample_rate : 0;
  const int64_t delay_ms =
      std::clamp(1 + burst_ms, kMinDelayBeforeCloseMs, kMaxDelayBeforeCloseMs);
  return delay_ms * 1'000;
}

}  // namespace

// Owns the AAudio stream of a sink. All stream operations run on the
// StreamWorker thread, and the tasks posted there keep the controller alive.
// The sink detaches itself in its destructor (under |callback_mutex_|), after
// which callbacks output silence and never call into the sink or the renderer.
class AaudioAudioSink::StreamController
    : public std::enable_shared_from_this<StreamController> {
 public:
  StreamController(AaudioAudioSink* sink,
                   int channels,
                   int sample_rate,
                   bool is_web_audio)
      : channels_(channels),
        sample_rate_(sample_rate),
        is_web_audio_(is_web_audio),
        sink_(sink) {}

  ~StreamController() {
    // The last reference may be dropped by a callback on an AAudio thread
    // (see DataCallback()), so this must stay trivial. The stream is always
    // closed on the worker thread before, as the close task holds a
    // reference.
    SB_DCHECK(!stream_);
    SB_DCHECK(!handle_);
  }

  void Start() {
    PostStreamTask("open", [self = shared_from_this()] {
      self->OpenAndStartStream(0, /*is_reopen=*/false);
    });
  }

  // Called by the sink destructor on the player thread. Never blocks on
  // AAudio: it only waits for an in-flight callback to return, then posts
  // stopping and closing the stream to the worker thread.
  void DetachSinkAndShutdown() {
    {
      std::lock_guard lock(callback_mutex_);
      sink_ = nullptr;
    }
    quit_.store(true);
    StreamWorker::GetInstance()->Post(
        "close", [self = shared_from_this()] { self->CloseStream(); });
  }

  std::mutex& callback_mutex() { return callback_mutex_; }

  int open_count() const { return open_count_.load(); }

  void SimulateError(aaudio_result_t error) {
    OnAudioError(active_generation_.load(), error);
  }

 private:
  // The AAudio callback user data. It only holds a weak reference, so the
  // controller can be destroyed while AAudio may still call back (before
  // Android 12, callbacks may run after AAudioStream_close(), see
  // CloseStream()); it is released |kCallbackHandleReleaseDelayUs| after its
  // stream is closed.
  struct CallbackHandle {
    std::weak_ptr<StreamController> controller;
    // Identifies the stream, to ignore callbacks from replaced streams.
    int generation;
    int channels;
  };

  // Value of |waiting_since_us_| after the watchdog gave up on the stream.
  static constexpr int64_t kAbandoned = -1;

  // Posts a task that opens or reopens the stream. If the task doesn't
  // complete within |kStalledWorkerThresholdUs| after it's due, because it
  // hangs in AAudio or is queued behind a hung call of another sink (e.g. in
  // AAudioStream_close()), the watchdog reports a capability changed error, so
  // that the player isn't stuck without audio and is recreated. An abandoned
  // task isn't run, or closes the stream it opened.
  // Note that only the latest posted task is watched; if two are pending
  // (very rare, e.g. a reopen and an open retry), the first to complete stops
  // the watchdog.
  void PostStreamTask(const char* name,
                      std::function<void()> task,
                      int64_t delay_us = 0) {
    int64_t waiting = waiting_since_us_.load();
    const int64_t due_us = CurrentMonotonicTime() + delay_us;
    while (waiting != kAbandoned &&
           !waiting_since_us_.compare_exchange_weak(waiting, due_us)) {
    }
    if (waiting == kAbandoned) {
      return;
    }
    StreamWorker::GetInstance()->Post(
        name,
        [self = shared_from_this(), task = std::move(task)] {
          if (self->waiting_since_us_.load() != kAbandoned) {
            task();
          }
        },
        delay_us);
    if (!watchdog_armed_.exchange(true)) {
      StreamWorker::GetInstance()->PostWatchdog(
          [self = shared_from_this()] { self->CheckStreamTask(); },
          delay_us + kWatchdogIntervalUs);
    }
  }

  // Runs on the worker thread, when a stream task completes. Returns false if
  // the watchdog has given up on the stream (and reported an error).
  bool CompleteStreamTask() {
    int64_t waiting = waiting_since_us_.load();
    while (waiting != kAbandoned) {
      if (waiting_since_us_.compare_exchange_weak(waiting, 0)) {
        return true;
      }
    }
    return false;
  }

  // Runs on the watchdog thread.
  void CheckStreamTask() {
    int64_t waiting = waiting_since_us_.load();
    if (waiting <= 0 || quit_.load()) {
      // No stream task is pending. Re-check after disarming, in case a task
      // was posted in between without arming the watchdog.
      watchdog_armed_.store(false);
      waiting = waiting_since_us_.load();
      if (waiting <= 0 || quit_.load() || watchdog_armed_.exchange(true)) {
        return;
      }
    }
    if (CurrentMonotonicTime() - waiting > kStalledWorkerThresholdUs) {
      if (waiting_since_us_.compare_exchange_strong(waiting, kAbandoned)) {
        SB_LOG(ERROR) << "AAudio stream operation hasn't completed "
                      << ElapsedMs(waiting) << " ms after it was due.";
        if (StreamWorker::GetInstance()->IsStalled()) {
          LogFallbackOnce(FallbackReason::kWorkerStalled);
        }
        ReportError(/*capability_changed=*/true, "AAudio worker stalled");
      }
      return;
    }
    StreamWorker::GetInstance()->PostWatchdog(
        [self = shared_from_this()] { self->CheckStreamTask(); },
        kWatchdogIntervalUs);
  }

  void MarkFormatFailed(FallbackReason reason) {
    SB_LOG(ERROR) << "Falling back to AudioTrack for channels=" << channels_
                  << ", rate=" << sample_rate_ << " for "
                  << kUnsupportedFormatRetryIntervalUs / 1'000'000 << " s.";
    LogFallbackOnce(reason);
    UnsupportedFormats::GetInstance()->MarkFailed(channels_, sample_rate_);
  }

  static aaudio_data_callback_result_t DataCallback(AAudioStream* stream,
                                                    void* user_data,
                                                    void* audio_data,
                                                    int32_t num_frames) {
    auto* handle = static_cast<CallbackHandle*>(user_data);
    std::shared_ptr<StreamController> controller = handle->controller.lock();
    if (!controller) {
      std::fill_n(static_cast<float*>(audio_data),
                  num_frames * handle->channels, 0.0f);
      return AAUDIO_CALLBACK_RESULT_STOP;
    }
    return controller->OnAudioData(handle->generation, stream, audio_data,
                                   num_frames);
  }

  static void ErrorCallback(AAudioStream* /*stream*/,
                            void* user_data,
                            aaudio_result_t error) {
    auto* handle = static_cast<CallbackHandle*>(user_data);
    if (std::shared_ptr<StreamController> controller =
            handle->controller.lock()) {
      controller->OnAudioError(handle->generation, error);
    }
  }

  void OnAudioError(int generation, aaudio_result_t error) {
    SB_LOG(WARNING) << "AAudio pull stream error: "
                    << AAudio::ConvertResultToText(error);
    if (generation == 0 || generation != active_generation_.load() ||
        quit_.load()) {
      // A late error from a stream that is being or has been replaced, or
      // the sink is being destroyed.
      return;
    }
    if (error == AAUDIO_ERROR_DISCONNECTED) {
      // AAudio forbids closing/reopening the stream from its callbacks, so
      // reopen on the worker thread. This keeps playback going across audio
      // route changes (e.g. HDMI/CEC/BT).
      if (!reopen_pending_.exchange(true)) {
        PostStreamTask("reopen", [self = shared_from_this(), generation] {
          self->ReopenStream(generation);
        });
      }
      return;
    }
    ReportError(/*capability_changed=*/false, "AAudio stream error");
  }

  // Runs on the worker thread.
  void ReopenStream(int generation) {
    reopen_pending_.store(false);
    if (quit_.load() || generation != active_generation_.load()) {
      // The disconnected stream has already been replaced.
      CompleteStreamTask();
      return;
    }
    if (reopen_start_us_ != 0 && CurrentMonotonicTime() - reopen_start_us_ >
                                     kReopenCountResetIntervalUs) {
      // The stream has been healthy for a while since the last reopen.
      reopen_count_ = 0;
    }
    if (reopen_count_ >= kMaxReopens) {
      SB_LOG(WARNING) << "AAudio stream disconnected " << reopen_count_ + 1
                      << " times in a row, giving up reopening it.";
      CloseStream();
      if (CompleteStreamTask()) {
        // Fall back to the player-level recovery used before the in-sink
        // reopen, which recreates the player, with AudioTrack.
        MarkFormatFailed(FallbackReason::kDisconnectLoop);
        ReportError(/*capability_changed=*/true,
                    "AAudio stream disconnected too often");
      }
      return;
    }

    ++reopen_count_;
    SB_LOG(INFO) << "AAudio stream disconnected, reopening (reopen "
                 << reopen_count_ << "/" << kMaxReopens << ").";
    reopen_start_us_ = CurrentMonotonicTime();
    // Unlike AudioTrackAudioSink on kResetAndContinue, there is no media time
    // to resync: frames are reported as consumed when presented, so the frames
    // copied to the disconnected stream but not presented are copied again to
    // the new stream, and the media time stalls until the new stream presents
    // frames.
    CloseStream();
    OpenAndStartStream(0, /*is_reopen=*/true);
  }

  // Runs on the worker thread. Retries are posted with a delay so that the
  // worker thread isn't blocked during the backoff.
  void OpenAndStartStream(int attempt, bool is_reopen) {
    if (quit_.load()) {
      CompleteStreamTask();
      return;
    }
    const aaudio_result_t result = TryOpenAndStartStream();
    if (result == AAUDIO_OK) {
      if (!CompleteStreamTask()) {
        // The watchdog has given up on this (slow) open, and the player is
        // being recreated.
        CloseStream();
        return;
      }
      UnsupportedFormats::GetInstance()->MarkSucceeded(channels_, sample_rate_);
      SB_LOG_IF(INFO, is_reopen)
          << "AAudio stream reopened after disconnect in "
          << ElapsedMs(reopen_start_us_) << " ms.";
      return;
    }
    const int64_t* retry_delays_us =
        is_reopen ? kReopenRetryDelaysUs : kOpenRetryDelaysUs;
    const int max_retries =
        static_cast<int>(is_reopen ? std::size(kReopenRetryDelaysUs)
                                   : std::size(kOpenRetryDelaysUs));
    if (attempt < max_retries && !quit_.load()) {
      PostStreamTask(
          is_reopen ? "reopen_retry" : "open_retry",
          [self = shared_from_this(), attempt, is_reopen] {
            self->OpenAndStartStream(attempt + 1, is_reopen);
          },
          retry_delays_us[attempt]);
      return;
    }
    if (!CompleteStreamTask() || quit_.load()) {
      return;
    }
    const std::string result_text = AAudio::ConvertResultToText(result);
    if (is_reopen) {
      // The output is likely still unavailable after a route change, rather
      // than the format being unsupported, so the format isn't marked failed:
      // the recreated player tries AAudio again, and falls back to AudioTrack
      // only if that fails too.
      SB_LOG(ERROR) << "Failed to reopen AAudio stream after disconnect: "
                    << result_text;
      ReportError(/*capability_changed=*/true,
                  "AAudio stream disconnected and reopening failed (" +
                      result_text + ")");
      return;
    }
    SB_LOG(ERROR) << "Failed to open AAudio stream: " << result_text;
    MarkFormatFailed(FallbackReason::kOpenFailed);
    ReportError(/*capability_changed=*/true,
                "AAudio stream open failed (" + result_text +
                    "), falling back to AudioTrack");
  }

  // Runs on the worker thread.
  aaudio_result_t TryOpenAndStartStream() {
    SB_DCHECK(!stream_);

    if (g_open_failures_for_testing.load() > 0) {
      g_open_failures_for_testing.fetch_sub(1);
      return AAUDIO_ERROR_UNAVAILABLE;
    }

    AAudioStreamBuilder* raw_builder = nullptr;
    if (aaudio_result_t result = AAudio::CreateStreamBuilder(&raw_builder);
        result != AAUDIO_OK || !raw_builder) {
      SB_LOG(ERROR) << "Failed to create AAudioStreamBuilder: "
                    << AAudio::ConvertResultToText(result);
      return result != AAUDIO_OK ? result : AAUDIO_ERROR_INTERNAL;
    }
    std::unique_ptr<AAudioStreamBuilder, AAudioStreamBuilderDeleter> builder(
        raw_builder);

    auto* handle =
        new CallbackHandle{weak_from_this(), ++last_generation_, channels_};
    // Callbacks are only forwarded to the sink for the active stream. Publish
    // the generation before opening the stream, so that an
    // AAUDIO_ERROR_DISCONNECTED delivered during or right after
    // StreamBuilder_OpenStream() isn't ignored.
    active_generation_.store(handle->generation);

    AAudio::StreamBuilder_SetDirection(builder.get(), AAUDIO_DIRECTION_OUTPUT);
    AAudio::StreamBuilder_SetSampleRate(builder.get(), sample_rate_);
    AAudio::StreamBuilder_SetChannelCount(builder.get(), channels_);
    AAudio::StreamBuilder_SetFormat(builder.get(), AAUDIO_FORMAT_PCM_FLOAT);
    AAudio::StreamBuilder_SetSharingMode(builder.get(),
                                         AAUDIO_SHARING_MODE_SHARED);
    AAudio::StreamBuilder_SetPerformanceMode(
        builder.get(), AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudio::StreamBuilder_SetUsage(
        builder.get(),
        is_web_audio_ ? AAUDIO_USAGE_NOTIFICATION : AAUDIO_USAGE_MEDIA);
    AAudio::StreamBuilder_SetContentType(
        builder.get(),
        is_web_audio_ ? AAUDIO_CONTENT_TYPE_MUSIC : AAUDIO_CONTENT_TYPE_MOVIE);
    AAudio::StreamBuilder_SetDataCallback(builder.get(), &DataCallback, handle);
    AAudio::StreamBuilder_SetErrorCallback(builder.get(), &ErrorCallback,
                                           handle);

    const int64_t open_start_us = CurrentMonotonicTime();
    AAudioStream* stream = nullptr;
    if (aaudio_result_t result =
            AAudio::StreamBuilder_OpenStream(builder.get(), &stream);
        result != AAUDIO_OK || !stream) {
      SB_LOG(ERROR) << "Failed to open AAudioStream: "
                    << AAudio::ConvertResultToText(result);
      // No stream was created, so AAudio never calls back with |handle|.
      active_generation_.store(0);
      delete handle;
      return result != AAUDIO_OK ? result : AAUDIO_ERROR_INTERNAL;
    }
    const int64_t open_ms = ElapsedMs(open_start_us);

    const int32_t burst_frames = AAudio::Stream_GetFramesPerBurst(stream);
    // No callback runs before AAudioStream_requestStart() below.
    frames_written_.store(0, std::memory_order_relaxed);
    stream_ = stream;
    handle_ = handle;
    delay_before_close_us_ = GetDelayBeforeCloseUs(burst_frames, sample_rate_);
    if (burst_frames > 0) {
      AAudio::Stream_SetBufferSizeInFrames(stream, burst_frames * 2);
    }

    const int64_t start_start_us = CurrentMonotonicTime();
    if (aaudio_result_t result = AAudio::Stream_RequestStart(stream);
        result != AAUDIO_OK) {
      SB_LOG(ERROR) << "Failed to start AAudioStream: "
                    << AAudio::ConvertResultToText(result);
      CloseStream();
      return result;
    }

    open_count_.fetch_add(1);
    SB_LOG(INFO) << "AaudioAudioSink (Pull Mode) created: channels="
                 << channels_ << ", rate=" << sample_rate_
                 << ", burst=" << burst_frames << ", open_ms=" << open_ms
                 << ", start_ms=" << ElapsedMs(start_start_us);
    return AAUDIO_OK;
  }

  // Runs on the worker thread.
  void CloseStream() {
    if (!stream_) {
      return;
    }
    active_generation_.store(0);

    const int64_t close_start_us = CurrentMonotonicTime();
    const int32_t xrun_count = AAudio::Stream_GetXRunCount(stream_);
    AAudio::Stream_RequestStop(stream_);
    // Before Android 12 (S, API level 31), AAudioStream_close() doesn't wait
    // for the callback thread, so callbacks may still run after requestStop()
    // and even after close() returns (crbug.com/1183255). Like Oboe's
    // sleepBeforeClose(), give them time to finish. Callbacks after close()
    // only reach |handle_|, which is released later, and see an inactive
    // stream.
    if (android_get_device_api_level() < __ANDROID_API_S__) {
      usleep(delay_before_close_us_);
    }
    AAudio::Stream_Close(stream_);
    stream_ = nullptr;
    StreamWorker::GetInstance()->Post(
        "release_handle", [handle = handle_] { delete handle; },
        kCallbackHandleReleaseDelayUs);
    handle_ = nullptr;
    SB_LOG(INFO) << "AAudio stream closed in " << ElapsedMs(close_start_us)
                 << " ms, xrun_count=" << xrun_count;
  }

  aaudio_data_callback_result_t OnAudioData(int generation,
                                            AAudioStream* stream,
                                            void* audio_data,
                                            int32_t num_frames) {
    float* dest = static_cast<float*>(audio_data);
    if (generation != active_generation_.load()) {
      std::fill_n(dest, num_frames * channels_, 0.0f);
      return AAUDIO_CALLBACK_RESULT_STOP;
    }
    // The stream position of |audio_data|, i.e. the frames passed to the data
    // callback of this stream before, which matches the positions returned by
    // AAudioStream_getTimestamp(). Counted even if the sink outputs silence.
    const int64_t stream_position =
        frames_written_.fetch_add(num_frames, std::memory_order_relaxed);
    // If the lock isn't acquired (Flush() or the sink destructor is running),
    // output silence and don't call into the sink.
    std::unique_lock lock(callback_mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
      std::fill_n(dest, num_frames * channels_, 0.0f);
      return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    if (!sink_) {
      std::fill_n(dest, num_frames * channels_, 0.0f);
      return AAUDIO_CALLBACK_RESULT_STOP;
    }
    return sink_->OnAudioData(audio_data, num_frames, stream, generation,
                              stream_position);
  }

  void ReportError(bool capability_changed, const std::string& message) {
    std::lock_guard lock(callback_mutex_);
    if (sink_) {
      sink_->ReportError(capability_changed, message);
    }
  }

  const int channels_;
  const int sample_rate_;
  const bool is_web_audio_;

  // Guards |sink_| and the sink state accessed by its OnAudioData() and
  // Flush() (e.g. |is_flushed_| and the presented frame tracking). Held while
  // calling into the sink, so DetachSinkAndShutdown() waits for in-flight
  // callbacks.
  std::mutex callback_mutex_;
  AaudioAudioSink* sink_;  // Guarded by |callback_mutex_|.

  std::atomic_bool quit_ = false;
  std::atomic_bool reopen_pending_ = false;
  // Generation of the stream whose callbacks are forwarded to the sink, 0 if
  // none.
  std::atomic_int active_generation_ = 0;
  std::atomic_int open_count_ = 0;
  // Frames passed to the data callback of the current stream.
  std::atomic<int64_t> frames_written_ = 0;
  // When the pending open/reopen task was due, 0 if none, or |kAbandoned|.
  std::atomic<int64_t> waiting_since_us_ = 0;
  std::atomic_bool watchdog_armed_ = false;

  // Only accessed on the worker thread.
  AAudioStream* stream_ = nullptr;
  CallbackHandle* handle_ = nullptr;
  int last_generation_ = 0;
  int reopen_count_ = 0;
  int64_t reopen_start_us_ = 0;
  int64_t delay_before_close_us_ = 0;
};

// static
bool AaudioAudioSink::IsSupported(SbMediaAudioSampleType sample_type) {
  return sample_type == kSbMediaAudioSampleTypeFloat32 &&
         android_get_device_api_level() >= 28 && AAudio::Load();
}

// static
std::unique_ptr<AaudioAudioSink> AaudioAudioSink::Create(
    int channels,
    int sample_rate,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    Callbacks callbacks,
    bool is_web_audio,
    void* context) {
  if (sample_type != kSbMediaAudioSampleTypeFloat32) {
    SB_LOG(WARNING) << "AaudioAudioSink only supports Float32 sample type.";
    return nullptr;
  }
  if (!AAudio::Load()) {
    SB_LOG(WARNING) << "AAudio library is not available.";
    return nullptr;
  }
  if (UnsupportedFormats::GetInstance()->IsUnsupported(channels, sample_rate)) {
    SB_LOG(WARNING) << "AAudio recently failed with channels=" << channels
                    << ", rate=" << sample_rate << ".";
    LogFallbackOnce(FallbackReason::kUnsupportedFormat);
    return nullptr;
  }
  if (StreamWorker::GetInstance()->IsStalled()) {
    LogFallbackOnce(FallbackReason::kWorkerStalled);
    return nullptr;
  }

  auto sink = std::make_unique<AaudioAudioSink>(
      PassKey<AaudioAudioSink>(), channels, sample_rate, frame_buffers,
      frames_per_channel, callbacks, context);
  // Not std::make_shared(), so that the memory of the controller is freed
  // even while weak references from callback handles remain.
  sink->controller_ = std::shared_ptr<StreamController>(
      new StreamController(sink.get(), channels, sample_rate, is_web_audio));
  sink->controller_->Start();

  SB_LOG(INFO) << "AaudioAudioSink (Pull Mode) creating asynchronously: "
               << "channels=" << channels << ", rate=" << sample_rate
               << ", buffer_size=" << frames_per_channel;
  return sink;
}

AaudioAudioSink::AaudioAudioSink(PassKey<AaudioAudioSink>,
                                 int channels,
                                 int sample_rate,
                                 SbAudioSinkFrameBuffers frame_buffers,
                                 int frames_per_channel,
                                 Callbacks callbacks,
                                 void* context)
    : channels_(channels),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      callbacks_(callbacks),
      context_(context),
      presented_frames_(sample_rate) {
  SB_CHECK(callbacks_.update_source_status);
  SB_CHECK(callbacks_.consume_frames);
  SB_CHECK(frame_buffer_);
}

AaudioAudioSink::~AaudioAudioSink() {
  if (controller_) {
    // Waits only for an in-flight callback to return. The stream is stopped
    // and closed asynchronously on the worker thread, so this doesn't block
    // on AAudio. Must run before other members are destroyed.
    controller_->DetachSinkAndShutdown();
  }
}

void AaudioAudioSink::SetPlaybackRate(double playback_rate) {
  // AaudioAudioSink only supports 0.0 (pause) and 1.0 (play). Variable playback
  // rates (e.g. 0.5x, 1.25x, 2.0x) are time-stretched upstream by
  // AudioRendererPcm using Sonic since AllowDirectPlaybackRateSetting() returns
  // false.
  SB_CHECK(playback_rate == 0.0 || playback_rate == 1.0);
  playback_rate_.store(static_cast<float>(playback_rate),
                       std::memory_order_relaxed);
}

void AaudioAudioSink::SetVolume(double volume) {
  volume_.store(static_cast<float>(volume), std::memory_order_relaxed);
}

bool AaudioAudioSink::Flush() {
  // Only called on seek with Media.FlushAudioTrackDuringSeek. Without it,
  // AudioRendererSinkAndroid destroys and recreates the sink on each seek,
  // which closes and reopens the stream on the worker (off the player thread,
  // but adding ~80 ms of seek latency).
  // Acquire the callback mutex to wait for any in-flight OnAudioData() call.
  std::lock_guard lock(controller_->callback_mutex());
  is_flushed_ = true;
  // Frames in flight are pre-seek frames: drop them without consuming them.
  presented_frames_.Reset();
  timestamp_unavailable_since_us_ = 0;
  return true;
}

void AaudioAudioSink::SetStartTime(int64_t /*start_time_us*/) {
  // Called when the renderer resumes after a seek, once its state has been
  // reset, so it's safe to consume frames again.
  std::lock_guard lock(controller_->callback_mutex());
  is_flushed_ = false;
}

void AaudioAudioSink::SimulateErrorForTesting(aaudio_result_t error) {
  // Ignored before the stream is active, like a real error callback for a
  // stale stream.
  controller_->SimulateError(error);
}

int AaudioAudioSink::GetStreamOpenCountForTesting() const {
  return controller_->open_count();
}

// static
void AaudioAudioSink::SetOpenFailuresForTesting(int count) {
  g_open_failures_for_testing.store(count);
}

// static
int AaudioAudioSink::GetOpenFailuresForTesting() {
  return g_open_failures_for_testing.load();
}

// static
void AaudioAudioSink::StallWorkerForTesting(int64_t duration_us) {
  StreamWorker::GetInstance()->Post("stall_for_testing",
                                    [duration_us] { usleep(duration_us); });
}

// static
void AaudioAudioSink::ResetUnsupportedFormatsForTesting() {
  UnsupportedFormats::GetInstance()->Reset();
}

// static
void AaudioAudioSink::WaitForPendingStreamOperationsForTesting() {
  StreamWorker::GetInstance()->WaitForTasksDueNow();
}

// TODO: b/561166288 - Add unit tests for OnAudioData() (ring buffer
// wrap-around, underrun padding, volume, pause, and discard after Flush()).
aaudio_data_callback_result_t AaudioAudioSink::OnAudioData(
    void* audio_data,
    int32_t num_frames,
    AAudioStream* stream,
    int generation,
    int64_t stream_position) {
  float* dest = static_cast<float*>(audio_data);

  // After Flush() and until SetStartTime(), output silence and skip
  // consume_frames() to avoid consuming stale pre-seek frames.
  if (is_flushed_) {
    WriteSilence(dest, num_frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  // A reopened stream (after AAUDIO_ERROR_DISCONNECTED) restarts positions at
  // 0. Frames in flight in the old stream were dropped, so copy them again.
  // Also retry timestamps, which may have failed while the old stream was
  // disconnecting.
  if (generation != generation_) {
    generation_ = generation;
    presented_frames_.Reset();
    timestamp_unavailable_since_us_ = 0;
    timestamps_unavailable_ = false;
  }

  // Report the frames presented since the last callback. This must happen
  // before update_source_status(), which accounts for consumed frames.
  ConsumePresentedFrames(stream, stream_position);

  int frames_in_buffer = 0;
  int offset_in_frames = 0;
  bool is_playing = false;
  bool is_eos_reached = false;

  callbacks_.update_source_status(&frames_in_buffer, &offset_in_frames,
                                  &is_playing, &is_eos_reached, context_);

  // Frames in flight are still counted in |frames_in_buffer| until consumed;
  // copy only the frames after them.
  const int frames_in_flight = presented_frames_.frames_in_flight();
  const int frames_available = frames_in_buffer - frames_in_flight;
  const bool is_paused = playback_rate_.load(std::memory_order_relaxed) == 0.0f;
  if (!is_playing || is_paused || frames_available <= 0) {
    WriteSilence(dest, num_frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  // On underrun, play what's available and pad the rest with silence.
  const int frames_to_copy = std::min<int>(num_frames, frames_available);
  CopyFromFrameBuffer(
      (offset_in_frames + frames_in_flight) % frames_per_channel_,
      frames_to_copy, volume_.load(std::memory_order_relaxed), dest);
  WriteSilence(dest + frames_to_copy * channels_, num_frames - frames_to_copy);

  if (timestamps_unavailable_) {
    // AAudio doesn't provide timestamps: report frames as consumed when
    // copied. Media time then leads actual output by the output latency.
    callbacks_.consume_frames(frames_to_copy, CurrentMonotonicTime(), context_);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }
  presented_frames_.OnFramesCopied(stream_position, frames_to_copy);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AaudioAudioSink::ConsumePresentedFrames(AAudioStream* stream,
                                             int64_t stream_position) {
  if (timestamps_unavailable_ || presented_frames_.frames_in_flight() == 0) {
    return;
  }
  int64_t timestamp_position = 0;
  int64_t timestamp_ns = 0;
  const int64_t now_us = CurrentMonotonicTime();
  if (AAudio::Stream_GetTimestamp(stream, CLOCK_MONOTONIC, &timestamp_position,
                                  &timestamp_ns) == AAUDIO_OK) {
    timestamp_unavailable_since_us_ = 0;
    const int presented = presented_frames_.OnTimestamp(
        timestamp_position, timestamp_ns / 1'000, now_us, stream_position);
    if (presented > 0) {
      callbacks_.consume_frames(presented, now_us, context_);
    }
    return;
  }
  // Timestamps are unavailable for a short while after a stream starts.
  if (timestamp_unavailable_since_us_ == 0) {
    timestamp_unavailable_since_us_ = now_us;
    return;
  }
  if (now_us - timestamp_unavailable_since_us_ < kTimestampTimeoutUs) {
    return;
  }
  SB_LOG(WARNING) << "AAudio timestamps are unavailable, reporting frames as "
                     "consumed when copied.";
  timestamps_unavailable_ = true;
  callbacks_.consume_frames(presented_frames_.frames_in_flight(), now_us,
                            context_);
  presented_frames_.Reset();
}

// TODO: b/561166288 - When the output device changes (e.g. speaker to
// Bluetooth), AAudio may reroute without AAUDIO_ERROR_DISCONNECTED, so the
// stream isn't reopened and media time isn't resynced to the new latency.
// Detect it and flush, like AudioTrackAudioSink does on
// AudioDeviceChange::kResetAndContinue.
void AaudioAudioSink::ReportError(bool capability_changed,
                                  const std::string& message) {
  if (!callbacks_.error) {
    return;
  }
  callbacks_.error(capability_changed, message, context_);
}

void AaudioAudioSink::CopyFromFrameBuffer(int offset_in_frames,
                                          int num_frames,
                                          float volume,
                                          float* dest) const {
  SB_CHECK_GE(offset_in_frames, 0);
  SB_CHECK_LT(offset_in_frames, frames_per_channel_);
  SB_CHECK_LE(num_frames, frames_per_channel_);

  const float* src = static_cast<const float*>(frame_buffer_);
  const int frames_before_wrap =
      std::min(num_frames, frames_per_channel_ - offset_in_frames);
  const int frames_after_wrap = num_frames - frames_before_wrap;

  CopyWithVolume(src + offset_in_frames * channels_,
                 frames_before_wrap * channels_, volume, dest);
  if (frames_after_wrap <= 0) {
    return;
  }
  CopyWithVolume(src, frames_after_wrap * channels_, volume,
                 dest + frames_before_wrap * channels_);
}

void AaudioAudioSink::WriteSilence(float* dest, int num_frames) const {
  std::fill_n(dest, num_frames * channels_, 0.0f);
}

}  // namespace starboard
