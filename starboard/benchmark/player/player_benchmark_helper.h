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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"

namespace starboard {
namespace benchmark {

class SbPlayerBenchmarkHelper {
 public:
  // Default upper bound for the waits on player callbacks. Hitting it means
  // that the player has stalled, so the benchmark should report an error
  // rather than hang.
  static constexpr int64_t kDefaultTimeoutUs = 5'000'000;  // 5 seconds

  SbPlayerBenchmarkHelper();
  ~SbPlayerBenchmarkHelper();

  // Initialize the player with specific audio and video DMP files.
  // Returns true on success.
  bool InitializePlayer(const char* audio_file,
                        const char* video_file,
                        SbPlayerOutputMode output_mode);

  // Safely destroy the player and wait for it to be fully released.
  void DestroyPlayer();

  // Generate creation parameters for standard AVC and AAC playback
  void GetCreationParam(SbPlayerCreationParam* out_creation_param,
                        SbPlayerOutputMode output_mode);

  // Feed audio/video samples until a sample at or past |target_media_time_us|
  // (in microseconds) has been written for every stream, or EOS is reached.
  // Blocks the calling thread. Returns false if the player reports an error,
  // or if it does not request more data for |timeout_us|.
  bool FeedData(int64_t target_media_time_us,
                int64_t timeout_us = kDefaultTimeoutUs);

  // Prepare seek by resetting DMP reader index and return the next ticket.
  // The caller then calls SbPlayerSeek() itself, so that it can time it.
  int PrepareSeek(int64_t seek_time_us);

  // Wait for the player to reach a specific state for a given ticket.
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

  // Player callbacks. The Dummy*Func() callbacks ignore their context and can
  // be used by benchmarks that do not track the player state; the *Callback()
  // callbacks expect a pointer to the owning SbPlayerBenchmarkHelper as the
  // context.
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

 private:
  // Seeks the player to |seek_time_us| and prepares the readers accordingly.
  void Seek(int64_t seek_time_us);
  void WriteSamples(SbMediaType type);
  size_t FindAudioStartIndex(int64_t target_time_us);
  size_t FindVideoStartIndex(int64_t target_time_us);
  starboard::FakeGraphicsContextProvider fake_graphics_context_provider_;

  SbPlayer player_ = kSbPlayerInvalid;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;

  std::unique_ptr<VideoDmpReader> audio_reader_;
  std::unique_ptr<VideoDmpReader> video_reader_;

  size_t next_audio_sample_index_ = 0;
  size_t next_video_sample_index_ = 0;

  // Timestamps of the last samples written since the last seek, if any.
  std::optional<int64_t> last_written_audio_timestamp_;
  std::optional<int64_t> last_written_video_timestamp_;

  bool audio_eos_written_ = false;
  bool video_eos_written_ = false;

  // Thread synchronization and state tracking. All the state below is guarded
  // by |mutex_|, and every change is signalled through |cv_|.
  std::mutex mutex_;
  std::condition_variable cv_;

  SbPlayerState player_state_ = kSbPlayerStateInitialized;
  int player_state_ticket_ = 0;

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
  std::condition_variable cv_;
  bool initialized_ = false;
  bool error_occurred_ = false;
};

}  // namespace benchmark
}  // namespace starboard

#endif  // STARBOARD_BENCHMARK_PLAYER_PLAYER_BENCHMARK_HELPER_H_
