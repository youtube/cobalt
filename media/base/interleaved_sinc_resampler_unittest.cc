/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <math.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/interleaved_sinc_resampler.h"
#include "media/base/sinc_resampler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Used to compare if two samples are the same. Because the resampled result
// from the SincResampler and the InterleavedSincResampler can be slightly
// different as the first one may use the SSE Convolve function.
const float kEpsilon = 0.0001f;

bool AreSamplesSame(float sample1, float sample2) {
  return fabs(sample1 - sample2) < kEpsilon;
}

// Function to provide the audio data of a single channel indicated by
// |channel_index| inside a multi channel audio stream to SincResampler.
void ReadCB(const float* source,
            const int source_size,
            int channel_index,
            int channel_count,
            int* offset,
            float* destination,
            int samples) {
  int samples_to_copy = std::min(source_size - *offset, samples);
  samples -= samples_to_copy;

  while (samples_to_copy != 0) {
    *destination++ = source[*offset * channel_count + channel_index];
    --samples_to_copy;
    ++*offset;
  }
  if (samples != 0) {
    memset(destination, 0, sizeof(float) * samples);
  }
}

class TestBuffer : public Buffer {
 public:
  TestBuffer(const void* data, int data_size)
      : Buffer(base::TimeDelta(), base::TimeDelta()),
        data_(static_cast<const uint8*>(data)),
        data_size_(data_size) {}

  const uint8* GetData() const override { return data_; }

  int GetDataSize() const override { return data_size_; }

 private:
  const uint8* data_;
  int data_size_;
};

}  // namespace

TEST(InterleavedSincResamplerTest, InitialState) {
  InterleavedSincResampler interleaved_resampler(1, 1);
  float output[1];

  ASSERT_FALSE(interleaved_resampler.ReachedEOS());
  ASSERT_TRUE(interleaved_resampler.CanQueueBuffer());
  ASSERT_FALSE(interleaved_resampler.Resample(output, 1));
}

TEST(InterleavedSincResamplerTest, Read) {
  const int kInputFrames = 1024;
  const int kOutputFrames = kInputFrames * 2;
  float samples[kInputFrames] = {0.0};
  float output[kOutputFrames];

  InterleavedSincResampler interleaved_resampler(
      static_cast<double>(kInputFrames) / kOutputFrames, 1);

  interleaved_resampler.QueueBuffer(new TestBuffer(samples, sizeof(samples)));
  ASSERT_FALSE(interleaved_resampler.Resample(output, kOutputFrames + 1));

  while (interleaved_resampler.CanQueueBuffer()) {
    interleaved_resampler.QueueBuffer(new TestBuffer(samples, sizeof(samples)));
  }

  // There is really no guarantee that we can read more.
  ASSERT_TRUE(interleaved_resampler.Resample(output, 1));
}

TEST(InterleavedSincResamplerTest, ReachedEOS) {
  const int kInputFrames = 512 * 3 + 32;
  const int kOutputFrames = kInputFrames * 2;
  float input[kInputFrames] = {0.0};

  InterleavedSincResampler interleaved_resampler(
      static_cast<double>(kInputFrames) / kOutputFrames, 1);

  interleaved_resampler.QueueBuffer(new TestBuffer(input, sizeof(input)));
  interleaved_resampler.QueueBuffer(new TestBuffer(NULL, 0));  // EOS

  ASSERT_FALSE(interleaved_resampler.ReachedEOS());

  float output[kOutputFrames];

  ASSERT_TRUE(interleaved_resampler.Resample(output, kOutputFrames - 4));
  ASSERT_FALSE(interleaved_resampler.ReachedEOS());

  ASSERT_TRUE(interleaved_resampler.Resample(output, 4));
  ASSERT_TRUE(interleaved_resampler.ReachedEOS());
}

// As InterleavedSincResampler is just the interleaved version of SincResampler,
// the following unit tests just try to verify that the results of using
// InterleavedSincResampler are the same as using SincResampler on individual
// channel.
TEST(InterleavedSincResamplerTest, ResampleSingleChannel) {
  const int kInputFrames = 1719;
  // Read twice of the frames out to ensure that we saturate the input frames.
  const int kOutputFrames = kInputFrames * 2;
  const double kResampleRatio = 44100. / 48000.;
  float input[kInputFrames];

  // Filled the samples
  for (int i = 0; i < kInputFrames; ++i) {
    input[i] = i / static_cast<float>(kInputFrames);
  }

  int offset = 0;
  SincResampler sinc_resampler(
      kResampleRatio, base::Bind(ReadCB, input, kInputFrames, 0, 1, &offset));
  InterleavedSincResampler interleaved_resampler(kResampleRatio, 1);

  interleaved_resampler.QueueBuffer(new TestBuffer(input, sizeof(input)));
  interleaved_resampler.QueueBuffer(new TestBuffer(NULL, 0));  // EOS

  float non_interleaved_output[kOutputFrames];
  float interleaved_output[kOutputFrames];

  sinc_resampler.Resample(non_interleaved_output, kOutputFrames);
  ASSERT_TRUE(
      interleaved_resampler.Resample(interleaved_output, kOutputFrames));

  for (int i = 0; i < kOutputFrames; ++i) {
    ASSERT_TRUE(
        AreSamplesSame(non_interleaved_output[i], interleaved_output[i]));
  }
}

TEST(InterleavedSincResamplerTest, ResampleMultipleChannels) {
  const int kChannelCount = 3;
  const int kInputFrames = 8737;
  // Read twice of the frames out to ensure that we saturate the input frames.
  const int kOutputFrames = kInputFrames * 2;
  const double kResampleRatio = 44100. / 48000.;
  float input[kInputFrames * kChannelCount];

  // Filled the buffer with different samples per frame on different channels.
  for (int i = 0; i < kInputFrames * kChannelCount; ++i) {
    input[i] = i / static_cast<float>(kInputFrames * kChannelCount);
  }

  float non_interleaved_outputs[kChannelCount][kInputFrames * 2];

  for (int i = 0; i < kChannelCount; ++i) {
    int offset = 0;
    SincResampler sinc_resampler(
        kResampleRatio,
        base::Bind(ReadCB, input, kInputFrames, i, kChannelCount, &offset));
    sinc_resampler.Resample(non_interleaved_outputs[i], kOutputFrames);
  }

  InterleavedSincResampler interleaved_resampler(kResampleRatio, kChannelCount);
  interleaved_resampler.QueueBuffer(new TestBuffer(input, sizeof(input)));
  interleaved_resampler.QueueBuffer(new TestBuffer(NULL, 0));  // EOS

  float interleaved_output[kOutputFrames * kChannelCount];

  ASSERT_TRUE(
      interleaved_resampler.Resample(interleaved_output, kOutputFrames));

  for (int i = 0; i < kOutputFrames; ++i) {
    for (int channel = 0; channel < kChannelCount; ++channel) {
      ASSERT_TRUE(
          AreSamplesSame(non_interleaved_outputs[channel][i],
                         interleaved_output[i * kChannelCount + channel]));
    }
  }
}

TEST(InterleavedSincResamplerTest, Benchmark) {
  const int kChannelCount = 8;
  const int kInputFrames = 44100;
  const int kNumberOfIterations = 100;
  const int kOutputFrames = kInputFrames * 2;
  const double kResampleRatio = 44100. / 48000.;
  std::vector<float> input(kInputFrames * kChannelCount);

  // Filled the buffer with different samples per frame on different channels.
  for (int i = 0; i < kInputFrames * kChannelCount; ++i) {
    input[i] = i / static_cast<float>(kInputFrames * kChannelCount);
  }

  InterleavedSincResampler interleaved_resampler(kResampleRatio, kChannelCount);

  base::TimeTicks start = base::TimeTicks::HighResNow();
  std::vector<float> interleaved_output(kOutputFrames * kChannelCount);
  int total_output_frames = 0;

  for (int i = 0; i < kNumberOfIterations; ++i) {
    interleaved_resampler.QueueBuffer(
        new TestBuffer(&input[0], sizeof(float) * input.size()));
    if (interleaved_resampler.Resample(&interleaved_output[0], kOutputFrames)) {
      total_output_frames += kOutputFrames;
    }
  }

  double total_time_c_ms =
      (base::TimeTicks::HighResNow() - start).InMillisecondsF();
  printf(
      "Benchmarking InterleavedSincResampler in %d channels for %d "
      "iterations took %.4gms.\n:\n",
      kChannelCount, kNumberOfIterations, total_time_c_ms);

  start = base::TimeTicks::HighResNow();

  int offset = 0;
  SincResampler sinc_resampler(
      kResampleRatio,
      base::Bind(ReadCB, &input[0], kInputFrames, 0, 1, &offset));

  while (total_output_frames > 0) {
    sinc_resampler.Resample(&interleaved_output[0], kOutputFrames);
    total_output_frames -= kOutputFrames;
    // Set offset to 0 so we'll never reach EOS to enforce a sample by sample
    // copy for every frame as previously we have to convert interleaved stream
    // to non-interleaved stream to use MultiChannelResampler and then convert
    // the result stream back to interleaved.
    offset = 0;
  }

  total_time_c_ms = (base::TimeTicks::HighResNow() - start).InMillisecondsF();
  printf(
      "Benchmarking SincResampler with one channel for %d iterations took "
      "%.4gms.\n:\n",
      kNumberOfIterations, total_time_c_ms);
}

}  // namespace media
