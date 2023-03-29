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
#include <set>

#include "starboard/common/queue.h"
#include "starboard/common/scoped_ptr.h"
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
  class GroupedSamples {
   public:
    int start_index() const { return start_index_; }
    int samples_to_write() const { return samples_to_write_; }
    bool write_eos() const { return write_eos_; }

   protected:
    GroupedSamples(SbMediaType media_type,
                   int start_index,
                   int samples_to_write)
        : media_type_(media_type),
          start_index_(start_index),
          samples_to_write_(samples_to_write) {}

    SbMediaType media_type_;
    int start_index_ = 0;
    int samples_to_write_ = 0;
    bool write_eos_ = false;
  };

  class AudioSamples : public GroupedSamples {
   public:
    AudioSamples() : GroupedSamples(kSbMediaTypeAudio, 0, 0) {}
    AudioSamples(int start_index, int samples_to_write)
        : GroupedSamples(kSbMediaTypeAudio, start_index, samples_to_write) {}

    AudioSamples& WithEOS() {
      write_eos_ = true;
      return *this;
    }
  };

  class VideoSamples : public GroupedSamples {
   public:
    VideoSamples() : GroupedSamples(kSbMediaTypeVideo, 0, 0) {}
    VideoSamples(int start_index, int samples_to_write)
        : GroupedSamples(kSbMediaTypeVideo, start_index, samples_to_write) {}

    VideoSamples& WithEOS() {
      write_eos_ = true;
      return *this;
    }
  };

  class AudioEOS : public AudioSamples {
   public:
    AudioEOS() { write_eos_ = true; }
  };

  class VideoEOS : public VideoSamples {
   public:
    VideoEOS() { write_eos_ = true; }
  };

  explicit SbPlayerTestFixture(const SbPlayerTestConfig& config);
  ~SbPlayerTestFixture();

  void Seek(const SbTime time);
  // Write audio and video samples. It waits for
  // |kSbPlayerDecoderStateNeedsData| internally. When writing EOS are
  // requested, the function will write EOS after all samples of the same type
  // are written.
  void Write(const AudioSamples& audio_samples);
  void Write(const VideoSamples& video_samples);
  void Write(const AudioSamples& audio_samples,
             const VideoSamples& video_samples);
  // Wait until kSbPlayerStateEndOfStream received.
  void WaitForPlayerEndOfStream();

  SbPlayer GetPlayer() { return player_; }

 private:
  static constexpr SbTime kDefaultWaitForDecoderStateNeedsDataTimeout =
      5 * kSbTimeSecond;
  static constexpr SbTime kDefaultWaitForPlayerStateTimeout = 5 * kSbTimeSecond;
  static constexpr SbTime kDefaultWaitForCallbackEventTimeout =
      15 * kSbTimeMillisecond;

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

  void InitializePlayer();
  void TearDownPlayer();

  void WriteSamples(SbMediaType media_type,
                    int start_index,
                    int samples_to_write);
  void WriteEndOfStream(SbMediaType media_type);

  // Checks if there are pending callback events and, if so, logs the received
  // state update in the appropriate tracking container:
  // * |can_accept_more_*_data_| for SbPlayerDecoderState updates.
  // * |player_state_set_| for SbPlayerState updates.
  // Executes a blocking wait for any new CallbackEvents to be enqueued.
  void WaitAndProcessNextEvent(
      SbTime timeout = kDefaultWaitForCallbackEventTimeout);

  // Waits for |kSbPlayerDecoderStateNeedsData| to be sent.
  void WaitForDecoderStateNeedsData(
      const SbTime timeout = kDefaultWaitForDecoderStateNeedsDataTimeout);

  // Waits for desired player state update to be sent.
  void WaitForPlayerState(
      const SbPlayerState desired_state,
      const SbTime timeout = kDefaultWaitForPlayerStateTimeout);

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
  scoped_ptr<VideoDmpReader> audio_dmp_reader_;
  scoped_ptr<VideoDmpReader> video_dmp_reader_;
  testing::FakeGraphicsContextProvider fake_graphics_context_provider_;

  SbPlayer player_ = kSbPlayerInvalid;

  // Queue of events from the underlying player.
  Queue<CallbackEvent> callback_event_queue_;

  // States of if decoder can accept more inputs.
  bool can_accept_more_audio_data_ = false;
  bool can_accept_more_video_data_ = false;

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
