// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>
#include <deque>
#include <functional>

#include "starboard/atomic.h"
#include "starboard/common/optional.h"
#include "starboard/common/queue.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/common/string.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/string.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using shared::starboard::player::video_dmp::VideoDmpReader;
using testing::FakeGraphicsContextProvider;
using ::testing::ValuesIn;

const SbTimeMonotonic kDefaultWaitForDecoderStateNeedsDataTimeout =
    5 * kSbTimeSecond;
const SbTimeMonotonic kDefaultWaitForPlayerStateTimeout = 5 * kSbTimeSecond;
const SbTimeMonotonic kDefaultWaitForCallbackEventTimeout =
    15 * kSbTimeMillisecond;

class SbPlayerWriteSampleTest
    : public ::testing::TestWithParam<SbPlayerTestConfig> {
 public:
  SbPlayerWriteSampleTest();

  void SetUp() override;
  void TearDown() override;

 protected:
  struct CallbackEvent {
    CallbackEvent() {}

    CallbackEvent(SbPlayer player, SbPlayerState state, int ticket)
        : player(player), player_state(state), ticket(ticket) {}

    CallbackEvent(SbPlayer player, SbPlayerDecoderState state, int ticket)
        : player(player), decoder_state(state), ticket(ticket) {}

    bool HasStateUpdate() const {
      return player_state.has_engaged() || decoder_state.has_engaged();
    }

    SbPlayer player = kSbPlayerInvalid;
    optional<SbPlayerState> player_state;
    optional<SbPlayerDecoderState> decoder_state;
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

  void InitializePlayer();

  void PrepareForSeek();

  void Seek(const SbTime time);

  // When the |output_mode| is decoding to texture, then this method is used to
  // advance the decoded frames.
  void GetDecodeTargetWhenSupported();

  // Callback methods from the underlying player.
  void OnDecoderState(SbPlayer player,
                      SbMediaType media_type,
                      SbPlayerDecoderState state,
                      int ticket);
  void OnPlayerState(SbPlayer player, SbPlayerState state, int ticket);
  void OnError(SbPlayer player, SbPlayerError error, const char* message);

  // Waits for |kSbPlayerDecoderStateNeedsData| to be sent.
  void WaitForDecoderStateNeedsData(
      const SbTime timeout = kDefaultWaitForDecoderStateNeedsDataTimeout);

  // Waits for desired player state update to be sent.
  void WaitForPlayerState(
      const SbPlayerState desired_state,
      const SbTime timeout = kDefaultWaitForPlayerStateTimeout);

  // Player and Decoder methods for driving input and output.
  void WriteSingleBatch(int start_index, int samples_to_write);
  void WriteEndOfStream();
  void WriteMultipleBatches(size_t start_index,
                            size_t number_of_write_sample_calls);
  void DrainOutputs();

  int GetNumBuffers() const;

  SbMediaType GetTestMediaType() const { return test_media_type_; }

  // Determine if the the current event is valid based on previously received
  // player state updates, or other inputs to the player.
  void AssertPlayerStateIsValid(SbPlayerState state) const;

  bool HasReceivedPlayerState(SbPlayerState state) const {
    return player_state_set_.find(state) != player_state_set_.end();
  }

  // Checks if there are pending callback events and, if so, logs the received
  // state update in the appropriate tracking container:
  // * |decoder_state_queue_| for SbPlayerDecoderState updates.
  // * |player_state_set_| for SbPlayerState updates.
  // Executes a blocking wait for any new CallbackEvents to be enqueued.
  void TryProcessCallbackEvents(SbTime timeout);

  // Queue of events from the underlying player.
  Queue<CallbackEvent> callback_event_queue_;

  // Ordered queue of pending decoder state updates.
  std::deque<SbPlayerDecoderState> decoder_state_queue_;

  // Set of received player state updates from the underlying player. This is
  // used to check that the state updates occur in a valid order during normal
  // playback.
  std::set<SbPlayerState> player_state_set_;

  // Test instance specific configuration.
  std::string dmp_filename_;
  SbMediaType test_media_type_;
  SbPlayerOutputMode output_mode_;
  scoped_ptr<VideoDmpReader> dmp_reader_;

  FakeGraphicsContextProvider fake_graphics_context_provider_;
  SbPlayer player_ = kSbPlayerInvalid;

  bool destroy_player_called_ = false;
  bool end_of_stream_written_ = false;
  atomic_bool error_occurred_;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;
};

SbPlayerWriteSampleTest::SbPlayerWriteSampleTest() {
  const char* audio_filename = std::get<0>(GetParam());
  const char* video_filename = std::get<1>(GetParam());
  output_mode_ = std::get<2>(GetParam());

  SB_DCHECK(output_mode_ != kSbPlayerOutputModeInvalid);

  if (audio_filename != NULL) {
    SB_DCHECK(video_filename == NULL);

    dmp_filename_ = audio_filename;
    test_media_type_ = kSbMediaTypeAudio;
  } else {
    SB_DCHECK(video_filename != NULL);

    dmp_filename_ = video_filename;
    test_media_type_ = kSbMediaTypeVideo;
  }
  dmp_reader_.reset(
      new VideoDmpReader(ResolveTestFileName(dmp_filename_.c_str()).c_str()));

  SB_LOG(INFO) << FormatString(
      "Initialize SbPlayerWriteSampleTest with dmp file '%s' and with output "
      "mode '%s'.",
      dmp_filename_.c_str(),
      output_mode_ == kSbPlayerOutputModeDecodeToTexture ? "Decode To Texture"
                                                         : "Punchout");
}

std::string GetSbPlayerTestConfigName(
    ::testing::TestParamInfo<SbPlayerTestConfig> info) {
  const char* audio_filename = std::get<0>(info.param);
  const char* video_filename = std::get<1>(info.param);
  SbPlayerOutputMode output_mode = std::get<2>(info.param);
  std::string name(FormatString(
      "audio_%s_video_%s_output_%s", audio_filename, video_filename,
      output_mode == kSbPlayerOutputModeDecodeToTexture ? "DecodeToTexture"
                                                        : "Punchout"));
  std::replace(name.begin(), name.end(), '.', '_');
  std::replace(name.begin(), name.end(), '(', '_');
  std::replace(name.begin(), name.end(), ')', '_');
  return name;
}

void SbPlayerWriteSampleTest::SetUp() {
  SbMediaVideoCodec video_codec = dmp_reader_->video_codec();
  SbMediaAudioCodec audio_codec = dmp_reader_->audio_codec();
  const SbMediaAudioSampleInfo* audio_sample_info = NULL;

  if (test_media_type_ == kSbMediaTypeAudio) {
    SB_DCHECK(audio_codec != kSbMediaAudioCodecNone);

    audio_sample_info = &dmp_reader_->audio_sample_info();
    video_codec = kSbMediaVideoCodecNone;
  } else {
    SB_DCHECK(video_codec != kSbMediaVideoCodecNone);

    audio_codec = kSbMediaAudioCodecNone;
  }

  player_ = CallSbPlayerCreate(
      fake_graphics_context_provider_.window(), video_codec, audio_codec,
      kSbDrmSystemInvalid, audio_sample_info, "", DummyDeallocateSampleFunc,
      DecoderStatusCallback, PlayerStatusCallback, ErrorCallback, this,
      output_mode_, fake_graphics_context_provider_.decoder_target_provider());

  ASSERT_TRUE(SbPlayerIsValid(player_));

  InitializePlayer();
}

void SbPlayerWriteSampleTest::TearDown() {
  if (!SbPlayerIsValid(player_)) {
    return;
  }

  ASSERT_FALSE(destroy_player_called_);
  destroy_player_called_ = true;
  SbPlayerDestroy(player_);

  // We expect the event to be sent already.
  ASSERT_NO_FATAL_FAILURE(WaitForPlayerState(kSbPlayerStateDestroyed, 0));
  ASSERT_FALSE(error_occurred_.load());
}

// static
void SbPlayerWriteSampleTest::DecoderStatusCallback(SbPlayer player,
                                                    void* context,
                                                    SbMediaType type,
                                                    SbPlayerDecoderState state,
                                                    int ticket) {
  SbPlayerWriteSampleTest* sb_player_write_sample_test =
      static_cast<SbPlayerWriteSampleTest*>(context);
  sb_player_write_sample_test->OnDecoderState(player, type, state, ticket);
}

// static
void SbPlayerWriteSampleTest::PlayerStatusCallback(SbPlayer player,
                                                   void* context,
                                                   SbPlayerState state,
                                                   int ticket) {
  SbPlayerWriteSampleTest* sb_player_write_sample_test =
      static_cast<SbPlayerWriteSampleTest*>(context);
  sb_player_write_sample_test->OnPlayerState(player, state, ticket);
}

// static
void SbPlayerWriteSampleTest::ErrorCallback(SbPlayer player,
                                            void* context,
                                            SbPlayerError error,
                                            const char* message) {
  SbPlayerWriteSampleTest* sb_player_write_sample_test =
      static_cast<SbPlayerWriteSampleTest*>(context);
  sb_player_write_sample_test->OnError(player, error, message);
}

void SbPlayerWriteSampleTest::InitializePlayer() {
  ASSERT_FALSE(destroy_player_called_);
  ASSERT_NO_FATAL_FAILURE(WaitForPlayerState(kSbPlayerStateInitialized));
  Seek(0);
  SbPlayerSetPlaybackRate(player_, 1.0);
  SbPlayerSetVolume(player_, 1.0);
}

void SbPlayerWriteSampleTest::PrepareForSeek() {
  ASSERT_FALSE(destroy_player_called_);
  ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
  ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStateInitialized));
  player_state_set_.clear();
  player_state_set_.insert(kSbPlayerStateInitialized);
  ticket_++;
}

void SbPlayerWriteSampleTest::Seek(const SbTime time) {
  PrepareForSeek();
  SbPlayerSeek2(player_, time, ticket_);
  ASSERT_NO_FATAL_FAILURE(WaitForDecoderStateNeedsData());
  ASSERT_TRUE(decoder_state_queue_.empty());
}

void SbPlayerWriteSampleTest::GetDecodeTargetWhenSupported() {
  if (destroy_player_called_) {
    return;
  }
#if SB_HAS(GLES2)
  fake_graphics_context_provider_.RunOnGlesContextThread([&]() {
    ASSERT_TRUE(SbPlayerIsValid(player_));
    if (output_mode_ != kSbPlayerOutputModeDecodeToTexture) {
      ASSERT_EQ(SbPlayerGetCurrentFrame(player_), kSbDecodeTargetInvalid);
      return;
    }
    ASSERT_EQ(output_mode_, kSbPlayerOutputModeDecodeToTexture);
    SbDecodeTarget frame = SbPlayerGetCurrentFrame(player_);
    if (SbDecodeTargetIsValid(frame)) {
      SbDecodeTargetRelease(frame);
    }
  });
#endif  // SB_HAS(GLES2)
}

void SbPlayerWriteSampleTest::OnDecoderState(SbPlayer player,
                                             SbMediaType type,
                                             SbPlayerDecoderState state,
                                             int ticket) {
  switch (state) {
    case kSbPlayerDecoderStateNeedsData:
      callback_event_queue_.Put(CallbackEvent(player, state, ticket));
      break;
  }
}

void SbPlayerWriteSampleTest::OnPlayerState(SbPlayer player,
                                            SbPlayerState state,
                                            int ticket) {
  callback_event_queue_.Put(CallbackEvent(player, state, ticket));
}

void SbPlayerWriteSampleTest::OnError(SbPlayer player,
                                      SbPlayerError error,
                                      const char* message) {
  SB_LOG(ERROR) << FormatString("Got SbPlayerError %d with message '%s'", error,
                                message != NULL ? message : "");
  error_occurred_.exchange(true);
}

void SbPlayerWriteSampleTest::WaitForDecoderStateNeedsData(
    const SbTime timeout) {
  optional<SbPlayerDecoderState> received_state;
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  do {
    ASSERT_FALSE(error_occurred_.load());
    GetDecodeTargetWhenSupported();
    ASSERT_NO_FATAL_FAILURE(TryProcessCallbackEvents(
        std::min(timeout, kDefaultWaitForCallbackEventTimeout)));
    if (decoder_state_queue_.empty()) {
      continue;
    }
    received_state = decoder_state_queue_.front();
    decoder_state_queue_.pop_front();
    if (received_state.value() == kSbPlayerDecoderStateNeedsData) {
      break;
    }
  } while (SbTimeGetMonotonicNow() - start < timeout);

  ASSERT_TRUE(received_state.has_engaged()) << "Did not receive any states.";
  ASSERT_EQ(kSbPlayerDecoderStateNeedsData, received_state.value())
      << "Did not receive expected state.";
}

void SbPlayerWriteSampleTest::WaitForPlayerState(
    const SbPlayerState desired_state,
    const SbTime timeout) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  do {
    ASSERT_FALSE(error_occurred_.load());
    GetDecodeTargetWhenSupported();
    ASSERT_NO_FATAL_FAILURE(TryProcessCallbackEvents(
        std::min(timeout, kDefaultWaitForCallbackEventTimeout)));
    if (HasReceivedPlayerState(desired_state)) {
      break;
    }
  } while (SbTimeGetMonotonicNow() - start < timeout);
  ASSERT_TRUE(HasReceivedPlayerState(desired_state))
      << "Did not received expected state.";
}

void SbPlayerWriteSampleTest::WriteSingleBatch(int start_index,
                                               int samples_to_write) {
  SB_DCHECK(samples_to_write > 0);
  SB_DCHECK(start_index >= 0);
  ASSERT_FALSE(destroy_player_called_);

  int max_batch_size =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, test_media_type_);
  SB_DCHECK(max_batch_size > 0);
  samples_to_write = std::min(samples_to_write, max_batch_size);

  SB_DCHECK(start_index + samples_to_write <= GetNumBuffers());
  // Prepare a batch writing.
  std::vector<SbPlayerSampleInfo> sample_infos;
  for (int i = 0; i < samples_to_write; ++i) {
    sample_infos.push_back(
        dmp_reader_->GetPlayerSampleInfo(test_media_type_, start_index++));
  }
  SbPlayerWriteSample2(player_, test_media_type_, sample_infos.data(),
                       samples_to_write);
}

void SbPlayerWriteSampleTest::WriteEndOfStream() {
  ASSERT_FALSE(destroy_player_called_);
  ASSERT_FALSE(end_of_stream_written_);
  end_of_stream_written_ = true;
  SbPlayerWriteEndOfStream(player_, test_media_type_);
}

void SbPlayerWriteSampleTest::WriteMultipleBatches(
    size_t start_index,
    size_t number_of_write_sample_calls) {
  SB_DCHECK(number_of_write_sample_calls > 0);
  SB_DCHECK(start_index < GetNumBuffers());
  ASSERT_FALSE(destroy_player_called_);

  int max_batch_size =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, test_media_type_);
  SB_DCHECK(max_batch_size > 0);

  int num_inputs_to_write = max_batch_size;
  int sample_index = start_index;
  for (int i = 0; i < number_of_write_sample_calls; ++i) {
    if (sample_index + num_inputs_to_write > GetNumBuffers()) {
      break;
    }
    ASSERT_NO_FATAL_FAILURE(
        WriteSingleBatch(sample_index, num_inputs_to_write));
    ASSERT_NO_FATAL_FAILURE(WaitForDecoderStateNeedsData());
    sample_index = sample_index + num_inputs_to_write;
    num_inputs_to_write =
        (num_inputs_to_write == 1) ? max_batch_size : num_inputs_to_write - 1;
  }
}

void SbPlayerWriteSampleTest::DrainOutputs() {
  ASSERT_TRUE(end_of_stream_written_);
  ASSERT_NO_FATAL_FAILURE(WaitForPlayerState(kSbPlayerStateEndOfStream));
  // We should not get any new decoder events after end of stream.
  ASSERT_TRUE(decoder_state_queue_.empty());
}

int SbPlayerWriteSampleTest::GetNumBuffers() const {
  return test_media_type_ == kSbMediaTypeAudio
             ? dmp_reader_->number_of_audio_buffers()
             : dmp_reader_->number_of_video_buffers();
}

void SbPlayerWriteSampleTest::AssertPlayerStateIsValid(
    SbPlayerState state) const {
  // Note: it is possible to receive the same state that has been previously
  // received in the case of multiple Seek() calls. Prior to any Seek commands
  // issued in this test, we should reset the |player_state_set_| member.
  ASSERT_FALSE(HasReceivedPlayerState(state));

  switch (state) {
    case kSbPlayerStateInitialized:
      // No other states have been received before getting Initialized.
      ASSERT_TRUE(player_state_set_.empty());
      break;
    case kSbPlayerStatePrerolling:
      ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStateInitialized));
      ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
      break;
    case kSbPlayerStatePresenting:
      ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStatePrerolling));
      ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
      break;
    case kSbPlayerStateEndOfStream:
      ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStateInitialized));
      ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStatePrerolling));
      ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
      break;
    case kSbPlayerStateDestroyed:
      // Nothing stops the user of the player from destroying the player during
      // any of the previous states.
      ASSERT_TRUE(destroy_player_called_);
      break;
  }
}

void SbPlayerWriteSampleTest::TryProcessCallbackEvents(SbTime timeout) {
  for (;;) {
    auto event = callback_event_queue_.GetTimed(timeout);
    if (!event.HasStateUpdate()) {
      break;
    }
    if (event.ticket != ticket_) {
      continue;
    }
    ASSERT_EQ(event.player, player_);
    SB_DCHECK(event.decoder_state.has_engaged() ^
              event.player_state.has_engaged());
    if (event.decoder_state.has_engaged()) {
      // Callbacks may be in-flight at the time that the player is destroyed by
      // a call to |SbPlayerDestroy|. In this case, the callbacks are ignored.
      // However no new callbacks are expected after receiving the player status
      // |kSbPlayerStateDestroyed|.
      ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
      decoder_state_queue_.push_back(event.decoder_state.value());
      continue;
    }
    ASSERT_NO_FATAL_FAILURE(
        AssertPlayerStateIsValid(event.player_state.value()));
    player_state_set_.insert(event.player_state.value());
  }
}

TEST_P(SbPlayerWriteSampleTest, SeekAndDestroy) {
  PrepareForSeek();
  SbPlayerSeek2(player_, 1 * kSbTimeSecond, ticket_);
}

TEST_P(SbPlayerWriteSampleTest, NoInput) {
  WriteEndOfStream();
  DrainOutputs();
}

// A single call to write a batch consists of multiple samples.
TEST_P(SbPlayerWriteSampleTest, WriteSingleBatch) {
  int max_batch_size =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, test_media_type_);
  ASSERT_NO_FATAL_FAILURE(
      WriteSingleBatch(0, std::min<size_t>(max_batch_size, GetNumBuffers())));
  ASSERT_NO_FATAL_FAILURE(WaitForDecoderStateNeedsData());
  WriteEndOfStream();
  DrainOutputs();
}

TEST_P(SbPlayerWriteSampleTest, WriteMultipleBatches) {
  ASSERT_NO_FATAL_FAILURE(WriteMultipleBatches(0, 8));
  WriteEndOfStream();
  DrainOutputs();
}

INSTANTIATE_TEST_CASE_P(SbPlayerWriteSampleTests,
                        SbPlayerWriteSampleTest,
                        ValuesIn(GetSupportedSbPlayerTestConfigs()),
                        GetSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
