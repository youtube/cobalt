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

#include "starboard/shared/starboard/player/filter/audio_frame_discarder.h"

#include <memory>
#include <utility>
#include <vector>

#include "starboard/shared/starboard/player/decoded_audio_internal.h"
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

using ::testing::ValuesIn;
using video_dmp::VideoDmpReader;

class AudioFrameDiscarderTest : public ::testing::TestWithParam<const char*> {
 public:
  AudioFrameDiscarderTest()
      : dmp_reader_(GetParam(), VideoDmpReader::kEnableReadOnDemand) {}

 protected:
  VideoDmpReader dmp_reader_;
};

scoped_refptr<DecodedAudio> MakeDecodedAudio(int channels, int64_t timestamp) {
  scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
      channels, kSbMediaAudioSampleTypeFloat32,
      kSbMediaAudioFrameStorageTypeInterleaved, timestamp,
      media::GetBytesPerSample(kSbMediaAudioSampleTypeFloat32) * channels *
          1536);

  memset(decoded_audio->data(), timestamp % 256,
         decoded_audio->size_in_bytes());

  return decoded_audio;
}

TEST_P(AudioFrameDiscarderTest, Empty) {
  AudioFrameDiscarder discarder;
  discarder.Reset();
  discarder.OnDecodedAudioEndOfStream();
  discarder.Reset();
}

TEST_P(AudioFrameDiscarderTest, NonPartialAudio) {
  InputBuffers input_buffers;
  std::vector<scoped_refptr<DecodedAudio>> decoded_audios;
  const auto& audio_stream_info = dmp_reader_.audio_stream_info();

  for (int i = 0; i < std::min<int>(32, dmp_reader_.number_of_audio_buffers());
       ++i) {
    input_buffers.push_back(GetAudioInputBuffer(&dmp_reader_, i));
    decoded_audios.push_back(
        MakeDecodedAudio(audio_stream_info.number_of_channels,
                         input_buffers.back()->timestamp()));
  }
  ASSERT_EQ(input_buffers.size(), decoded_audios.size());

  AudioFrameDiscarder discarder;

  for (size_t i = 0; i < input_buffers.size(); ++i) {
    discarder.OnInputBuffers({input_buffers[i]});
    auto copy = decoded_audios[i]->Clone();
    discarder.AdjustForDiscardedDurations(audio_stream_info.samples_per_second,
                                          &copy);
    ASSERT_EQ(*copy, *decoded_audios[i]);
  }

  discarder.Reset();

  discarder.OnInputBuffers(input_buffers);

  for (auto decoded_audio : decoded_audios) {
    auto copy = decoded_audio->Clone();
    discarder.AdjustForDiscardedDurations(audio_stream_info.samples_per_second,
                                          &copy);
    ASSERT_EQ(*copy, *decoded_audio);
  }
}

TEST_P(AudioFrameDiscarderTest, PartialAudio) {
  InputBuffers input_buffers;
  std::vector<scoped_refptr<DecodedAudio>> decoded_audios;
  const auto& audio_stream_info = dmp_reader_.audio_stream_info();
  const int64_t duration = media::AudioFramesToDuration(
      MakeDecodedAudio(audio_stream_info.number_of_channels, 0)->frames(),
      audio_stream_info.samples_per_second);

  for (int i = 0; i < std::min<int>(32, dmp_reader_.number_of_audio_buffers());
       ++i) {
    if (i % 2 == 0) {
      // Skip 1/4 of the duration from both front and back on even buffers.
      input_buffers.push_back(
          GetAudioInputBuffer(&dmp_reader_, i, duration / 4, duration / 4));
    } else {
      input_buffers.push_back(GetAudioInputBuffer(&dmp_reader_, i));
    }
    decoded_audios.push_back(
        MakeDecodedAudio(dmp_reader_.audio_stream_info().number_of_channels,
                         input_buffers.back()->timestamp()));
  }
  ASSERT_EQ(input_buffers.size(), decoded_audios.size());

  AudioFrameDiscarder discarder;

  for (size_t i = 0; i < input_buffers.size(); ++i) {
    discarder.OnInputBuffers({input_buffers[i]});
    auto copy = decoded_audios[i]->Clone();
    discarder.AdjustForDiscardedDurations(audio_stream_info.samples_per_second,
                                          &copy);
    if (i % 2 == 0) {
      ASSERT_NEAR(copy->frames(), decoded_audios[i]->frames() / 2, 2);
    } else {
      ASSERT_EQ(*copy, *decoded_audios[i]);
    }
  }

  discarder.Reset();

  discarder.OnInputBuffers(input_buffers);

  for (size_t i = 0; i < decoded_audios.size(); ++i) {
    auto copy = decoded_audios[i]->Clone();
    discarder.AdjustForDiscardedDurations(audio_stream_info.samples_per_second,
                                          &copy);
    if (i % 2 == 0) {
      ASSERT_NEAR(copy->frames(), decoded_audios[i]->frames() / 2, 2);
    } else {
      ASSERT_EQ(*copy, *decoded_audios[i]);
    }
  }
}

INSTANTIATE_TEST_CASE_P(
    AudioFrameDiscarderTests,
    AudioFrameDiscarderTest,
    ValuesIn(
        GetSupportedAudioTestFiles(kIncludeHeaac, 6, "audiopassthrough=false")),
    [](::testing::TestParamInfo<const char*> info) {
      std::string filename(info.param);
      std::replace(filename.begin(), filename.end(), '.', '_');
      return filename;
    });

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
