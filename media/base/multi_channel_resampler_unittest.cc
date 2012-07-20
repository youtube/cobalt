// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/multi_channel_resampler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Just test a basic resampling case.  The SincResampler unit test will take
// care of accuracy testing; we just need to check that multichannel works as
// expected within some tolerance.
static const float kScaleFactor = 192000.0f / 44100.0f;

// Simulate large and small sample requests used by the different audio paths.
static const int kHighLatencySize = 8192;
// Low latency buffers show a larger error than high latency ones.  Which makes
// sense since each error represents a larger portion of the total request.
static const int kLowLatencySize = 128;

// Test fill value.
static const float kFillValue = 0.1f;

// Chosen arbitrarily based on what each resampler reported during testing.
static const double kLowLatencyMaxRMSError = 0.0036;
static const double kLowLatencyMaxError = 0.04;
static const double kHighLatencyMaxRMSError = 0.0036;
static const double kHighLatencyMaxError = 0.04;

class MultiChannelResamplerTest
    : public testing::TestWithParam<int> {
 public:
  MultiChannelResamplerTest() {}
  virtual ~MultiChannelResamplerTest() {
    if (!audio_data_.empty()) {
      for (size_t i = 0; i < audio_data_.size(); ++i)
        delete [] audio_data_[i];
      audio_data_.clear();
    }
  }

  void InitializeAudioData(int channels, int frames) {
    frames_ = frames;
    audio_data_.reserve(channels);
    for (int i = 0; i < channels; ++i) {
      audio_data_.push_back(new float[frames]);

      // Zero initialize so we can be sure every value has been provided.
      memset(audio_data_[i], 0, sizeof(*audio_data_[i]) * frames);
    }
  }

  // MultiChannelResampler::MultiChannelAudioSourceProvider implementation, just
  // fills the provided audio_data with |kFillValue|.
  virtual void ProvideInput(const std::vector<float*>& audio_data,
                            int number_of_frames) {
    EXPECT_EQ(audio_data.size(), audio_data_.size());
    for (size_t i = 0; i < audio_data.size(); ++i)
      for (int j = 0; j < number_of_frames; ++j)
        audio_data[i][j] = kFillValue;
  }

  void MultiChannelTest(int channels, int frames, double expected_max_rms_error,
                        double expected_max_error) {
    InitializeAudioData(channels, frames);
    MultiChannelResampler resampler(
        channels, kScaleFactor, base::Bind(
            &MultiChannelResamplerTest::ProvideInput,
            base::Unretained(this)));
    resampler.Resample(audio_data_, frames);
    TestValues(expected_max_rms_error, expected_max_error);
  }

  void HighLatencyTest(int channels) {
    MultiChannelTest(channels, kHighLatencySize, kHighLatencyMaxRMSError,
                     kHighLatencyMaxError);
  }

  void LowLatencyTest(int channels) {
    MultiChannelTest(channels, kLowLatencySize, kLowLatencyMaxRMSError,
                     kLowLatencyMaxError);
  }

  void TestValues(double expected_max_rms_error, double expected_max_error ) {
    // Calculate Root-Mean-Square-Error for the resampling.
    double max_error = 0.0;
    double sum_of_squares = 0.0;
    for (size_t i = 0; i < audio_data_.size(); ++i) {
      for (int j = 0; j < frames_; ++j) {
        // Ensure all values are accounted for.
        ASSERT_NE(audio_data_[i][j], 0);

        double error = fabs(audio_data_[i][j] - kFillValue);
        max_error = std::max(max_error, error);
        sum_of_squares += error * error;
      }
    }

    double rms_error = sqrt(
        sum_of_squares / (frames_ * audio_data_.size()));

    EXPECT_LE(rms_error, expected_max_rms_error);
    EXPECT_LE(max_error, expected_max_error);
  }

 protected:
  int frames_;
  std::vector<float*> audio_data_;

  DISALLOW_COPY_AND_ASSIGN(MultiChannelResamplerTest);
};

TEST_P(MultiChannelResamplerTest, HighLatency) {
  HighLatencyTest(GetParam());
}

TEST_P(MultiChannelResamplerTest, LowLatency) {
  LowLatencyTest(GetParam());
}

// Test common channel layouts: mono, stereo, 5.1, 7.1.
INSTANTIATE_TEST_CASE_P(
    MultiChannelResamplerTest, MultiChannelResamplerTest,
    testing::Values(1, 2, 6, 8));

}  // namespace media
