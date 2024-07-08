// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_PLAYER_TEST_FIXTURE_H_
#define STARBOARD_NPLB_PLAYER_TEST_FIXTURE_H_

#include <atomic>
#include <deque>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "starboard/common/queue.h"
#include "starboard/drm.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/testing/fake_graphics_context_provider.h"

namespace starboard {
namespace nplb {

class SbPlayerTestFixture {
 public:
  // A simple encapsulation of grouped samples.
  class GroupedSamplesIterator;
  class GroupedSamples {
   public:
    struct AudioSamplesDescriptor {
      int start_index = 0;
      int samples_count = 0;
      int64_t timestamp_offset = 0;
      int64_t discarded_duration_from_front = 0;
      int64_t discarded_duration_from_back = 0;
      bool is_end_of_stream = false;
    };

    struct VideoSamplesDescriptor {
      int start_index = 0;
      int samples_count = 0;
      bool is_end_of_stream = false;
    };

    GroupedSamples& AddAudioSamples(int start_index, int number_of_samples);
    GroupedSamples& AddAudioSamples(int start_index,
                                    int number_of_samples,
                                    int64_t timestamp_offset,
                                    int64_t discarded_duration_from_front,
                                    int64_t discarded_duration_from_back);
    GroupedSamples& AddAudioEOS();
    GroupedSamples& AddVideoSamples(int start_index, int number_of_samples);
    GroupedSamples& AddVideoEOS();

    friend class GroupedSamplesIterator;

   private:
    std::vector<AudioSamplesDescriptor> audio_samples_;
    std::vector<VideoSamplesDescriptor> video_samples_;
  };

  SbPlayerTestFixture(
      const SbPlayerTestConfig& config,
      testing::FakeGraphicsContextProvider* fake_graphics_context_provider);
  ~SbPlayerTestFixture();

  void Seek(const int64_t time);
  // Write audio and video samples. It waits for
  // |kSbPlayerDecoderStateNeedsData| internally. When writing EOS are
  // requested, the function will write EOS after all samples of the same type
  // are written.
  void Write(const GroupedSamples& grouped_samples);
  // Wait until kSbPlayerStatePresenting received.
  void WaitForPlayerPresenting();
  // Wait until kSbPlayerStateEndOfStream received.
  void WaitForPlayerEndOfStream();
  int64_t GetCurrentMediaTime() const;

  void SetAudioWriteDuration(int64_t duration);

  SbPlayer GetPlayer() const { return player_; }
  bool HasAudio() const { return audio_dmp_reader_ != nullptr; }
  bool HasVideo() const { return video_dmp_reader_ != nullptr; }

  int64_t GetAudioSampleTimestamp(int index) const;
  int ConvertDurationToAudioBufferCount(int64_t duration) const;
  int ConvertDurationToVideoBufferCount(int64_t duration) const;

 private:
  static constexpr int64_t kDefaultWaitForPlayerStateTimeout = 5'000'000LL;
  static constexpr int64_t kDefaultWaitForCallbackEventTimeout = 15'000;

  typedef shared::starboard::player::video_dmp::VideoDmpReader VideoDmpReader;

  typedef enum CallbackEventType {
    kEmptyEvent,
    kDecoderStateEvent,
    kPlayerStateEvent,
  } CallbackEventType;

  struct CallbackEvent {
    CallbackEvent();
    CallbackEvent(SbPlayer player,
                  SbMediaType type,
                  SbPlayerDecoderState state,
                  int ticket);
    CallbackEvent(SbPlayer player, SbPlayerState state, int ticket);

    CallbackEventType event_type;
    SbPlayer player = kSbPlayerInvalid;
    SbMediaType media_type;
    SbPlayerDecoderState decoder_state;
    SbPlayerState player_state;
    int ticket = SB_PLAYER_INITIAL_TICKET;
  };

  static void DecoderStatusCallback(SbPlayer player,
                                    void* context,
                                    SbMediaType type,
                                    SbPlayerDecoderState state,
                                    int ticket);

  static void PlayerStatusCallback(SbPlayer player,
                                   void* context,
                                   SbPlayerState state,
                                   int ticket);

  static void ErrorCallback(SbPlayer player,
                            void* context,
                            SbPlayerError error,
                            const char* message);

  // Callback methods from the underlying player. Note that callbacks may be
  // invoked on underlying player's thread.
  void OnDecoderState(SbPlayer player,
                      SbMediaType media_type,
                      SbPlayerDecoderState state,
                      int ticket);
  void OnPlayerState(SbPlayer player, SbPlayerState state, int ticket);
  void OnError(SbPlayer player, SbPlayerError error, const char* message);

  void Initialize();
  void TearDown();

  bool CanWriteMoreAudioData();
  bool CanWriteMoreVideoData();

  void WriteAudioSamples(int start_index,
                         int samples_to_write,
                         int64_t timestamp_offset,
                         int64_t discarded_duration_from_front,
                         int64_t discarded_duration_from_back);
  void WriteVideoSamples(int start_index, int samples_to_write);
  void WriteEndOfStream(SbMediaType media_type);

  // Checks if there are pending callback events and, if so, logs the received
  // state update in the appropriate tracking container:
  // * |can_accept_more_*_data_| for SbPlayerDecoderState updates.
  // * |player_state_set_| for SbPlayerState updates.
  // Executes a blocking wait for any new CallbackEvents to be enqueued.
  void WaitAndProcessNextEvent(
      int64_t timeout = kDefaultWaitForCallbackEventTimeout);

  // Waits for |kSbPlayerDecoderStateNeedsData| to be sent.
  void WaitForDecoderStateNeedsData(
      const int64_t timeout = kDefaultWaitForCallbackEventTimeout);

  // Waits for desired player state update to be sent.
  void WaitForPlayerState(
      const SbPlayerState desired_state,
      const int64_t timeout = kDefaultWaitForPlayerStateTimeout);

  // When the |output_mode| is decoding to texture, then this method is used to
  // advance the decoded frames.
  void GetDecodeTargetWhenSupported();

  // Determine if the the current event is valid based on previously received
  // player state updates, or other inputs to the player.
  void AssertPlayerStateIsValid(SbPlayerState state) const;

  bool HasReceivedPlayerState(SbPlayerState state) const {
    return player_state_set_.find(state) != player_state_set_.end();
  }

  shared::starboard::ThreadChecker thread_checker_;
  const SbPlayerOutputMode output_mode_;
  std::string key_system_;
  std::string max_video_capabilities_;
  std::unique_ptr<VideoDmpReader> audio_dmp_reader_;
  std::unique_ptr<VideoDmpReader> video_dmp_reader_;
  testing::FakeGraphicsContextProvider* fake_graphics_context_provider_;

  SbPlayer player_ = kSbPlayerInvalid;
  SbDrmSystem drm_system_ = kSbDrmSystemInvalid;

  // Queue of events from the underlying player.
  Queue<CallbackEvent> callback_event_queue_;

  // States of if decoder can accept more inputs.
  bool can_accept_more_audio_data_ = false;
  bool can_accept_more_video_data_ = false;

  // The duration of how far past the current playback position we will write
  // audio samples, in microseconds.
  int64_t audio_write_duration_ = 0;
  int64_t last_written_audio_timestamp_ = 0;

  // Set of received player state updates from the underlying player. This is
  // used to check that the state updates occur in a valid order during normal
  // playback.
  std::set<SbPlayerState> player_state_set_;

  bool destroy_player_called_ = false;
  bool audio_end_of_stream_written_ = false;
  bool video_end_of_stream_written_ = false;
  std::atomic_bool error_occurred_{false};
  int ticket_ = SB_PLAYER_INITIAL_TICKET;
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_PLAYER_TEST_FIXTURE_H_
