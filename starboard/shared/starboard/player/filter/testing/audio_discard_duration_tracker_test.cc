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

#include "starboard/shared/starboard/player/filter/audio_discard_duration_tracker.h"

#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

using starboard::player::video_dmp::VideoDmpReader;

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AudioDiscardDurationTrackerTest);

class AudioDiscardDurationTrackerTest
    : public ::testing::TestWithParam<const char*> {
 public:
  AudioDiscardDurationTrackerTest()
      : dmp_reader_(GetParam(), VideoDmpReader::kEnableReadOnDemand) {}

 protected:
  VideoDmpReader dmp_reader_;
  // TODO: Determine the duration from the InputBuffer itself.
  const SbTime kPassthroughBufferDuration = 32 * kSbTimeMillisecond;
};

TEST_P(AudioDiscardDurationTrackerTest, SingleInputNonPartialAudio) {
  scoped_refptr<InputBuffer> input_buffer =
      GetAudioInputBuffer(&dmp_reader_, 0);

  AudioDiscardDurationTracker tracker;
  tracker.CacheDiscardDuration(input_buffer, kPassthroughBufferDuration);

  SbTime timestamp = kPassthroughBufferDuration;
  SbTime adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(timestamp);

  EXPECT_EQ(adjusted_timestamp, timestamp);
}

TEST_P(AudioDiscardDurationTrackerTest, MultipleInputsNonPartialAudio) {
  InputBuffers input_buffers;

  int buffers_written = 0;
  for (int i = 0; i < std::min<int>(32, dmp_reader_.number_of_audio_buffers());
       ++i) {
    input_buffers.push_back(GetAudioInputBuffer(&dmp_reader_, i));
    ++buffers_written;
  }

  AudioDiscardDurationTracker tracker;
  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDuration);

  for (int i = 1; i <= buffers_written; ++i) {
    SbTime timestamp = kPassthroughBufferDuration * i;
    SbTime adjusted_timestamp =
        tracker.AdjustTimeForTotalDiscardDuration(timestamp);
    EXPECT_EQ(adjusted_timestamp, timestamp);
  }
}

TEST_P(AudioDiscardDurationTrackerTest, SingleInputPartialAudio) {
  const SbTime kDiscardDuration = 10 * kSbTimeMillisecond;

  // Discard front only.
  scoped_refptr<InputBuffer> input_buffer =
      GetAudioInputBuffer(&dmp_reader_, 0, kDiscardDuration, 0);

  AudioDiscardDurationTracker tracker;
  tracker.CacheDiscardDuration(input_buffer, kPassthroughBufferDuration);

  // The timestamp at which audio begins to be discarded. For a 32 ms buffer at
  // timestamp 50 ms with a front discard duration of 10 ms, the discard start
  // timestamp is 50 ms, ending at 50 ms + 10 ms = 60 ms. For the same buffer
  // duration 32 ms at timestamp 50 ms with a back discard duration of 10 ms,
  // the discard start timestamp is 50 ms + 32 ms - 10 ms = 72 ms, ending at 50
  // ms + 32 ms = 82 ms.
  SbTime discard_start_timestamp = input_buffer->timestamp();
  SbTime discard_end_timestamp = discard_start_timestamp + kDiscardDuration;

  SbTime adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(discard_start_timestamp);
  EXPECT_EQ(adjusted_timestamp, discard_start_timestamp);

  adjusted_timestamp = tracker.AdjustTimeForTotalDiscardDuration(
      discard_start_timestamp + kDiscardDuration / 2);
  // Expect the timestamp to be adjusted midway through the discard duration.
  EXPECT_EQ(adjusted_timestamp, discard_start_timestamp);

  adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(discard_end_timestamp);
  EXPECT_EQ(adjusted_timestamp, discard_start_timestamp);

  // A timestamp beyond the end of the discard duration.
  SbTime incremented_timestamp =
      5 + kDiscardDuration + input_buffer->timestamp();
  adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(incremented_timestamp);
  // After the timestamp progresses beyond the end of the discard duration,
  // expect the adjusted timestamp to subtract the total discard duration.
  EXPECT_EQ(adjusted_timestamp, incremented_timestamp - kDiscardDuration);

  tracker.Reset();

  // Discard back only.
  input_buffer = GetAudioInputBuffer(&dmp_reader_, 0, 0, kDiscardDuration);
  tracker.CacheDiscardDuration(input_buffer, kPassthroughBufferDuration);

  discard_start_timestamp =
      input_buffer->timestamp() + kPassthroughBufferDuration - kDiscardDuration;
  discard_end_timestamp = discard_start_timestamp + kDiscardDuration;

  adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(discard_start_timestamp);
  EXPECT_EQ(adjusted_timestamp, discard_start_timestamp);

  adjusted_timestamp = tracker.AdjustTimeForTotalDiscardDuration(
      discard_start_timestamp + kDiscardDuration / 2);
  EXPECT_EQ(adjusted_timestamp, discard_start_timestamp);

  adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(discard_end_timestamp);
  EXPECT_EQ(adjusted_timestamp, discard_start_timestamp);

  incremented_timestamp =
      5 + kPassthroughBufferDuration + input_buffer->timestamp();
  adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(incremented_timestamp);
  EXPECT_EQ(adjusted_timestamp, incremented_timestamp - kDiscardDuration);

  tracker.Reset();

  // Discard both front and back.
  input_buffer =
      GetAudioInputBuffer(&dmp_reader_, 0, kDiscardDuration, kDiscardDuration);
  SbTime front_discard_duration = kDiscardDuration;
  SbTime back_discard_duration = kDiscardDuration;
  tracker.CacheDiscardDuration(input_buffer, kPassthroughBufferDuration);

  SbTime discard_front_start_timestamp = input_buffer->timestamp();
  SbTime discard_front_end_timestamp =
      discard_front_start_timestamp + front_discard_duration;

  // Check the start of the input buffer.
  adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(input_buffer->timestamp());
  EXPECT_EQ(adjusted_timestamp, discard_front_start_timestamp);

  // Check the middle of the input buffer, in between discard durations.
  incremented_timestamp = discard_front_end_timestamp + 5 * kSbTimeMillisecond;
  adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(incremented_timestamp);
  EXPECT_EQ(adjusted_timestamp, incremented_timestamp - front_discard_duration);

  // Check the end of the input buffer.
  adjusted_timestamp = tracker.AdjustTimeForTotalDiscardDuration(
      input_buffer->timestamp() + kPassthroughBufferDuration);
  EXPECT_EQ(adjusted_timestamp,
            input_buffer->timestamp() + kPassthroughBufferDuration -
                front_discard_duration - back_discard_duration);
}

TEST_P(AudioDiscardDurationTrackerTest, MultipleInputsPartialAudio) {
  InputBuffers input_buffers;

  int buffers_written = 0;
  const SbTime kDiscardDuration = kPassthroughBufferDuration / 2;
  for (int i = 0; i < std::min<int>(32, dmp_reader_.number_of_audio_buffers());
       ++i) {
    SbTime front_discard_duration = i % 2 == 0 ? kDiscardDuration : 0;
    SbTime back_discard_duration = i % 2 == 0 ? 0 : kDiscardDuration;
    input_buffers.push_back(GetAudioInputBuffer(
        &dmp_reader_, i, front_discard_duration, back_discard_duration));
    ++buffers_written;
  }

  AudioDiscardDurationTracker tracker;
  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDuration);

  for (int i = 1; i <= buffers_written; ++i) {
    SbTime timestamp = kPassthroughBufferDuration * i;
    SbTime adjusted_timestamp =
        tracker.AdjustTimeForTotalDiscardDuration(timestamp);
    SbTime expected_timestamp = timestamp - i * kDiscardDuration;
    EXPECT_EQ(adjusted_timestamp, expected_timestamp);
  }
}

TEST_P(AudioDiscardDurationTrackerTest, DiscardAll) {
  scoped_refptr<InputBuffer> input_buffer = GetAudioInputBuffer(
      &dmp_reader_, 0, kPassthroughBufferDuration, kPassthroughBufferDuration);

  AudioDiscardDurationTracker tracker;
  tracker.CacheDiscardDuration(input_buffer, kPassthroughBufferDuration);

  SbTime adjusted_timestamp =
      tracker.AdjustTimeForTotalDiscardDuration(input_buffer->timestamp());
  EXPECT_EQ(input_buffer->timestamp(), adjusted_timestamp);

  // As the entire buffer is discarded, expect the adjusted timestamp to match
  // the timestamp of the input buffer.
  adjusted_timestamp = tracker.AdjustTimeForTotalDiscardDuration(
      input_buffer->timestamp() + kPassthroughBufferDuration);
  EXPECT_EQ(adjusted_timestamp, input_buffer->timestamp());

  tracker.Reset();

  InputBuffers input_buffers;

  int buffers_written = 0;
  for (int i = 0; i < std::min<int>(32, dmp_reader_.number_of_audio_buffers());
       ++i) {
    input_buffers.push_back(GetAudioInputBuffer(&dmp_reader_, i,
                                                kPassthroughBufferDuration,
                                                kPassthroughBufferDuration));
    ++buffers_written;
  }

  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDuration);

  SbTime initial_timestamp = GetAudioInputBuffer(&dmp_reader_, 0)->timestamp();
  for (int i = 1; i <= buffers_written; ++i) {
    SbTime timestamp = kPassthroughBufferDuration * i;
    SbTime adjusted_timestamp =
        tracker.AdjustTimeForTotalDiscardDuration(timestamp);
    EXPECT_EQ(adjusted_timestamp, initial_timestamp);
  }
}

INSTANTIATE_TEST_CASE_P(AudioDiscardDurationTrackerTests,
                        AudioDiscardDurationTrackerTest,
                        ::testing::ValuesIn(GetSupportedAudioTestFiles(
                            kExcludeHeaac,
                            6,
                            "" /* extra_mime_attributes*/,
                            true /* passthrough_only*/)),
                        [](::testing::TestParamInfo<const char*> info) {
                          std::string filename(info.param);
                          std::replace(filename.begin(), filename.end(), '.',
                                       '_');
                          return filename;
                        });

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
