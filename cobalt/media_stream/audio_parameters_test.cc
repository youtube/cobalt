// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media_stream/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media_stream {

TEST(AudioParameters, Constructor_ParametersValues) {
  int channel_count = 6;
  int sample_rate = 20000;
  int bits_per_sample = 24;
  AudioParameters params(channel_count, sample_rate, bits_per_sample);

  EXPECT_EQ(channel_count, params.channel_count());
  EXPECT_EQ(bits_per_sample, params.bits_per_sample());
  EXPECT_EQ(sample_rate, params.sample_rate());
}

TEST(AudioParameters, AsHumanReadableString) {
  int channel_count = 6;
  int sample_rate = 20000;
  int bits_per_sample = 24;
  AudioParameters params(channel_count, sample_rate, bits_per_sample);
  std::string params_string = params.AsHumanReadableString();
  EXPECT_EQ("channel_count: 6 sample_rate: 20000 bits_per_sample: 24",
            params_string);
}

TEST(AudioParameters, IsValid_Parameterized) {
  int channel_count = 1;
  int sample_rate = 44100;
  int bits_per_sample = 16;
  AudioParameters params(channel_count, sample_rate, bits_per_sample);
  EXPECT_TRUE(params.IsValid());
}

TEST(AudioParameters, GetBitsPerSecond) {
  int channel_count = 2;
  int sample_rate = 44100;
  int bits_per_sample = 16;
  AudioParameters params(channel_count, sample_rate, bits_per_sample);
  EXPECT_EQ(2 * 44100 * 16, params.GetBitsPerSecond());
}

TEST(AudioParameters, Compare) {
  int channel_count = 1;
  int sample_rate = 44100;
  int bits_per_sample = 16;
  AudioParameters params1(channel_count, sample_rate, bits_per_sample);
  AudioParameters params2(channel_count, sample_rate, bits_per_sample);
  AudioParameters params3(channel_count, sample_rate + 1, bits_per_sample);
  EXPECT_EQ(params1, params2);
  EXPECT_NE(params2, params3);
}

}  // namespace media_stream
}  // namespace cobalt
