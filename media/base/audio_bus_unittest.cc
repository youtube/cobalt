// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/channel_layout.h"
#include "testing/gtest/include/gtest/gtest.h"

static const int kChannels = 6;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_5_1;
// Use a buffer size which is intentionally not a multiple of 16; see
// kChannelAlignment in audio_bus.cc.
static const int kFrameCount = 1234;

namespace media {

class AudioBusTest : public testing::Test {
 public:
  AudioBusTest() {}
  ~AudioBusTest() {
    for (size_t i = 0; i < data_.size(); ++i)
      base::AlignedFree(data_[i]);
  }

  // Validate parameters returned by AudioBus v.s. the constructed parameters.
  void VerifyParams(AudioBus* bus) {
    EXPECT_EQ(kChannels, bus->channels());
    EXPECT_EQ(kFrameCount, bus->frames());
  }

  void VerifyValue(const float data[], int size, float value) {
    for (int i = 0; i < size; ++i)
      ASSERT_FLOAT_EQ(value, data[i]);
  }

  // Read and write to the full extent of the allocated channel data.  Also test
  // the Zero() method and verify it does as advertised.  Also test data if data
  // is 16-byte aligned as advertised (see kChannelAlignment in audio_bus.cc).
  void VerifyChannelData(AudioBus* bus) {
    for (int i = 0; i < bus->channels(); ++i) {
      ASSERT_EQ(0U, reinterpret_cast<uintptr_t>(bus->channel(i)) & 0x0F);
      std::fill(bus->channel(i), bus->channel(i) + bus->frames(), i);
    }

    for (int i = 0; i < bus->channels(); ++i)
      VerifyValue(bus->channel(i), bus->frames(), i);

    bus->Zero();
    for (int i = 0; i < bus->channels(); ++i)
      VerifyValue(bus->channel(i), bus->frames(), 0);
  }


 protected:
  std::vector<float*> data_;

  DISALLOW_COPY_AND_ASSIGN(AudioBusTest);
};

// Verify basic Create(...) method works as advertised.
TEST_F(AudioBusTest, Create) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);
  VerifyParams(bus.get());
  VerifyChannelData(bus.get());
}

// Verify Create(...) using AudioParameters works as advertised.
TEST_F(AudioBusTest, CreateUsingAudioParameters) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(AudioParameters(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, 48000, 32,
      kFrameCount));
  VerifyParams(bus.get());
  VerifyChannelData(bus.get());
}

// Verify an AudioBus created via wrapping works as advertised.
TEST_F(AudioBusTest, Wrap) {
  data_.reserve(kChannels);
  for (int i = 0; i < kChannels; ++i) {
    data_.push_back(static_cast<float*>(base::AlignedAlloc(
        sizeof(*data_[i]) * kFrameCount, 16)));
  }

  scoped_ptr<AudioBus> bus = AudioBus::WrapVector(kFrameCount, data_);
  VerifyParams(bus.get());
  VerifyChannelData(bus.get());
}

// Simulate a shared memory transfer and verify results.
TEST_F(AudioBusTest, AudioData) {
  scoped_ptr<AudioBus> bus1 = AudioBus::Create(kChannels, kFrameCount);
  scoped_ptr<AudioBus> bus2 = AudioBus::Create(kChannels, kFrameCount);

  // Fill |bus1| with dummy data and zero out |bus2|.
  for (int i = 0; i < bus1->channels(); ++i)
    std::fill(bus1->channel(i), bus1->channel(i) + bus1->frames(), i);
  bus2->Zero();

  // Transfer audio data from |bus1| to |bus2|.
  ASSERT_EQ(bus1->data_size(), bus2->data_size());
  memcpy(bus2->data(), bus1->data(), bus1->data_size());

  for (int i = 0; i < bus2->channels(); ++i)
    VerifyValue(bus2->channel(i), bus2->frames(), i);
}

// Verify Zero() and ZeroFrames(...) utility methods work as advertised.
TEST_F(AudioBusTest, Zero) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);

  // First fill the bus with dummy data.
  for (int i = 0; i < bus->channels(); ++i)
    std::fill(bus->channel(i), bus->channel(i) + bus->frames(), i + 1);

  // Zero half the frames of each channel.
  bus->ZeroFrames(kFrameCount / 2);
  for (int i = 0; i < bus->channels(); ++i)
    VerifyValue(bus->channel(i), kFrameCount / 2, 0);

  // Zero all the frames of each channel.
  bus->Zero();
  for (int i = 0; i < bus->channels(); ++i)
    VerifyValue(bus->channel(i), bus->frames(), 0);
}

}  // namespace media
