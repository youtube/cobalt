// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
  const int64_t kPassthroughBufferDurationUs = 32'000;  // 32 ms
};

TEST_P(AudioDiscardDurationTrackerTest, SingleInputNonPartialAudio) {
  scoped_refptr<InputBuffer> input_buffer =
      GetAudioInputBuffer(&dmp_reader_, 0);

  InputBuffers input_buffers = {input_buffer};
  AudioDiscardDurationTracker tracker;
  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDurationUs);

  int64_t timestamp_us = kPassthroughBufferDurationUs;
  int64_t adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(timestamp_us);

  EXPECT_EQ(adjusted_timestamp_us, timestamp_us);
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
                                        kPassthroughBufferDurationUs);

  for (int i = 1; i <= buffers_written; ++i) {
    int64_t timestamp_us = kPassthroughBufferDurationUs * i;
    int64_t adjusted_timestamp_us =
        tracker.AdjustTimeForTotalDiscardDuration(timestamp_us);
    EXPECT_EQ(adjusted_timestamp_us, timestamp_us);
  }
}

TEST_P(AudioDiscardDurationTrackerTest, SingleInputPartialAudio) {
  const int64_t kDiscardDurationUs = 10'000;  // 10 ms.

  // Discard front only.
  scoped_refptr<InputBuffer> input_buffer =
      GetAudioInputBuffer(&dmp_reader_, 0, kDiscardDurationUs, 0);
  InputBuffers input_buffers = {input_buffer};

  AudioDiscardDurationTracker tracker;
  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDurationUs);

  // The timestamp at which audio begins to be discarded. For a 32 ms buffer at
  // timestamp 50 ms with a front discard duration of 10 ms, the discard start
  // timestamp is 50 ms, ending at 50 ms + 10 ms = 60 ms. For the same buffer
  // duration 32 ms at timestamp 50 ms with a back discard duration of 10 ms,
  // the discard start timestamp is 50 ms + 32 ms - 10 ms = 72 ms, ending at 50
  // ms + 32 ms = 82 ms.
  int64_t discard_start_timestamp_us = input_buffer->timestamp();
  int64_t discard_end_timestamp_us =
      discard_start_timestamp_us + kDiscardDurationUs;

  int64_t adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(discard_start_timestamp_us);
  EXPECT_EQ(adjusted_timestamp_us, discard_start_timestamp_us);

  adjusted_timestamp_us = tracker.AdjustTimeForTotalDiscardDuration(
      discard_start_timestamp_us + kDiscardDurationUs / 2);
  // Expect the timestamp to be adjusted midway through the discard duration.
  EXPECT_EQ(adjusted_timestamp_us, discard_start_timestamp);

  adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(discard_end_timestamp_us);
  EXPECT_EQ(adjusted_timestamp_us, discard_start_timestamp);

  // A timestamp beyond the end of the discard duration.
  int64_t incremented_timestamp_us =
      5 + kDiscardDurationUs + input_buffer->timestamp();
  adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(incremented_timestamp_us);
  // After the timestamp progresses beyond the end of the discard duration,
  // expect the adjusted timestamp to subtract the total discard duration.
  EXPECT_EQ(adjusted_timestamp_us,
            incremented_timestamp_us - kDiscardDurationUs);

  tracker.Reset();
  input_buffers.clear();

  // Discard back only.
  input_buffer = GetAudioInputBuffer(&dmp_reader_, 0, 0, kDiscardDurationUs);
  input_buffers = {input_buffer};
  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDurationUs);

  discard_start_timestamp_us = input_buffer->timestamp() +
                               kPassthroughBufferDurationUs -
                               kDiscardDurationUs;
  discard_end_timestamp_us = discard_start_timestamp_us + kDiscardDurationUs;

  adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(discard_start_timestamp_us);
  EXPECT_EQ(adjusted_timestamp_us, discard_start_timestamp_us);

  adjusted_timestamp_us = tracker.AdjustTimeForTotalDiscardDuration(
      discard_start_timestamp_us + kDiscardDurationUs / 2);
  EXPECT_EQ(adjusted_timestamp_us, discard_start_timestamp_us);

  adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(discard_end_timestamp_us);
  EXPECT_EQ(adjusted_timestamp_us, discard_start_timestamp_us);

  incremented_timestamp_us =
      5 + kPassthroughBufferDurationUs + input_buffer->timestamp();
  adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(incremented_timestamp_us);
  EXPECT_EQ(adjusted_timestamp_us,
            incremented_timestamp_us - kDiscardDurationUs);

  tracker.Reset();
  input_buffers.clear();

  // Discard both front and back.
  input_buffer = GetAudioInputBuffer(&dmp_reader_, 0, kDiscardDurationUs,
                                     kDiscardDurationUs);
  int64_t front_discard_duration_us = kDiscardDurationUs;
  int64_t back_discard_duration = kDiscardDurationUs;
  input_buffers = {input_buffer};
  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDurationUs);

  int64_t discard_front_start_timestamp_us = input_buffer->timestamp();
  int64_t discard_front_end_timestamp_us =
      discard_front_start_timestamp_us + front_discard_duration_us;

  // Check the start of the input buffer.
  adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(input_buffer->timestamp());
  EXPECT_EQ(adjusted_timestamp_us, discard_front_start_timestamp_us);

  // Check the middle of the input buffer, in between discard durations.
  incremented_timestamp_us =
      discard_front_end_timestamp_us + 5 * kSbTimeMillisecond;
  adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(incremented_timestamp_us);
  EXPECT_EQ(adjusted_timestamp_us,
            incremented_timestamp_us - front_discard_duration_us);

  // Check the end of the input buffer.
  adjusted_timestamp_us = tracker.AdjustTimeForTotalDiscardDuration(
      input_buffer->timestamp() + kPassthroughBufferDurationUs);
  EXPECT_EQ(adjusted_timestamp_us,
            input_buffer->timestamp() + kPassthroughBufferDurationUs -
                front_discard_duration_us - back_discard_duration);
}

TEST_P(AudioDiscardDurationTrackerTest, MultipleInputsPartialAudio) {
  InputBuffers input_buffers;

  int buffers_written = 0;
  const int64_t kDiscardDurationUs = kPassthroughBufferDurationUs / 2;
  for (int i = 0; i < std::min<int>(32, dmp_reader_.number_of_audio_buffers());
       ++i) {
    int64_t front_discard_duration_us = i % 2 == 0 ? kDiscardDurationUs : 0;
    int64_t back_discard_duration = i % 2 == 0 ? 0 : kDiscardDurationUs;
    input_buffers.push_back(GetAudioInputBuffer(
        &dmp_reader_, i, front_discard_duration_us, back_discard_duration));
    ++buffers_written;
  }

  AudioDiscardDurationTracker tracker;
  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDurationUs);

  for (int i = 1; i <= buffers_written; ++i) {
    int64_t timestamp_us = kPassthroughBufferDurationUs * i;
    int64_t adjusted_timestamp_us =
        tracker.AdjustTimeForTotalDiscardDuration(timestamp_us);
    int64_t expected_timestamp_us = timestamp_us - i * kDiscardDurationUs;
    EXPECT_EQ(adjusted_timestamp_us, expected_timestamp_us);
  }
}

TEST_P(AudioDiscardDurationTrackerTest, DiscardAll) {
  scoped_refptr<InputBuffer> input_buffer =
      GetAudioInputBuffer(&dmp_reader_, 0, kPassthroughBufferDurationUs,
                          kPassthroughBufferDurationUs);

  AudioDiscardDurationTracker tracker;
  InputBuffers input_buffers = {input_buffer};
  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDurationUs);

  int64_t adjusted_timestamp_us =
      tracker.AdjustTimeForTotalDiscardDuration(input_buffer->timestamp());
  EXPECT_EQ(input_buffer->timestamp(), adjusted_timestamp_us);

  // As the entire buffer is discarded, expect the adjusted timestamp to match
  // the timestamp of the input buffer.
  adjusted_timestamp_us = tracker.AdjustTimeForTotalDiscardDuration(
      input_buffer->timestamp() + kPassthroughBufferDurationUs);
  EXPECT_EQ(adjusted_timestamp_us, input_buffer->timestamp());

  tracker.Reset();
  input_buffers.clear();

  int buffers_written = 0;
  for (int i = 0; i < std::min<int>(32, dmp_reader_.number_of_audio_buffers());
       ++i) {
    input_buffers.push_back(GetAudioInputBuffer(&dmp_reader_, i,
                                                kPassthroughBufferDurationUs,
                                                kPassthroughBufferDurationUs));
    ++buffers_written;
  }

  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDurationUs);

  int64_t initial_timestamp = GetAudioInputBuffer(&dmp_reader_, 0)->timestamp();
  for (int i = 1; i <= buffers_written; ++i) {
    int64_t timestamp_us = kPassthroughBufferDurationUs * i;
    int64_t adjusted_timestamp_us =
        tracker.AdjustTimeForTotalDiscardDuration(timestamp_us);
    EXPECT_EQ(adjusted_timestamp_us, initial_timestamp);
  }
}

TEST_P(AudioDiscardDurationTrackerTest, TimestampRegression) {
  if (dmp_reader_.number_of_audio_buffers() < 2) {
    GTEST_SKIP() << "Too few audio buffers to run test.";
  }

  const int64_t kDiscardDurationUs = 10 * kSbTimeMillisecond;

  scoped_refptr<InputBuffer> first_input_buffer =
      GetAudioInputBuffer(&dmp_reader_, 0, kDiscardDurationUs, 0);
  scoped_refptr<InputBuffer> second_input_buffer =
      GetAudioInputBuffer(&dmp_reader_, 1, kDiscardDurationUs, 0);

  InputBuffers input_buffers = {first_input_buffer, second_input_buffer};
  AudioDiscardDurationTracker tracker;
  tracker.CacheMultipleDiscardDurations(input_buffers,
                                        kPassthroughBufferDurationUs);

  tracker.AdjustTimeForTotalDiscardDuration(second_input_buffer->timestamp());
  // Expect AudioDiscardDurationTracker not to crash when processing a timestamp
  // regression.
  tracker.AdjustTimeForTotalDiscardDuration(first_input_buffer->timestamp());
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
