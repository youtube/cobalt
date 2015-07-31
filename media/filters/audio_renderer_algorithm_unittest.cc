// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The format of these tests are to enqueue a known amount of data and then
// request the exact amount we expect in order to dequeue the known amount of
// data.  This ensures that for any rate we are consuming input data at the
// correct rate.  We always pass in a very large destination buffer with the
// expectation that FillBuffer() will fill as much as it can but no more.

#include <cmath>

#include "base/bind.h"
#include "base/callback.h"
#include "media/base/channel_layout.h"
#include "media/base/data_buffer.h"
#include "media/filters/audio_renderer_algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const size_t kRawDataSize = 2048;
static const int kSamplesPerSecond = 3000;
static const ChannelLayout kDefaultChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kDefaultSampleBits = 16;

class AudioRendererAlgorithmTest : public testing::Test {
 public:
  AudioRendererAlgorithmTest()
      : bytes_enqueued_(0) {
  }

  ~AudioRendererAlgorithmTest() {}

  void Initialize() {
    Initialize(kDefaultChannelLayout, kDefaultSampleBits, kSamplesPerSecond);
  }

  void Initialize(ChannelLayout channel_layout, int bits_per_channel,
                  int samples_per_second) {
    static const int kFrames = kRawDataSize / ((kDefaultSampleBits / 8) *
        ChannelLayoutToChannelCount(kDefaultChannelLayout));
    AudioParameters params(
        media::AudioParameters::AUDIO_PCM_LINEAR, channel_layout,
        samples_per_second, bits_per_channel, kFrames);

    algorithm_.Initialize(1, params, base::Bind(
        &AudioRendererAlgorithmTest::EnqueueData, base::Unretained(this)));
    EnqueueData();
  }

  void EnqueueData() {
    scoped_array<uint8> audio_data(new uint8[kRawDataSize]);
    CHECK_EQ(kRawDataSize % algorithm_.bytes_per_channel(), 0u);
    CHECK_EQ(kRawDataSize % algorithm_.bytes_per_frame(), 0u);
    // The value of the data is meaningless; we just want non-zero data to
    // differentiate it from muted data.
    memset(audio_data.get(), 1, kRawDataSize);
    algorithm_.EnqueueBuffer(new DataBuffer(audio_data.Pass(), kRawDataSize));
    bytes_enqueued_ += kRawDataSize;
  }

  void CheckFakeData(uint8* audio_data, int frames_written) {
    int sum = 0;
    for (int i = 0; i < frames_written * algorithm_.bytes_per_frame(); ++i)
      sum |= audio_data[i];

    if (algorithm_.is_muted())
      ASSERT_EQ(sum, 0);
    else
      ASSERT_NE(sum, 0);
  }

  int ComputeConsumedBytes(int initial_bytes_enqueued,
                           int initial_bytes_buffered) {
    int byte_delta = bytes_enqueued_ - initial_bytes_enqueued;
    int buffered_delta = algorithm_.bytes_buffered() - initial_bytes_buffered;
    int consumed = byte_delta - buffered_delta;
    CHECK_GE(consumed, 0);
    return consumed;
  }

  void TestPlaybackRate(double playback_rate) {
    const int kDefaultBufferSize = algorithm_.samples_per_second() / 10;
    const int kDefaultFramesRequested = 2 * algorithm_.samples_per_second();

    TestPlaybackRate(playback_rate, kDefaultBufferSize,
                     kDefaultFramesRequested);
  }

  void TestPlaybackRate(double playback_rate,
                        int buffer_size_in_frames,
                        int total_frames_requested) {
    int initial_bytes_enqueued = bytes_enqueued_;
    int initial_bytes_buffered = algorithm_.bytes_buffered();

    algorithm_.SetPlaybackRate(static_cast<float>(playback_rate));

    scoped_array<uint8> buffer(
        new uint8[buffer_size_in_frames * algorithm_.bytes_per_frame()]);

    if (playback_rate == 0.0) {
      int frames_written =
          algorithm_.FillBuffer(buffer.get(), buffer_size_in_frames);
      EXPECT_EQ(0, frames_written);
      return;
    }

    int frames_remaining = total_frames_requested;
    while (frames_remaining > 0) {
      int frames_requested = std::min(buffer_size_in_frames, frames_remaining);
      int frames_written =
          algorithm_.FillBuffer(buffer.get(), frames_requested);
      ASSERT_GT(frames_written, 0);
      CheckFakeData(buffer.get(), frames_written);
      frames_remaining -= frames_written;
    }

    int bytes_requested = total_frames_requested * algorithm_.bytes_per_frame();
    int bytes_consumed = ComputeConsumedBytes(initial_bytes_enqueued,
                                              initial_bytes_buffered);

    // If playing back at normal speed, we should always get back the same
    // number of bytes requested.
    if (playback_rate == 1.0) {
      EXPECT_EQ(bytes_requested, bytes_consumed);
      return;
    }

    // Otherwise, allow |kMaxAcceptableDelta| difference between the target and
    // actual playback rate.
    // When |kSamplesPerSecond| and |total_frames_requested| are reasonably
    // large, one can expect less than a 1% difference in most cases. In our
    // current implementation, sped up playback is less accurate than slowed
    // down playback, and for playback_rate > 1, playback rate generally gets
    // less and less accurate the farther it drifts from 1 (though this is
    // nonlinear).
    static const double kMaxAcceptableDelta = 0.01;
    double actual_playback_rate = 1.0 * bytes_consumed / bytes_requested;

    // Calculate the percentage difference from the target |playback_rate| as a
    // fraction from 0.0 to 1.0.
    double delta = std::abs(1.0 - (actual_playback_rate / playback_rate));

    EXPECT_LE(delta, kMaxAcceptableDelta);
  }

 protected:
  AudioRendererAlgorithm algorithm_;
  int bytes_enqueued_;
};

TEST_F(AudioRendererAlgorithmTest, FillBuffer_NormalRate) {
  Initialize();
  TestPlaybackRate(1.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_NearlyNormalFasterRate) {
  Initialize();
  TestPlaybackRate(1.0001);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_NearlyNormalSlowerRate) {
  Initialize();
  TestPlaybackRate(0.9999);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_OneAndAQuarterRate) {
  Initialize();
  TestPlaybackRate(1.25);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_OneAndAHalfRate) {
  Initialize();
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_DoubleRate) {
  Initialize();
  TestPlaybackRate(2.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_EightTimesRate) {
  Initialize();
  TestPlaybackRate(8.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_ThreeQuartersRate) {
  Initialize();
  TestPlaybackRate(0.75);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_HalfRate) {
  Initialize();
  TestPlaybackRate(0.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_QuarterRate) {
  Initialize();
  TestPlaybackRate(0.25);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_Pause) {
  Initialize();
  TestPlaybackRate(0.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_SlowDown) {
  Initialize();
  TestPlaybackRate(4.5);
  TestPlaybackRate(3.0);
  TestPlaybackRate(2.0);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(0.25);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_SpeedUp) {
  Initialize();
  TestPlaybackRate(0.25);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.0);
  TestPlaybackRate(2.0);
  TestPlaybackRate(3.0);
  TestPlaybackRate(4.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_JumpAroundSpeeds) {
  Initialize();
  TestPlaybackRate(2.1);
  TestPlaybackRate(0.9);
  TestPlaybackRate(0.6);
  TestPlaybackRate(1.4);
  TestPlaybackRate(0.3);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_SmallBufferSize) {
  Initialize();
  static const int kBufferSizeInFrames = 1;
  static const int kFramesRequested = 2 * kSamplesPerSecond;
  TestPlaybackRate(1.0, kBufferSizeInFrames, kFramesRequested);
  TestPlaybackRate(0.5, kBufferSizeInFrames, kFramesRequested);
  TestPlaybackRate(1.5, kBufferSizeInFrames, kFramesRequested);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_LargeBufferSize) {
  Initialize(kDefaultChannelLayout, kDefaultSampleBits, 44100);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_LowerQualityAudio) {
  static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_MONO;
  static const int kSampleBits = 8;
  Initialize(kChannelLayout, kSampleBits, kSamplesPerSecond);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_HigherQualityAudio) {
  static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
  static const int kSampleBits = 32;
  Initialize(kChannelLayout, kSampleBits, kSamplesPerSecond);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.5);
}

}  // namespace media
