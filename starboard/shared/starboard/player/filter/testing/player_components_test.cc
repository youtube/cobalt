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

#include "starboard/shared/starboard/player/filter/player_components.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

using ::starboard::testing::FakeGraphicsContextProvider;
using std::placeholders::_1;
using std::placeholders::_2;
using std::string;
using std::unique_ptr;
using std::vector;
using ::testing::ValuesIn;
using video_dmp::VideoDmpReader;

typedef VideoDmpReader::AudioAccessUnit AudioAccessUnit;
typedef VideoDmpReader::VideoAccessUnit VideoAccessUnit;
typedef PlayerComponents::Factory::CreationParameters CreationParameters;
typedef std::tuple<const char*, const char*, SbPlayerOutputMode>
    PlayerComponentsTestParam;

const SbTimeMonotonic kDefaultPrerollTimeOut = 5 * kSbTimeSecond;
const SbTimeMonotonic kDefaultEndTimeOut = 5 * kSbTimeSecond;
const SbTimeMonotonic kDefaultWriteTimeOut = 5 * kSbTimeSecond;

const SbPlayer kDummyPlayer = reinterpret_cast<SbPlayer>(1);

class PlayerComponentsTest
    : public ::testing::TestWithParam<PlayerComponentsTestParam> {
 protected:
  PlayerComponentsTest()
      : audio_filename_(std::get<0>(GetParam())),
        video_filename_(std::get<1>(GetParam())),
        output_mode_(std::get<2>(GetParam())) {
    SB_LOG(INFO) << "Testing: \"" << audio_filename_ << "\", \""
                 << video_filename_
                 << (output_mode_ == kSbPlayerOutputModeDecodeToTexture
                         ? "\", kSbPlayerOutputModeDecodeToTexture."
                         : "\", kSbPlayerOutputModePunchOut.");
  }

  void SetUp() override {
    if (audio_filename_.length() > 0) {
      audio_reader_.reset(new VideoDmpReader(
          ResolveTestFileName(audio_filename_.c_str()).c_str(),
          VideoDmpReader::kEnableReadOnDemand));
    }
    if (video_filename_.length() > 0) {
      video_reader_.reset(new VideoDmpReader(
          ResolveTestFileName(video_filename_.c_str()).c_str(),
          VideoDmpReader::kEnableReadOnDemand));
    }

    scoped_ptr<PlayerComponents::Factory> factory =
        PlayerComponents::Factory::Create();
    string error_message;
    if (audio_reader_ && video_reader_) {
      CreationParameters creation_parameters(
          audio_reader_->audio_codec(), audio_reader_->audio_sample_info(),
          video_reader_->video_codec(),
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
          video_reader_->GetPlayerSampleInfo(kSbMediaTypeVideo, 0)
              .video_sample_info,
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
          kDummyPlayer, output_mode_,
          fake_graphics_context_provider_.decoder_target_provider());
      player_components_ =
          factory->CreateComponents(creation_parameters, &error_message);
    } else if (audio_reader_) {
      // Audio only
      CreationParameters creation_parameters(
          audio_reader_->audio_codec(), audio_reader_->audio_sample_info());
      player_components_ =
          factory->CreateComponents(creation_parameters, &error_message);
    } else {
      // Video only
      ASSERT_TRUE(video_reader_);
      CreationParameters creation_parameters(
          video_reader_->video_codec(),
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
          video_reader_->GetPlayerSampleInfo(kSbMediaTypeVideo, 0)
              .video_sample_info,
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
          kDummyPlayer, output_mode_,
          fake_graphics_context_provider_.decoder_target_provider());
      player_components_ =
          factory->CreateComponents(creation_parameters, &error_message);
    }
    ASSERT_TRUE(player_components_);

    if (GetAudioRenderer()) {
      GetAudioRenderer()->Initialize(
          std::bind(&PlayerComponentsTest::OnError, this, _1, _2),
          std::bind(&PlayerComponentsTest::OnPrerolled, this,
                    kSbMediaTypeAudio),
          std::bind(&PlayerComponentsTest::OnEnded, this, kSbMediaTypeAudio));
    }
    SetPlaybackRate(playback_rate_);
    if (GetVideoRenderer()) {
      GetVideoRenderer()->Initialize(
          std::bind(&PlayerComponentsTest::OnError, this, _1, _2),
          std::bind(&PlayerComponentsTest::OnPrerolled, this,
                    kSbMediaTypeVideo),
          std::bind(&PlayerComponentsTest::OnEnded, this, kSbMediaTypeVideo));
    }
  }

  MediaTimeProvider* GetMediaTimeProvider() const {
    return player_components_->GetMediaTimeProvider();
  }
  AudioRenderer* GetAudioRenderer() const {
    return player_components_->GetAudioRenderer();
  }
  VideoRenderer* GetVideoRenderer() const {
    return player_components_->GetVideoRenderer();
  }

  bool IsPlaybackPrerolled() {
    if (GetAudioRenderer() && !audio_prerolled_) {
      return false;
    }
    if (GetVideoRenderer() && !video_prerolled_) {
      return false;
    }
    return true;
  }

  bool IsPlaybackEnded() {
    if (GetAudioRenderer() && !audio_ended_) {
      return false;
    }
    if (GetVideoRenderer() && !video_ended_) {
      return false;
    }
    return true;
  }

  void Seek(SbTime seek_to_time) {
    has_error_ = false;
    audio_prerolled_ = false;
    video_prerolled_ = false;
    audio_ended_ = false;
    video_ended_ = false;
    audio_index_ = 0;
    video_index_ = 0;
    // Find the closest key frame prior to |seek_to_time|.
    if (GetAudioRenderer()) {
      for (int index = 1; index < audio_reader_->number_of_audio_buffers();
           index++) {
        SbPlayerSampleInfo sample_info =
            audio_reader_->GetPlayerSampleInfo(kSbMediaTypeAudio, index);
        if (sample_info.timestamp > seek_to_time) {
          break;
        }
        audio_index_ = index;
      }
    }
    if (GetVideoRenderer()) {
      for (int index = 1; index < video_reader_->number_of_video_buffers();
           index++) {
        SbPlayerSampleInfo sample_info =
            video_reader_->GetPlayerSampleInfo(kSbMediaTypeVideo, index);
        if (sample_info.timestamp > seek_to_time) {
          break;
        }
        if (sample_info.video_sample_info.is_key_frame) {
          video_index_ = index;
        }
      }
    }
    GetMediaTimeProvider()->Pause();
    if (GetVideoRenderer()) {
      GetVideoRenderer()->Seek(seek_to_time);
    }
    GetMediaTimeProvider()->Seek(seek_to_time);
  }

  void Play() { GetMediaTimeProvider()->Play(); }

  void Pause() { GetMediaTimeProvider()->Pause(); }

  void SetPlaybackRate(double playback_rate) {
    playback_rate_ = playback_rate;
    GetMediaTimeProvider()->SetPlaybackRate(playback_rate_);
  }

  SbTime GetMediaTime() {
    bool is_playing, is_eos_played, is_underflow;
    double playback_rate;
    return GetMediaTimeProvider()->GetCurrentMediaTime(
        &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  }

  bool IsPlaying() {
    bool is_playing, is_eos_played, is_underflow;
    double playback_rate;
    GetMediaTimeProvider()->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                &is_underflow, &playback_rate);
    return is_playing;
  }

  SbTime GetCurrentAudioBufferTimestamp() const {
    if (!GetAudioRenderer()) {
      return 0;
    }
    if (audio_index_ >= audio_reader_->number_of_audio_buffers()) {
      return audio_reader_->audio_duration();
    }
    return audio_reader_->GetPlayerSampleInfo(kSbMediaTypeAudio, audio_index_)
        .timestamp;
  }

  SbTime GetCurrentVideoBufferTimestamp() const {
    if (!GetVideoRenderer()) {
      return 0;
    }
    if (video_index_ >= video_reader_->number_of_video_buffers()) {
      return video_reader_->video_duration();
    }
    if (video_index_ >= video_reader_->number_of_video_buffers() - 1) {
      return video_reader_->GetPlayerSampleInfo(kSbMediaTypeVideo, video_index_)
          .timestamp;
    }
    // The buffers are ordered by decoding order. We need to find the
    // presentation timestamp of next frame.
    SbTime next_timestamps[2] = {
        video_reader_->GetPlayerSampleInfo(kSbMediaTypeVideo, video_index_)
            .timestamp,
        video_reader_->GetPlayerSampleInfo(kSbMediaTypeVideo, video_index_ + 1)
            .timestamp};
    return std::min(next_timestamps[0], next_timestamps[1]);
  }

  void WriteDataUntilPrerolled(SbTime timeout = kDefaultPrerollTimeOut) {
    SbTimeMonotonic start_time = SbTimeGetMonotonicNow();
    SbTime max_timestamp = GetMediaTime() + kMaxWriteAheadDuration;
    while (!IsPlaybackPrerolled()) {
      ASSERT_LE(SbTimeGetMonotonicNow() - start_time, timeout)
          << "WriteDataUntilPrerolled() timed out, buffered audio ("
          << GetCurrentAudioBufferTimestamp() << "), buffered video ("
          << GetCurrentVideoBufferTimestamp() << "), max timestamp ("
          << max_timestamp << ").";
      bool written = TryToWriteOneInputBuffer(max_timestamp);
      if (!written) {
        ASSERT_NO_FATAL_FAILURE(RenderAndProcessPendingJobs());
        SbThreadSleep(5 * kSbTimeMillisecond);
      }
    }
  }

  // The function will exit after all buffers before |eos_timestamp| are written
  // into the player. Note that, to avoid audio or video underflow, the function
  // allow to write buffers of timestamp greater than |timestamp|.
  void WriteDataUntil(SbTime timestamp, SbTime timeout = kDefaultWriteTimeOut) {
    SB_CHECK(playback_rate_ != 0);

    SbTimeMonotonic last_input_filled_time = SbTimeGetMonotonicNow();
    while (
        (GetAudioRenderer() && GetCurrentAudioBufferTimestamp() < timestamp) ||
        (GetVideoRenderer() && GetCurrentVideoBufferTimestamp() < timestamp)) {
      if (last_input_filled_time != -1) {
        ASSERT_LE(SbTimeGetMonotonicNow() - last_input_filled_time, timeout)
            << "WriteDataUntil() timed out, buffered audio ("
            << GetCurrentAudioBufferTimestamp() << "), buffered video ("
            << GetCurrentVideoBufferTimestamp() << "), timestamp (" << timestamp
            << ").";
      }
      bool written =
          TryToWriteOneInputBuffer(timestamp + kMaxWriteAheadDuration);
      if (written) {
        last_input_filled_time = SbTimeGetMonotonicNow();
      } else {
        ASSERT_NO_FATAL_FAILURE(RenderAndProcessPendingJobs());
        SbThreadSleep(5 * kSbTimeMillisecond);
      }
    }
  }

  // The function will write EOS immediately after all buffers before
  // |eos_timestamp| are written into the player.
  void WriteDataAndEOS(SbTime eos_timestamp,
                       SbTime timeout = kDefaultWriteTimeOut) {
    SB_CHECK(playback_rate_ != 0);
    bool audio_eos_written = !GetAudioRenderer();
    bool video_eos_written = !GetVideoRenderer();

    SbTimeMonotonic last_input_filled_time = SbTimeGetMonotonicNow();
    while (!audio_eos_written || !video_eos_written) {
      if (!audio_eos_written &&
          GetCurrentAudioBufferTimestamp() >= eos_timestamp) {
        GetAudioRenderer()->WriteEndOfStream();
        audio_eos_written = true;
      }
      if (!video_eos_written &&
          GetCurrentVideoBufferTimestamp() >= eos_timestamp) {
        GetVideoRenderer()->WriteEndOfStream();
        video_eos_written = true;
      }
      if (last_input_filled_time != -1) {
        ASSERT_LE(SbTimeGetMonotonicNow() - last_input_filled_time, timeout)
            << "WriteDataAndEOS() timed out, buffered audio ("
            << GetCurrentAudioBufferTimestamp() << "), buffered video ("
            << GetCurrentVideoBufferTimestamp() << "), eos_timestamp ("
            << eos_timestamp << ").";
      }
      bool written = TryToWriteOneInputBuffer(eos_timestamp);
      if (written) {
        last_input_filled_time = SbTimeGetMonotonicNow();
      } else {
        ASSERT_NO_FATAL_FAILURE(RenderAndProcessPendingJobs());
        SbThreadSleep(5 * kSbTimeMillisecond);
      }
    }
  }

  void WriteEOSDirectly() {
    if (GetAudioRenderer()) {
      GetAudioRenderer()->WriteEndOfStream();
    }
    if (GetVideoRenderer()) {
      GetVideoRenderer()->WriteEndOfStream();
    }
  }

  void WaitUntilPlaybackEnded() {
    SB_CHECK(playback_rate_ != 0);

    SbTime duration = std::max(GetCurrentAudioBufferTimestamp(),
                               GetCurrentVideoBufferTimestamp());
    SbTime current_time = GetMediaTime();
    SbTime expected_end_time =
        SbTimeGetMonotonicNow() +
        static_cast<SbTime>((duration - current_time) / playback_rate_) +
        kDefaultEndTimeOut;

    while (!IsPlaybackEnded()) {
      // If this fails, timeout must have been reached.
      ASSERT_LE(SbTimeGetMonotonicNow(), expected_end_time)
          << "WaitUntilPlaybackEnded() timed out, buffered audio ("
          << GetCurrentAudioBufferTimestamp() << "), buffered video ("
          << GetCurrentVideoBufferTimestamp() << "), current media time is "
          << GetMediaTime() << ".";
      ASSERT_NO_FATAL_FAILURE(RenderAndProcessPendingJobs());
      SbThreadSleep(5 * kSbTimeMillisecond);
    }
    current_time = GetMediaTime();
    // TODO: investigate and reduce the tolerance.
    ASSERT_LE(std::abs(current_time - duration), 500 * kSbTimeMillisecond);
  }

  // This function needs to be called periodically to keep player components
  // working.
  void RenderAndProcessPendingJobs() {
    // Call GetCurrentDecodeTarget() periodically for decode to texture mode.
    if (output_mode_ == kSbPlayerOutputModeDecodeToTexture &&
        GetVideoRenderer()) {
#if SB_HAS(GLES2)
      fake_graphics_context_provider_.RunOnGlesContextThread(
          std::bind(&PlayerComponentsTest::RenderOnGlesContextThread, this));
#endif  // SB_HAS(GLES2)
    }
    // Process jobs in the job queue.
    job_queue_.RunUntilIdle();
    ASSERT_FALSE(has_error_);
  }

 private:
  // We won't write audio data more than 1s ahead of current media time in
  // cobalt. So, to test with the same condition, we limit max inputs ahead to
  // 1s in the tests.
  const SbTime kMaxWriteAheadDuration = kSbTimeSecond;

  void OnError(SbPlayerError error, const std::string& error_message) {
    has_error_ = true;
    SB_LOG(ERROR) << "Caught renderer error (" << error
                  << "): " << error_message;
  }
  void OnPrerolled(SbMediaType media_type) {
    if (media_type == kSbMediaTypeAudio) {
      audio_prerolled_ = true;
    } else {
      EXPECT_EQ(media_type, kSbMediaTypeVideo);
      video_prerolled_ = true;
    }
  }
  void OnEnded(SbMediaType media_type) {
    if (media_type == kSbMediaTypeAudio) {
      SB_DLOG(INFO) << "Audio OnEnded is called.";
      audio_ended_ = true;
    } else {
      EXPECT_EQ(media_type, kSbMediaTypeVideo);
      SB_DLOG(INFO) << "Video OnEnded is called.";
      video_ended_ = true;
    }
  }

  scoped_refptr<InputBuffer> GetAudioInputBuffer(size_t index) const {
    auto player_sample_info =
        audio_reader_->GetPlayerSampleInfo(kSbMediaTypeAudio, index);
#if SB_API_VERSION >= 11
    auto input_buffer = new InputBuffer(StubDeallocateSampleFunc, nullptr,
                                        nullptr, player_sample_info);
#else   // SB_API_VERSION >= 11
    SbMediaAudioSampleInfo audio_sample_info =
        audio_reader_->GetAudioSampleInfo(index);
    auto input_buffer =
        new InputBuffer(kSbMediaTypeAudio, StubDeallocateSampleFunc, nullptr,
                        nullptr, player_sample_info, &audio_sample_info);
#endif  // SB_API_VERSION >= 11
    return input_buffer;
  }

  scoped_refptr<InputBuffer> GetVideoInputBuffer(size_t index) const {
    auto video_sample_info =
        video_reader_->GetPlayerSampleInfo(kSbMediaTypeVideo, index);
#if SB_API_VERSION >= 11
    auto input_buffer = new InputBuffer(StubDeallocateSampleFunc, NULL, NULL,
                                        video_sample_info);
#else   // SB_API_VERSION >= 11
    auto input_buffer =
        new InputBuffer(kSbMediaTypeVideo, StubDeallocateSampleFunc, NULL, NULL,
                        video_sample_info, NULL);
#endif  // SB_API_VERSION >= 11
    return input_buffer;
  }

  void RenderOnGlesContextThread() {
    SbDecodeTargetRelease(GetVideoRenderer()->GetCurrentDecodeTarget());
  }

  bool TryToWriteOneInputBuffer(SbTime max_timestamp) {
    bool input_buffer_written = false;
    if (GetAudioRenderer() && GetAudioRenderer()->CanAcceptMoreData() &&
        audio_index_ < audio_reader_->number_of_audio_buffers() &&
        GetCurrentAudioBufferTimestamp() < max_timestamp) {
      GetAudioRenderer()->WriteSample(GetAudioInputBuffer(audio_index_++));
      input_buffer_written = true;
    }
    if (GetVideoRenderer() && GetVideoRenderer()->CanAcceptMoreData() &&
        video_index_ < video_reader_->number_of_video_buffers() &&
        GetCurrentVideoBufferTimestamp() < max_timestamp) {
      GetVideoRenderer()->WriteSample(GetVideoInputBuffer(video_index_++));
      input_buffer_written = true;
    }
    if (input_buffer_written) {
      job_queue_.RunUntilIdle();
    }
    return input_buffer_written;
  }

  const std::string audio_filename_;
  const std::string video_filename_;
  const SbPlayerOutputMode output_mode_;
  JobQueue job_queue_;
  FakeGraphicsContextProvider fake_graphics_context_provider_;
  unique_ptr<VideoDmpReader> audio_reader_;
  unique_ptr<VideoDmpReader> video_reader_;
  scoped_ptr<PlayerComponents> player_components_;
  double playback_rate_ = 1.0;
  int audio_index_ = 0;
  int video_index_ = 0;
  bool has_error_ = false;
  bool audio_prerolled_ = false;
  bool video_prerolled_ = false;
  bool audio_ended_ = false;
  bool video_ended_ = false;
};

TEST_P(PlayerComponentsTest, Preroll) {
  Seek(0);
  ASSERT_NO_FATAL_FAILURE(WriteDataUntilPrerolled());
}

TEST_P(PlayerComponentsTest, SunnyDay) {
  Seek(0);
  ASSERT_NO_FATAL_FAILURE(WriteDataUntilPrerolled());
  ASSERT_EQ(GetMediaTime(), 0);
  ASSERT_FALSE(IsPlaying());

  SbTimeMonotonic play_requested_at = SbTimeGetMonotonicNow();
  Play();
  SbTime media_duration = std::max(GetCurrentVideoBufferTimestamp(),
                                   GetCurrentAudioBufferTimestamp());
  media_duration = std::max(kSbTimeSecond, media_duration);

  ASSERT_NO_FATAL_FAILURE(WriteDataAndEOS(media_duration));
  ASSERT_NO_FATAL_FAILURE(WaitUntilPlaybackEnded());

  // TODO: investigate and reduce the tolerance.
  // ASSERT_LE(
  //     std::abs(SbTimeGetMonotonicNow() - (play_requested_at +
  //     media_duration)), 300 * kSbTimeMillisecond);
}

TEST_P(PlayerComponentsTest, ShortPlayback) {
  Seek(0);
  ASSERT_NO_FATAL_FAILURE(WriteDataAndEOS(50 * kSbTimeMillisecond));
  Play();
  ASSERT_NO_FATAL_FAILURE(WaitUntilPlaybackEnded());
}

TEST_P(PlayerComponentsTest, EOSWithoutInput) {
  Seek(0);
  ASSERT_NO_FATAL_FAILURE(WriteEOSDirectly());
  Play();
  ASSERT_NO_FATAL_FAILURE(WaitUntilPlaybackEnded());
  // TODO: investigate and reduce the tolerance.
  // ASSERT_LE(std::abs(GetMediaTime()), 100 * kSbTimeMillisecond);
}

TEST_P(PlayerComponentsTest, Pause) {
  Seek(0);
  ASSERT_NO_FATAL_FAILURE(WriteDataUntilPrerolled());
  Play();
  ASSERT_NO_FATAL_FAILURE(WriteDataUntil(kSbTimeSecond));
  Pause();
  ASSERT_FALSE(IsPlaying());

  SbTimeMonotonic start_time = SbTimeGetMonotonicNow();
  while (SbTimeGetMonotonicNow() < start_time + 200 * kSbTimeMillisecond) {
    ASSERT_NO_FATAL_FAILURE(RenderAndProcessPendingJobs());
    SbThreadSleep(5 * kSbTimeMillisecond);
  }
  SbTime media_time = GetMediaTime();
  start_time = SbTimeGetMonotonicNow();
  while (SbTimeGetMonotonicNow() < start_time + 200 * kSbTimeMillisecond) {
    ASSERT_NO_FATAL_FAILURE(RenderAndProcessPendingJobs());
    SbThreadSleep(5 * kSbTimeMillisecond);
  }
  ASSERT_EQ(media_time, GetMediaTime());

  Play();
  SbTime duration = std::max(GetCurrentAudioBufferTimestamp(),
                             GetCurrentVideoBufferTimestamp());
  ASSERT_NO_FATAL_FAILURE(WriteDataAndEOS(duration));
  ASSERT_NO_FATAL_FAILURE(WaitUntilPlaybackEnded());
}

// TODO: Enable variable playback rate tests
// TEST_P(PlayerComponentsTest, PlaybackRateHalf) {
//   const double kPlaybackRate = 0.5;

//   Seek(0);
//   SetPlaybackRate(kPlaybackRate);
//   ASSERT_NO_FATAL_FAILURE(WriteDataUntilPrerolled());
//   ASSERT_EQ(GetMediaTime(), 0);
//   ASSERT_FALSE(IsPlaying());

//   SbTime media_duration_to_write = std::max(GetCurrentVideoBufferTimestamp(),
//                                             GetCurrentAudioBufferTimestamp());
//   media_duration_to_write = std::max(kSbTimeSecond, media_duration_to_write);
//   SbTime media_duration_to_play =
//       static_cast<SbTime>(media_duration_to_write / kPlaybackRate);

//   SbTimeMonotonic play_requested_at = SbTimeGetMonotonicNow();
//   Play();

//   ASSERT_NO_FATAL_FAILURE(WriteDataAndEOS(media_duration_to_write));
//   ASSERT_NO_FATAL_FAILURE(WaitUntilPlaybackEnded());

//   TODO: Enable the below check after we improve the accuracy of varied
//   playback rate time reporting.
//   ASSERT_GE(SbTimeGetMonotonicNow(),
//             play_requested_at + media_duration_to_play - 200 *
//             kSbTimeMillisecond);
// }

// TEST_P(PlayerComponentsTest, PlaybackRateDouble) {
//   const double kPlaybackRate = 2.0;

//   Seek(0);
//   SetPlaybackRate(kPlaybackRate);
//   ASSERT_NO_FATAL_FAILURE(WriteDataUntilPrerolled());
//   ASSERT_EQ(GetMediaTime(), 0);
//   ASSERT_FALSE(IsPlaying());

//   SbTime media_duration_to_write = std::max(GetCurrentVideoBufferTimestamp(),
//                                             GetCurrentAudioBufferTimestamp());
//   media_duration_to_write =
//       std::max(kSbTimeSecond * 2, media_duration_to_write);
//   SbTime media_duration_to_play =
//       static_cast<SbTime>(media_duration_to_write / kPlaybackRate);

//   SbTimeMonotonic play_requested_at = SbTimeGetMonotonicNow();
//   Play();

//   ASSERT_NO_FATAL_FAILURE(WriteDataAndEOS(media_duration_to_write));
//   ASSERT_NO_FATAL_FAILURE(WaitUntilPlaybackEnded());

//   playback rate time reporting.
//   ASSERT_LE(SbTimeGetMonotonicNow(),
//             play_requested_at + media_duration_to_play + 200 *
//             kSbTimeMillisecond);
// }

TEST_P(PlayerComponentsTest, SeekForward) {
  SbTime seek_to_time = 0;
  Seek(seek_to_time);
  ASSERT_NO_FATAL_FAILURE(WriteDataUntilPrerolled());
  ASSERT_EQ(GetMediaTime(), seek_to_time);
  ASSERT_FALSE(IsPlaying());

  Play();
  ASSERT_NO_FATAL_FAILURE(WriteDataUntil(kSbTimeSecond));

  Pause();
  seek_to_time = 2 * kSbTimeSecond;
  Seek(seek_to_time);
  ASSERT_NO_FATAL_FAILURE(WriteDataUntilPrerolled());
  ASSERT_EQ(GetMediaTime(), seek_to_time);
  ASSERT_FALSE(IsPlaying());

  Play();
  ASSERT_NO_FATAL_FAILURE(WriteDataAndEOS(seek_to_time + kSbTimeSecond));
  ASSERT_NO_FATAL_FAILURE(WaitUntilPlaybackEnded());
}

TEST_P(PlayerComponentsTest, SeekBackward) {
  SbTime seek_to_time = 3 * kSbTimeSecond;
  Seek(seek_to_time);
  ASSERT_NO_FATAL_FAILURE(WriteDataUntilPrerolled());
  ASSERT_EQ(GetMediaTime(), seek_to_time);
  ASSERT_FALSE(IsPlaying());

  Play();
  ASSERT_NO_FATAL_FAILURE(WriteDataAndEOS(seek_to_time + kSbTimeSecond));

  Pause();
  seek_to_time = 1 * kSbTimeSecond;
  Seek(seek_to_time);
  ASSERT_NO_FATAL_FAILURE(WriteDataUntilPrerolled());
  ASSERT_EQ(GetMediaTime(), seek_to_time);
  ASSERT_FALSE(IsPlaying());

  Play();
  ASSERT_NO_FATAL_FAILURE(WriteDataAndEOS(seek_to_time + kSbTimeSecond));
  ASSERT_NO_FATAL_FAILURE(WaitUntilPlaybackEnded());
}

PlayerComponentsTestParam CreateParam(const char* audio_file,
                                      const VideoTestParam& video_param) {
  return std::make_tuple(audio_file, std::get<0>(video_param),
                         std::get<1>(video_param));
}

vector<PlayerComponentsTestParam> GetSupportedCreationParameters() {
  vector<PlayerComponentsTestParam> supported_parameters;

  // TODO: Enable tests of "heaac.dmp".
  vector<const char*> audio_files =
      GetSupportedAudioTestFiles(kExcludeHeaac, SbAudioSinkGetMaxChannels());
  vector<VideoTestParam> video_params = GetSupportedVideoTests();

  // Filter too short dmp files, as the tests need at least 4s of data.
  for (auto iter = audio_files.begin(); iter != audio_files.end();) {
    VideoDmpReader audio_dmp_reader(ResolveTestFileName(*iter).c_str(),
                                    VideoDmpReader::kEnableReadOnDemand);
    if (audio_dmp_reader.audio_duration() < 5 * kSbTimeSecond) {
      iter = audio_files.erase(iter);
    } else {
      iter++;
    }
  }
  for (auto iter = video_params.begin(); iter != video_params.end();) {
    VideoDmpReader video_dmp_reader(
        ResolveTestFileName(std::get<0>(*iter)).c_str(),
        VideoDmpReader::kEnableReadOnDemand);
    if (video_dmp_reader.video_duration() < 5 * kSbTimeSecond) {
      iter = video_params.erase(iter);
    } else {
      iter++;
    }
  }

  for (size_t i = 0; i < video_params.size(); i++) {
    supported_parameters.push_back(CreateParam("", video_params[i]));
    for (size_t j = 0; j < audio_files.size(); j++) {
      supported_parameters.push_back(
          CreateParam(audio_files[j], video_params[i]));
    }
  }
  SB_DCHECK(supported_parameters.size() < 50)
      << "There're " << supported_parameters.size()
      << " tests added. It may take too long time to run and result in timeout";

  for (size_t i = 0; i < audio_files.size(); i++) {
    if (VideoDecoder::OutputModeSupported(kSbPlayerOutputModeDecodeToTexture,
                                          kSbMediaVideoCodecNone,
                                          kSbDrmSystemInvalid)) {
      supported_parameters.push_back(std::make_tuple(
          audio_files[i], "", kSbPlayerOutputModeDecodeToTexture));
    }
    if (VideoDecoder::OutputModeSupported(kSbPlayerOutputModePunchOut,
                                          kSbMediaVideoCodecNone,
                                          kSbDrmSystemInvalid)) {
      supported_parameters.push_back(
          std::make_tuple(audio_files[i], "", kSbPlayerOutputModePunchOut));
    }
  }

  return supported_parameters;
}

INSTANTIATE_TEST_CASE_P(PlayerComponentsTests,
                        PlayerComponentsTest,
                        ValuesIn(GetSupportedCreationParameters()));

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
