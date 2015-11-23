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

#include "media/base/shell_audio_bus.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace testing {
namespace {

template <typename T>
bool VerifyValues(const T* data, size_t size, T value) {
  for (size_t i = 0; i < size; ++i) {
    if (data[i] != value) {
      return false;
    }
  }
  return true;
}

template <typename T>
void FillValues(T* data, size_t size, T value) {
  for (size_t i = 0; i < size; ++i) {
    data[i] = value;
  }
}

void Scribble(uint8* data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    data[i] = static_cast<uint8>(i + 0x34);
  }
}

bool VerifyScribble(const uint8* data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (data[i] != static_cast<uint8>(i + 0x34)) {
      return false;
    }
  }
  return true;
}

// This function only works with audio bus containing float32 samples.
bool IsSameChannel(const ShellAudioBus& audio_bus_1,
                   size_t channel_1,
                   const ShellAudioBus& audio_bus_2,
                   size_t channel_2) {
  if (audio_bus_1.frames() != audio_bus_2.frames()) {
    return false;
  }
  for (size_t frame = 0; frame < audio_bus_1.frames(); ++frame) {
    if (fabsf(audio_bus_1.GetFloat32Sample(channel_1, frame) -
              audio_bus_2.GetFloat32Sample(channel_2, frame)) > 0.001) {
      return false;
    }
  }
  return true;
}

// This class allocates buffer with extra guard bytes in the front and back of
// the buffer.  It can be used to verify if the ShellAudioBus implementation
// writes any data out of boundary.
class GuardedBuffers {
 public:
  GuardedBuffers(size_t number_of_buffers, size_t bytes_per_buffer) {
    std::vector<uint8> buffer(bytes_per_buffer + kGuardBytes * 2);
    buffers_.resize(number_of_buffers, buffer);
    ScribbleContent();
  }

  void ScribbleContent() {
    for (size_t i = 0; i < buffers_.size(); ++i) {
      Scribble(&buffers_[i][0], kGuardBytes);
      Scribble(&buffers_[i][buffers_[i].size() - kGuardBytes], kGuardBytes);
      Scribble(&buffers_[i][kGuardBytes], buffers_[i].size() - kGuardBytes * 2);
    }
  }

  bool VerifyGuardBytes() const {
    for (size_t i = 0; i < buffers_.size(); ++i) {
      if (!VerifyScribble(&buffers_[i][0], kGuardBytes) ||
          !VerifyScribble(&buffers_[i][buffers_[i].size() - kGuardBytes],
                          kGuardBytes)) {
        return false;
      }
    }
    return true;
  }

  template <typename T>
  T* GetBuffer(size_t index) {
    return reinterpret_cast<T*>(&buffers_.at(index)[kGuardBytes]);
  }

 private:
  typedef std::vector<uint8> Buffer;
  typedef std::vector<Buffer> Buffers;

  static const size_t kGuardBytes = 256;

  Buffers buffers_;
};

class ShellAudioBusTest : public ::testing::Test {
 public:
  typedef ShellAudioBus::SampleType SampleType;
  typedef ShellAudioBus::StorageType StorageType;

  ~ShellAudioBusTest() {
    // We do an extra call to VerifyGuardBytes() just in case if it was omitted
    // in a test.  It is still recommended to verify the integrity in individual
    // tests so the failure message is more informational.
    if (guarded_buffers_) {
      EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
    }
  }

  template <typename T>
  void CreateAudioBus(StorageType storage_type) {
    // We do an extra call to VerifyGuardBytes() just in case if it was omitted
    // in a test.  It is still recommended to verify the integrity in individual
    // tests so the failure message is more informational.
    if (guarded_buffers_) {
      EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
    }
    if (storage_type == ShellAudioBus::kPlanar) {
      guarded_buffers_.reset(
          new GuardedBuffers(kChannels, sizeof(T) * kFrames));
      std::vector<T*> samples;
      for (size_t channel = 0; channel < kChannels; ++channel) {
        samples.push_back(guarded_buffers_->GetBuffer<T>(channel));
      }
      audio_bus_.reset(new ShellAudioBus(kFrames, samples));
    } else {
      guarded_buffers_.reset(
          new GuardedBuffers(1, sizeof(T) * kFrames * kChannels));
      audio_bus_.reset(new ShellAudioBus(kChannels, kFrames,
                                         guarded_buffers_->GetBuffer<T>(0)));
    }
  }

 protected:
  static const size_t kChannels;
  static const size_t kFrames;

  scoped_ptr<GuardedBuffers> guarded_buffers_;
  scoped_ptr<ShellAudioBus> audio_bus_;
};

const size_t ShellAudioBusTest::kChannels = 3;
const size_t ShellAudioBusTest::kFrames = 809;

TEST_F(ShellAudioBusTest, ConstructorWithAllocation) {
  ShellAudioBus audio_bus(kChannels, kFrames, ShellAudioBus::kFloat32,
                          ShellAudioBus::kPlanar);
  EXPECT_EQ(audio_bus.channels(), kChannels);
  EXPECT_EQ(audio_bus.frames(), kFrames);
  for (size_t channel = 0; channel < audio_bus.channels(); ++channel) {
    const uint8* samples = audio_bus.planar_data(channel);
    EXPECT_EQ(reinterpret_cast<intptr_t>(samples) %
                  ShellAudioBus::kChannelAlignmentInBytes,
              0);
  }
}

TEST_F(ShellAudioBusTest, ConstructorWithoutAllocation) {
  CreateAudioBus<float>(ShellAudioBus::kInterleaved);
  EXPECT_EQ(audio_bus_->channels(), kChannels);
  EXPECT_EQ(audio_bus_->frames(), kFrames);
  const float* samples =
      reinterpret_cast<const float*>(audio_bus_->interleaved_data());
  EXPECT_EQ(samples, guarded_buffers_->GetBuffer<float>(0));
  EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());

  CreateAudioBus<int16>(ShellAudioBus::kPlanar);
  EXPECT_EQ(audio_bus_->channels(), kChannels);
  EXPECT_EQ(audio_bus_->frames(), kFrames);
  for (size_t channel = 0; channel < audio_bus_->channels(); ++channel) {
    const int16* samples =
        reinterpret_cast<const int16*>(audio_bus_->planar_data(channel));
    EXPECT_EQ(samples, guarded_buffers_->GetBuffer<int16>(channel));
  }
  EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
}

TEST_F(ShellAudioBusTest, GetSampleSizeInBytes) {
  ShellAudioBus float_audio_bus(2, 4, ShellAudioBus::kFloat32,
                                ShellAudioBus::kPlanar);
  EXPECT_EQ(float_audio_bus.GetSampleSizeInBytes(), sizeof(float));
  ShellAudioBus int16_audio_bus(2, 4, ShellAudioBus::kInt16,
                                ShellAudioBus::kPlanar);
  EXPECT_EQ(int16_audio_bus.GetSampleSizeInBytes(), sizeof(int16));
}

TEST_F(ShellAudioBusTest, GetSample) {
  CreateAudioBus<float>(ShellAudioBus::kInterleaved);
  // Mark the first sample of the first channel as 1.f.
  guarded_buffers_->GetBuffer<float>(0)[0] = 1.f;
  // Mark the last sample of the last channel as 1.f.
  guarded_buffers_->GetBuffer<float>(0)[kFrames * kChannels - 1] = 1.f;
  EXPECT_EQ(audio_bus_->GetFloat32Sample(0, 0), 1.f);
  EXPECT_EQ(audio_bus_->GetFloat32Sample(kChannels - 1, kFrames - 1), 1.f);
  EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());

  CreateAudioBus<int16>(ShellAudioBus::kPlanar);
  for (size_t channel = 0; channel < audio_bus_->channels(); ++channel) {
    // Mark the first sample of the channel as 100.
    guarded_buffers_->GetBuffer<int16>(channel)[0] = 100;
    // Mark the last sample of the channel as 100.
    guarded_buffers_->GetBuffer<int16>(channel)[kFrames - 1] = 100;
    EXPECT_EQ(audio_bus_->GetInt16Sample(channel, 0), 100);
    EXPECT_EQ(audio_bus_->GetInt16Sample(channel, kFrames - 1), 100);
  }
  EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
}

TEST_F(ShellAudioBusTest, ZeroFrames) {
  CreateAudioBus<float>(ShellAudioBus::kInterleaved);
  Scribble(guarded_buffers_->GetBuffer<uint8>(0),
           sizeof(float) * kFrames * kChannels);
  audio_bus_->ZeroFrames(0, 0);
  audio_bus_->ZeroFrames(kFrames, kFrames);
  EXPECT_TRUE(VerifyScribble(guarded_buffers_->GetBuffer<uint8>(0),
                             sizeof(float) * kFrames * kChannels));
  EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());

  // Mark the first sample of last channel and second sample of the first
  // channel as 1.f.
  guarded_buffers_->GetBuffer<float>(0)[kChannels - 1] = 1.f;
  guarded_buffers_->GetBuffer<float>(0)[kChannels] = 1.f;
  audio_bus_->ZeroFrames(0, 1);
  EXPECT_EQ(guarded_buffers_->GetBuffer<float>(0)[kChannels - 1], 0.f);
  EXPECT_EQ(guarded_buffers_->GetBuffer<float>(0)[kChannels], 1.f);
  EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());

  // Mark the first sample of last channel and second but last sample of the
  // first channel as 1.f.
  guarded_buffers_->GetBuffer<float>(0)[kChannels * (kFrames - 1)] = 1.f;
  guarded_buffers_->GetBuffer<float>(0)[kChannels * (kFrames - 1) - 1] = 1.f;
  audio_bus_->ZeroFrames(kFrames - 1, kFrames);
  EXPECT_EQ(guarded_buffers_->GetBuffer<float>(0)[kChannels * (kFrames - 1)],
            0.f);
  EXPECT_EQ(
      guarded_buffers_->GetBuffer<float>(0)[kChannels * (kFrames - 1) - 1],
      1.f);

  audio_bus_->ZeroFrames(1, kFrames - 1);
  EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<float>(0),
                           kFrames * kChannels, 0.f));
  EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());

  CreateAudioBus<int16>(ShellAudioBus::kPlanar);
  for (size_t channel = 0; channel < audio_bus_->channels(); ++channel) {
    // Mark the first two samples of the channel as 100.
    guarded_buffers_->GetBuffer<int16>(channel)[0] = 100;
    guarded_buffers_->GetBuffer<int16>(channel)[1] = 100;
    audio_bus_->ZeroFrames(0, 1);
    EXPECT_EQ(guarded_buffers_->GetBuffer<int16>(channel)[0], 0);
    EXPECT_EQ(guarded_buffers_->GetBuffer<int16>(channel)[1], 100);
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());

    // Mark the last two samples of the channel as 100.
    guarded_buffers_->GetBuffer<int16>(channel)[kFrames - 1] = 100;
    guarded_buffers_->GetBuffer<int16>(channel)[kFrames - 2] = 100;
    audio_bus_->ZeroFrames(kFrames - 1, kFrames);
    EXPECT_EQ(guarded_buffers_->GetBuffer<int16>(channel)[kFrames - 1], 0);
    EXPECT_EQ(guarded_buffers_->GetBuffer<int16>(channel)[kFrames - 2], 100);
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());

    audio_bus_->ZeroFrames(1, kFrames - 1);
    EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<int16>(channel),
                             kFrames, static_cast<int16>(0)));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }
}

TEST_F(ShellAudioBusTest, AssignWithSameSampleTypeAndStorageType) {
  CreateAudioBus<float>(ShellAudioBus::kInterleaved);
  FillValues(guarded_buffers_->GetBuffer<float>(0), kFrames * kChannels, 1.f);
  {
    ShellAudioBus source(kChannels, kFrames / 2, ShellAudioBus::kFloat32,
                         ShellAudioBus::kInterleaved);
    source.ZeroAllFrames();
    audio_bus_->Assign(source);
    EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<float>(0),
                             kFrames / 2 * kChannels, 0.f));
    EXPECT_TRUE(VerifyValues(
        guarded_buffers_->GetBuffer<float>(0) + kFrames / 2 * kChannels,
        (kFrames - kFrames / 2) * kChannels, 1.f));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }

  {
    ShellAudioBus source(kChannels, kFrames, ShellAudioBus::kFloat32,
                         ShellAudioBus::kInterleaved);
    source.ZeroAllFrames();
    audio_bus_->Assign(source);
    EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<float>(0),
                             kFrames * kChannels, 0.f));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }

  {
    ShellAudioBus source(kChannels, kFrames * 2, ShellAudioBus::kFloat32,
                         ShellAudioBus::kInterleaved);
    source.ZeroAllFrames();
    audio_bus_->Assign(source);
    EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<float>(0),
                             kFrames * kChannels, 0.f));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }

  CreateAudioBus<int16>(ShellAudioBus::kPlanar);
  for (size_t channel = 0; channel < audio_bus_->channels(); ++channel) {
    FillValues(guarded_buffers_->GetBuffer<int16>(channel), kFrames,
               static_cast<int16>(100));
    {
      ShellAudioBus source(kChannels, kFrames / 2, ShellAudioBus::kInt16,
                           ShellAudioBus::kPlanar);
      source.ZeroAllFrames();
      audio_bus_->Assign(source);
      EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<int16>(channel),
                               kFrames / 2, static_cast<int16>(0)));
      EXPECT_TRUE(VerifyValues(
          guarded_buffers_->GetBuffer<int16>(channel) + kFrames / 2,
          kFrames - kFrames / 2, static_cast<int16>(100)));
      EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
    }

    {
      ShellAudioBus source(kChannels, kFrames, ShellAudioBus::kInt16,
                           ShellAudioBus::kPlanar);
      source.ZeroAllFrames();
      audio_bus_->Assign(source);
      EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<int16>(channel),
                               kFrames, static_cast<int16>(0)));
      EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
    }

    {
      ShellAudioBus source(kChannels, kFrames * 2, ShellAudioBus::kInt16,
                           ShellAudioBus::kPlanar);
      source.ZeroAllFrames();
      audio_bus_->Assign(source);
      EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<int16>(channel),
                               kFrames, static_cast<int16>(0)));
      EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
    }
  }
}

TEST_F(ShellAudioBusTest, AssignWithDifferentSampleTypesAndStorageTypes) {
  CreateAudioBus<float>(ShellAudioBus::kInterleaved);
  FillValues(guarded_buffers_->GetBuffer<float>(0), kFrames * kChannels, 1.f);
  {
    ShellAudioBus source(kChannels, kFrames / 2, ShellAudioBus::kInt16,
                         ShellAudioBus::kPlanar);
    source.ZeroAllFrames();
    audio_bus_->Assign(source);
    EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<float>(0),
                             kFrames / 2 * kChannels, 0.f));
    EXPECT_TRUE(VerifyValues(
        guarded_buffers_->GetBuffer<float>(0) + kFrames / 2 * kChannels,
        (kFrames - kFrames / 2) * kChannels, 1.f));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }

  {
    ShellAudioBus source(kChannels, kFrames, ShellAudioBus::kInt16,
                         ShellAudioBus::kPlanar);
    source.ZeroAllFrames();
    audio_bus_->Assign(source);
    EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<float>(0),
                             kFrames * kChannels, 0.f));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }

  {
    ShellAudioBus source(kChannels, kFrames * 2, ShellAudioBus::kInt16,
                         ShellAudioBus::kPlanar);
    source.ZeroAllFrames();
    audio_bus_->Assign(source);
    EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<float>(0),
                             kFrames * kChannels, 0.f));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }

  CreateAudioBus<int16>(ShellAudioBus::kPlanar);
  for (size_t channel = 0; channel < audio_bus_->channels(); ++channel) {
    FillValues(guarded_buffers_->GetBuffer<int16>(channel), kFrames,
               static_cast<int16>(100));
    {
      ShellAudioBus source(kChannels, kFrames / 2, ShellAudioBus::kFloat32,
                           ShellAudioBus::kInterleaved);
      source.ZeroAllFrames();
      audio_bus_->Assign(source);
      EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<int16>(channel),
                               kFrames / 2, static_cast<int16>(0)));
      EXPECT_TRUE(VerifyValues(
          guarded_buffers_->GetBuffer<int16>(channel) + kFrames / 2,
          kFrames - kFrames / 2, static_cast<int16>(100)));
      EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
    }

    {
      ShellAudioBus source(kChannels, kFrames, ShellAudioBus::kFloat32,
                           ShellAudioBus::kInterleaved);
      source.ZeroAllFrames();
      audio_bus_->Assign(source);
      EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<int16>(channel),
                               kFrames, static_cast<int16>(0)));
      EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
    }

    {
      ShellAudioBus source(kChannels, kFrames * 2, ShellAudioBus::kFloat32,
                           ShellAudioBus::kInterleaved);
      source.ZeroAllFrames();
      audio_bus_->Assign(source);
      EXPECT_TRUE(VerifyValues(guarded_buffers_->GetBuffer<int16>(channel),
                               kFrames, static_cast<int16>(0)));
      EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
    }
  }
}

TEST_F(ShellAudioBusTest, AssignWithMatrix) {
  CreateAudioBus<float>(ShellAudioBus::kInterleaved);
  // Fill the 1st channel with incrementally positive samples, fill the 2nd
  // channel with decrementally negative samples, and fill the 3rd channel with
  // 0.
  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t frame = 0; frame < kFrames; ++frame) {
      float sample = 0.f;
      if (channel == 0) {
        sample = static_cast<float>(frame) / static_cast<float>(kFrames);
      } else if (channel == 1) {
        sample = -static_cast<float>(frame) / static_cast<float>(kFrames);
      }
      guarded_buffers_->GetBuffer<float>(0)[frame * kChannels + channel] =
          sample;
    }
  }
  {
    ShellAudioBus other(kChannels * 2, kFrames, ShellAudioBus::kFloat32,
                        ShellAudioBus::kPlanar);
    const float kConversionMatrix[] = {
        0.f, 1.f, 0.f,   // dest channel 0 = source channel 1.
        1.f, 0.f, 0.f,   // dest channel 1 = source channel 0.
        1.f, 1.f, 1.f,   // dest channel 2 = sum of all source channels,
                         // effectively make it equal to source channel 2.
        -1.f, 0.f, 0.f,  // dest channel 3 = negative source channel 0,
                         // effectively make it equal to source channel 1.
        0.f, -1.f, 0.f,  // dest channel 4 = negative source channel 1,
                         // effectively make it equal to source channel 0.
        2.f, 1.f, -1.f,  // dest channel 5 = 2 * source channel 0 + source
                         // channel 2 - source channel 2, effectively make it
                         // equal to source channel 0.
    };
    std::vector<float> matrix(kConversionMatrix,
                              kConversionMatrix + arraysize(kConversionMatrix));
    other.Assign(*audio_bus_, matrix);

    EXPECT_TRUE(IsSameChannel(other, 0, *audio_bus_, 1));
    EXPECT_TRUE(IsSameChannel(other, 1, *audio_bus_, 0));
    EXPECT_TRUE(IsSameChannel(other, 2, *audio_bus_, 2));
    EXPECT_TRUE(IsSameChannel(other, 3, *audio_bus_, 1));
    EXPECT_TRUE(IsSameChannel(other, 4, *audio_bus_, 0));
    EXPECT_TRUE(IsSameChannel(other, 5, *audio_bus_, 0));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }
}

TEST_F(ShellAudioBusTest, MixWithoutMatrix) {
  CreateAudioBus<float>(ShellAudioBus::kInterleaved);
  // Fill the 1st channel with incrementally positive samples, fill the 2nd
  // channel with decrementally negative samples, and fill the 3rd channel with
  // 0.
  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t frame = 0; frame < kFrames; ++frame) {
      float sample = 0.f;
      if (channel == 0) {
        sample = static_cast<float>(frame) / static_cast<float>(kFrames);
      } else if (channel == 1) {
        sample = -static_cast<float>(frame) / static_cast<float>(kFrames);
      }
      guarded_buffers_->GetBuffer<float>(0)[frame * kChannels + channel] =
          sample;
    }
  }
  {
    ShellAudioBus other(kChannels, kFrames, ShellAudioBus::kFloat32,
                        ShellAudioBus::kPlanar);
    other.Assign(*audio_bus_);
    // By call Mix() here, we effectively multiplies every sample in |other| by
    // 2.
    other.Mix(*audio_bus_);
    // Adjust the original audio bus by multiplying every sample by 2.
    for (size_t channel = 0; channel < kChannels; ++channel) {
      for (size_t frame = 0; frame < kFrames; ++frame) {
        guarded_buffers_->GetBuffer<float>(0)[frame * kChannels + channel] *= 2;
      }
    }

    EXPECT_TRUE(IsSameChannel(other, 0, *audio_bus_, 0));
    EXPECT_TRUE(IsSameChannel(other, 1, *audio_bus_, 1));
    EXPECT_TRUE(IsSameChannel(other, 2, *audio_bus_, 2));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }
}

TEST_F(ShellAudioBusTest, MixWithMatrix) {
  CreateAudioBus<float>(ShellAudioBus::kInterleaved);
  // Fill the 1st channel with incrementally positive samples, fill the 2nd
  // channel with decrementally negative samples, and fill the 3rd channel with
  // 0.
  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t frame = 0; frame < kFrames; ++frame) {
      float sample = 0.f;
      if (channel == 0) {
        sample = static_cast<float>(frame) / static_cast<float>(kFrames);
      } else if (channel == 1) {
        sample = -static_cast<float>(frame) / static_cast<float>(kFrames);
      }
      guarded_buffers_->GetBuffer<float>(0)[frame * kChannels + channel] =
          sample;
    }
  }
  {
    ShellAudioBus other(kChannels * 2, kFrames, ShellAudioBus::kFloat32,
                        ShellAudioBus::kPlanar);
    const float kConversionMatrix[] = {
        0.f, 1.f, 0.f,   // dest channel 0 = source channel 1.
        1.f, 0.f, 0.f,   // dest channel 1 = source channel 0.
        1.f, 1.f, 1.f,   // dest channel 2 = sum of all source channels,
                         // effectively make it equal to source channel 2.
        -1.f, 0.f, 0.f,  // dest channel 3 = negative source channel 0,
                         // effectively make it equal to source channel 1.
        0.f, -1.f, 0.f,  // dest channel 4 = negative source channel 1,
                         // effectively make it equal to source channel 0.
        2.f, 1.f, -1.f,  // dest channel 5 = 2 * source channel 0 + source
                         // channel 2 - source channel 2, effectively make it
                         // equal to source channel 0.
    };
    std::vector<float> matrix(kConversionMatrix,
                              kConversionMatrix + arraysize(kConversionMatrix));
    other.Assign(*audio_bus_, matrix);
    // By call Mix() here, we effectively multiplies every sample in |other| by
    // 2.
    other.Mix(*audio_bus_, matrix);
    // Adjust the original audio bus by multiplying every sample by 2.
    for (size_t channel = 0; channel < kChannels; ++channel) {
      for (size_t frame = 0; frame < kFrames; ++frame) {
        guarded_buffers_->GetBuffer<float>(0)[frame * kChannels + channel] *= 2;
      }
    }

    EXPECT_TRUE(IsSameChannel(other, 0, *audio_bus_, 1));
    EXPECT_TRUE(IsSameChannel(other, 1, *audio_bus_, 0));
    EXPECT_TRUE(IsSameChannel(other, 2, *audio_bus_, 2));
    EXPECT_TRUE(IsSameChannel(other, 3, *audio_bus_, 1));
    EXPECT_TRUE(IsSameChannel(other, 4, *audio_bus_, 0));
    EXPECT_TRUE(IsSameChannel(other, 5, *audio_bus_, 0));
    EXPECT_TRUE(guarded_buffers_->VerifyGuardBytes());
  }
}

}  // namespace
}  // namespace testing
}  // namespace media
