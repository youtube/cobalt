/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/mfcc/mfcc.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "glog/logging.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {

TEST(MfccTest, AgreesWithPythonGoldenValues) {
  Mfcc mfcc;
  std::vector<double> input;
  const int kSampleCount = 513;
  for (int i = 0; i < kSampleCount; ++i) {
    input.push_back(i + 1);
  }

  ASSERT_TRUE(mfcc.Initialize(input.size(), 22050 /*sample rate*/));

  std::vector<double> output;
  mfcc.Compute(input, &output);

  std::vector<double> expected = {29.13970072, -6.41568601, -0.61903012,
                             -0.96778652, -0.26819878, -0.40907028,
                             -0.15614748, -0.23203119, -0.10481487,
                             -0.1543029,  -0.0769791,  -0.10806114,
                             -0.06047613};

  ASSERT_EQ(expected.size(), output.size());
  for (int i = 0; i < output.size(); ++i) {
    EXPECT_NEAR(output[i], expected[i], 1e-04);
  }
}

TEST(MfccTest, AvoidsNansWithZeroInput) {
  Mfcc mfcc;
  std::vector<double> input;
  const int kSampleCount = 513;
  for (int i = 0; i < kSampleCount; ++i) {
    input.push_back(0.0);
  }

  ASSERT_TRUE(mfcc.Initialize(input.size(), 22050 /*sample rate*/));

  std::vector<double> output;
  mfcc.Compute(input, &output);

  int expected_size = 13;
  ASSERT_EQ(expected_size, output.size());
  EXPECT_THAT(output, testing::Each(testing::Truly(
                          [](double value) { return !isnan(value); })));
}

TEST(MfccTest, SimpleInputSaneResult) {
  Mfcc mfcc;
  mfcc.set_lower_frequency_limit(125.0);
  mfcc.set_upper_frequency_limit(3800.0);
  mfcc.set_filterbank_channel_count(40);
  mfcc.set_dct_coefficient_count(40);
  const int kSpectrogramSize = 129;
  std::vector<double> input(kSpectrogramSize, 0.0);

  // Simulate a low-frequency sinusoid from the spectrogram.
  const int kHotBin = 10;
  input[kHotBin] = 1.0;
  ASSERT_TRUE(mfcc.Initialize(input.size(), 8000));

  std::vector<double> output;
  mfcc.Compute(input, &output);

  // For a single low-frequency input, output beyond c_0 should look like
  // a slow cosine, with a slight delay.  Largest value will be c_1.
  EXPECT_EQ(output.begin() + 1, std::max_element(output.begin(), output.end()));
}

}  // namespace audio_dsp
