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

#ifndef STARBOARD_BENCHMARK_PLAYER_PLAYER_BENCHMARK_HELPER_H_
#define STARBOARD_BENCHMARK_PLAYER_PLAYER_BENCHMARK_HELPER_H_

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>

#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/testing/fake_graphics_context_provider.h"

namespace starboard {

// Drives an SbPlayer for the player benchmarks: creates it from .dmp files,
// feeds it samples until it presents, seeks it, and waits for player states,
// so that the benchmarks only contain the calls they time. All waits on player
// callbacks are bounded and report failures through their return values, so
// that a benchmark can report them with SkipWithError() (missing testdata .dmp
// files are reported the same way, while corrupt ones still fail fast via
// VideoDmpReader's SB_CHECKs).
//
// Lifetime and ownership: Typically instantiated on the stack by a benchmark
// function for the duration of that benchmark. Owns its
// FakeGraphicsContextProvider (unless an external one is passed to the
// constructor), any SbPlayer created via InitializePlayer(), and all cached
// VideoDmpReader instances, destroying the player in DestroyPlayer() or on
// destruction and the readers on destruction (benchmarks that call
// SbPlayerCreate() directly with GetCreationParam() own that SbPlayer
// themselves).
//
// Threading model: Thread-affine; all public non-static methods must be called
// on the construction thread (enforced by |thread_checker_|). Registered
// SbPlayer callbacks may arrive on any thread and synchronize through |mutex_|.
//
// TODO(b/541298090): Much of this duplicates SbPlayerTestFixture and the
// helpers in starboard/nplb/player_test_util.* and
// starboard/nplb/player_creation_param_helpers.*. Move the shared parts into a
// library under starboard/testing/ that both nplb and the benchmarks use.
class SbPlayerBenchmarkHelper {
 public:
  // Benchmarks use manual timing (UseManualTime()) measured with this clock.
  using Clock = std::chrono::steady_clock;

  enum class InitResult {
    kOk,
    kOutputModeNotSupported,
    kFailed,
  };

  // Default upper bound for the waits on player callbacks. Hitting it means
  // that the player has stalled, so the benchmark should report an error
  // rather than hang.
  static constexpr Clock::duration kDefaultTimeout = std::chrono::seconds(5);

  // Timestamps recorded by InitializePlayer(). |created| and |initialized| are
  // only set if the corresponding step succeeded; |initialized| is recorded
  // inside the status callback, like the presenting time.
  struct InitializeTimes {
    Clock::time_point create_start;
    std::optional<Clock::time_point> created;
    std::optional<Clock::time_point> initialized;
  };

  // The first error reported by the player through its error callback.
  struct PlayerError {
    SbPlayerError error;
    std::string message;
  };

  // If |graphics_context_provider| is null, the helper creates and owns its own
  // FakeGraphicsContextProvider; otherwise it shares the caller's (which must
  // outlive this helper) so multiple decode-to-texture players can coexist on
  // the same EGL display and GLES context thread.
  explicit SbPlayerBenchmarkHelper(
      FakeGraphicsContextProvider* graphics_context_provider = nullptr);
  ~SbPlayerBenchmarkHelper();

  // Loads and caches the audio and video DMP files (a null file means no stream
  // of that type) and selects them as the active files, reusing any file that
  // was already loaded on this helper. Cached files stay in memory across
  // DestroyPlayer() and subsequent LoadDmpFiles() calls so switching between
  // preloaded clips does not touch the filesystem. Returns false and sets
  // |out_error| if a file cannot be found or has no samples of its type. Must
  // not be called while a player exists, as the player may still reference
  // samples of the loaded files.
  bool LoadDmpFiles(const char* audio_file,
                    const char* video_file,
                    std::string* out_error);

  // Returns true if |output_mode| is supported for the loaded DMP files.
  bool IsOutputModeSupportedForDmpFiles(SbPlayerOutputMode output_mode);

  // Returns the playable duration (in microseconds) of the active DMP files, or
  // the shorter of audio and video when both are loaded.
  int64_t GetDurationUs() const;

  // Initializes the player with specific audio and video DMP files, loading
  // them with LoadDmpFiles() first. If |out_times| is non-null, it receives
  // the timestamps of the creation steps. If the player was created but did
  // not initialize, it is left for DestroyPlayer() (or the destructor), so
  // that the caller can time its destruction.
  InitResult InitializePlayer(const char* audio_file,
                              const char* video_file,
                              SbPlayerOutputMode output_mode,
                              InitializeTimes* out_times = nullptr);

  // Safely destroys the player, if any.
  void DestroyPlayer();

  // Sets the Starboard experimental features (key and integer value pairs) for
  // the players created from now on; the constructor sets none.
  // SbPlayerCreate() reads the features of the calling thread, so this must be
  // called on the thread that initializes the players. Starboard ignores keys
  // it does not know. Returns false if the platform does not support
  // experimental features.
  bool SetExperimentalFeatures(const std::map<std::string, int64_t>& features);

  // Returns the first error reported by the current player, if any.
  std::optional<PlayerError> GetPlayerError();

  // Generates synthetic creation parameters for standard 1920x1080 AVC and
  // stereo AAC playback (used by the create/destroy benchmarks; the seek
  // benchmarks derive stream parameters from their .dmp files instead).
  // TODO(b/541298090): Expand this to other formats/codecs as well as
  // audio-only and video-only configurations.
  static void GetCreationParam(SbPlayerCreationParam* out_creation_param,
                               SbPlayerOutputMode output_mode);

  // Returns true if |creation_param.output_mode| is supported by the platform
  // for |creation_param|.
  static bool IsOutputModeSupported(
      const SbPlayerCreationParam& creation_param);

  // Feeds audio/video samples on demand (and polls SbPlayerGetCurrentFrame() in
  // decode-to-texture mode) until kSbPlayerStatePresenting is received for the
  // current ticket. If |out_presenting_time| is non-null, stores the timestamp
  // recorded inside the status callback when kSbPlayerStatePresenting arrived.
  // Returns false if the player reports an error or stalls for |timeout|.
  bool FeedUntilPresenting(Clock::time_point* out_presenting_time = nullptr,
                           Clock::duration timeout = kDefaultTimeout);

  // Prepares a seek by resetting DMP reader indices and returning the next
  // ticket. The caller then calls SbPlayerSeek() itself so that it can time it.
  int PrepareSeek(int64_t seek_time_us);

  // Waits for the player to reach |state| for |ticket|.
  // Returns false if the player reports an error or the timeout expires.
  bool WaitForPlayerState(SbPlayerState state,
                          int ticket,
                          Clock::duration timeout = kDefaultTimeout);

  SbPlayer GetPlayer() const { return player_; }

  FakeGraphicsContextProvider* graphics_context_provider() {
    return graphics_context_provider_;
  }
  SbWindow GetWindow() { return graphics_context_provider_->window(); }
  SbDecodeTargetGraphicsContextProvider* GetDecoderTargetProvider() {
    return graphics_context_provider_->decoder_target_provider();
  }

  // No-op sample/decoder callbacks that ignore their context, for benchmarks
  // that do not feed samples or track decoder readiness.
  static void DummyDeallocateSampleFunc(SbPlayer player,
                                        void* context,
                                        const void* sample_buffer);
  static void DummyDecoderStatusFunc(SbPlayer player,
                                     void* context,
                                     SbMediaType type,
                                     SbPlayerDecoderState state,
                                     int ticket) {}

 private:
  static void DecoderStatusCallback(SbPlayer player,
                                    void* context,
                                    SbMediaType type,
                                    SbPlayerDecoderState state,
                                    int ticket);
  static void PlayerStatusCallback(SbPlayer player,
                                   void* context,
                                   SbPlayerState state,
                                   int ticket);
  static void PlayerErrorCallback(SbPlayer player,
                                  void* context,
                                  SbPlayerError error,
                                  const char* message);

  bool ConfigureExperimentalFeaturesForCurrentThread(
      const std::map<std::string, int64_t>& features) const;
  void GetCreationParamForDmpFiles(SbPlayerOutputMode output_mode,
                                   SbPlayerCreationParam* out_creation_param);
  void Seek(int64_t seek_time_us);
  void WriteSamples(SbMediaType type);
  void PumpDecodeTarget();
  size_t FindAudioStartIndex(int64_t target_time_us);
  size_t FindVideoStartIndex(int64_t target_time_us);

  struct LoadedDmp {
    std::unique_ptr<VideoDmpReader> reader;
    std::string mime;
  };

  ThreadChecker thread_checker_;
  std::unique_ptr<FakeGraphicsContextProvider> owned_graphics_context_provider_;
  FakeGraphicsContextProvider* const graphics_context_provider_;

  // Accessed only on |thread_checker_|.
  SbPlayer player_ = kSbPlayerInvalid;
  SbPlayerOutputMode output_mode_ = kSbPlayerOutputModeInvalid;

  std::map<std::string, LoadedDmp> loaded_audio_dmps_;
  std::map<std::string, LoadedDmp> loaded_video_dmps_;
  VideoDmpReader* audio_reader_ = nullptr;
  VideoDmpReader* video_reader_ = nullptr;
  // Point into |loaded_*_dmps_| and are referenced by creation parameters.
  const std::string* audio_mime_ = nullptr;
  const std::string* video_mime_ = nullptr;

  size_t next_audio_sample_index_ = 0;
  size_t next_video_sample_index_ = 0;

  bool audio_eos_written_ = false;
  bool video_eos_written_ = false;

  // Thread synchronization and callback state tracking. All the state below is
  // guarded by |mutex_|, and every callback state change is signalled through
  // |condition_variable_|.
  std::mutex mutex_;
  std::condition_variable condition_variable_;

  int ticket_ = SB_PLAYER_INITIAL_TICKET;
  std::set<SbPlayerState> received_player_states_;
  std::optional<Clock::time_point> initialized_time_;
  std::optional<Clock::time_point> presenting_time_;

  bool audio_needs_data_ = false;
  bool video_needs_data_ = false;

  bool error_occurred_ = false;
  std::optional<PlayerError> player_error_;
};

// Tracks the asynchronous initialization and error state of an SbPlayer
// created directly by the create/destroy benchmarks with PlayerStatusFunc() and
// PlayerErrorFunc() as its callbacks and a pointer to this object as |context|.
//
// Lifetime and ownership: Stack-owned by the caller creating the SbPlayer (does
// not own the SbPlayer) and must outlive the player until SbPlayerDestroy()
// returns, as callbacks may be invoked until teardown completes.
//
// Threading model: Thread-safe via |mutex_|. PlayerStatusFunc() and
// PlayerErrorFunc() may be invoked from any thread (such as the player worker
// thread), while WaitForInitialization() and HasError() are called on the
// benchmark thread.
class PlayerInitializationWaiter {
 public:
  static void PlayerStatusFunc(SbPlayer player,
                               void* context,
                               SbPlayerState state,
                               int ticket);
  static void PlayerErrorFunc(SbPlayer player,
                              void* context,
                              SbPlayerError error,
                              const char* message);

  // Blocks until the player reports kSbPlayerStateInitialized, reports an
  // error, or the timeout expires. Returns true only if the player was
  // initialized without an error.
  bool WaitForInitialization(SbPlayerBenchmarkHelper::Clock::duration timeout =
                                 SbPlayerBenchmarkHelper::kDefaultTimeout);

  bool HasError() const;

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_variable_;
  bool initialized_ = false;
  bool error_occurred_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_BENCHMARK_PLAYER_PLAYER_BENCHMARK_HELPER_H_
