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
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

#include "starboard/extension/experimental/experimental_features.h"
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
// that a benchmark can report them with SkipWithError() (note that missing
// testdata .dmp files still fail fast via VideoDmpReader's SB_CHECKs).
//
// Must be used from a single thread (enforced by |thread_checker_|); the
// player callbacks may arrive on any thread and are synchronized through
// |mutex_|.
//
// TODO(b/541298090): Much of this duplicates SbPlayerTestFixture and the
// helpers in starboard/nplb/player_test_util.* and
// starboard/nplb/player_creation_param_helpers.*. Move the shared parts into a
// library under starboard/testing/ that both nplb and the benchmarks use.
class SbPlayerBenchmarkHelper {
 public:
  using Clock = std::chrono::steady_clock;

  enum class InitResult {
    kOk,
    kOutputModeNotSupported,
    kFailed,
  };

  // Default upper bound for the waits on player callbacks. Hitting it means
  // that the player has stalled, so the benchmark should report an error
  // rather than hang.
  static constexpr int64_t kDefaultTimeoutUs = 5'000'000;  // 5 seconds

  SbPlayerBenchmarkHelper();
  ~SbPlayerBenchmarkHelper();

  // Sets experimental feature entries applied before SbPlayerCreate() and
  // updates the current thread's configuration.
  void SetExperimentalFeatures(
      std::vector<StarboardExperimentalFeatureEntry> features);

  // Configures Starboard experimental features on the calling thread. Called
  // automatically by the constructor and InitializePlayer(); call explicitly
  // before invoking SbPlayerCreate() directly from another thread.
  void ConfigureExperimentalFeaturesForCurrentThread() const;

  // Initializes the player with specific audio and video DMP files.
  InitResult InitializePlayer(const char* audio_file,
                              const char* video_file,
                              SbPlayerOutputMode output_mode);

  // Safely destroys the player and releases loaded DMP readers.
  void DestroyPlayer();

  // Generates creation parameters for standard 1080p AVC and stereo AAC
  // playback.
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
  // Returns false if the player reports an error or stalls for |timeout_us|.
  bool FeedUntilPresenting(Clock::time_point* out_presenting_time = nullptr,
                           int64_t timeout_us = kDefaultTimeoutUs);

  // Prepares a seek by resetting DMP reader indices and returning the next
  // ticket. The caller then calls SbPlayerSeek() itself so that it can time it.
  int PrepareSeek(int64_t seek_time_us);

  // Waits for the player to reach |state| for |ticket|.
  // Returns false if the player reports an error or the timeout expires.
  bool WaitForPlayerState(SbPlayerState state,
                          int ticket,
                          int64_t timeout_us = kDefaultTimeoutUs);

  SbPlayer GetPlayer() const { return player_; }
  int GetCurrentTicket() const { return ticket_; }

  SbWindow GetWindow() { return fake_graphics_context_provider_.window(); }
  SbDecodeTargetGraphicsContextProvider* GetDecoderTargetProvider() {
    return fake_graphics_context_provider_.decoder_target_provider();
  }

  // No-op player callbacks that ignore their context, for benchmarks that do
  // not track player or decoder state.
  static void DummyDeallocateSampleFunc(SbPlayer player,
                                        void* context,
                                        const void* sample_buffer);
  static void DummyDecoderStatusFunc(SbPlayer player,
                                     void* context,
                                     SbMediaType type,
                                     SbPlayerDecoderState state,
                                     int ticket) {}
  static void DummyPlayerStatusFunc(SbPlayer player,
                                    void* context,
                                    SbPlayerState state,
                                    int ticket) {}
  static void DummyPlayerErrorFunc(SbPlayer player,
                                   void* context,
                                   SbPlayerError error,
                                   const char* message);

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

  // Seeks the player to |seek_time_us| and prepares the readers accordingly.
  void Seek(int64_t seek_time_us);
  void WriteSamples(SbMediaType type);
  void PumpDecodeTarget();
  size_t FindAudioStartIndex(int64_t target_time_us);
  size_t FindVideoStartIndex(int64_t target_time_us);

  ThreadChecker thread_checker_;
  FakeGraphicsContextProvider fake_graphics_context_provider_;
  std::vector<StarboardExperimentalFeatureEntry> experimental_features_;

  // Mutated only on |thread_checker_| under |mutex_|; safe to read without
  // |mutex_| on |thread_checker_|, and read under |mutex_| in callbacks.
  SbPlayer player_ = kSbPlayerInvalid;
  SbPlayerOutputMode output_mode_ = kSbPlayerOutputModeInvalid;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;

  std::unique_ptr<VideoDmpReader> audio_reader_;
  std::unique_ptr<VideoDmpReader> video_reader_;

  size_t next_audio_sample_index_ = 0;
  size_t next_video_sample_index_ = 0;

  bool audio_eos_written_ = false;
  bool video_eos_written_ = false;

  // Thread synchronization and state tracking. All the state below is guarded
  // by |mutex_|, and every change is signalled through |condition_variable_|.
  std::mutex mutex_;
  std::condition_variable condition_variable_;

  std::set<SbPlayerState> received_player_states_;
  std::optional<Clock::time_point> presenting_time_;

  bool audio_needs_data_ = false;
  bool video_needs_data_ = false;

  bool error_occurred_ = false;
};

// Tracks the asynchronous initialization of a player created with
// PlayerStatusFunc() and PlayerErrorFunc() as its status and error callbacks,
// and a pointer to this object as the callback context. The object has to
// outlive the player, as the callbacks may be invoked until SbPlayerDestroy()
// returns.
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
  bool WaitForInitialization(
      int64_t timeout_us = SbPlayerBenchmarkHelper::kDefaultTimeoutUs);

 private:
  std::mutex mutex_;
  std::condition_variable condition_variable_;
  bool initialized_ = false;
  bool error_occurred_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_BENCHMARK_PLAYER_PLAYER_BENCHMARK_HELPER_H_
