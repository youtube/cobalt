// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_fifo.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioFifoTest : public testing::Test {
 public:
  AudioFifoTest() {}
  ~AudioFifoTest() {}

  void VerifyValue(const float data[], int size, float value) {
    for (int i = 0; i < size; ++i)
      ASSERT_FLOAT_EQ(value, data[i]) << "i=" << i;
  }

 protected:
  DISALLOW_COPY_AND_ASSIGN(AudioFifoTest);
};

// Verify that construction works as intended.
TEST_F(AudioFifoTest, Construct) {
  static const int kChannels = 6;
  static const int kMaxFrameCount = 128;
  AudioFifo fifo(kChannels, kMaxFrameCount);
  EXPECT_EQ(fifo.frames_in_fifo(), 0);
}

// Pushes audio bus objects to a FIFO and fill it up to different degrees.
// Also, verify that it is not possible to overflow the FIFO.
TEST_F(AudioFifoTest, Push) {
  static const int kChannels = 2;
  static const int kMaxFrameCount = 128;
  AudioFifo fifo(kChannels, kMaxFrameCount);
  {
    SCOPED_TRACE("Push 50%");
    scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount / 2);
    EXPECT_EQ(fifo.frames_in_fifo(), 0);
    EXPECT_TRUE(fifo.Push(bus.get()));
    EXPECT_EQ(fifo.frames_in_fifo(), bus->frames());
    fifo.Clear();
  }
  {
    SCOPED_TRACE("Push 100%");
    scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount);
    EXPECT_EQ(fifo.frames_in_fifo(), 0);
    EXPECT_TRUE(fifo.Push(bus.get()));
    EXPECT_EQ(fifo.frames_in_fifo(), bus->frames());
    fifo.Clear();
  }
  {
    SCOPED_TRACE("Overflow");
    scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount + 1);
    EXPECT_EQ(fifo.frames_in_fifo(), 0);
    EXPECT_FALSE(fifo.Push(bus.get()));
    EXPECT_EQ(fifo.frames_in_fifo(), 0);
  }
}

// Consumes audio bus objects from a FIFO and empty it to different degrees.
// Also, verify that it is not possible to ask for more data than the FIFO
// contains (corresponds to underrun).
TEST_F(AudioFifoTest, Consume) {
  static const int kChannels = 2;
  static const int kMaxFrameCount = 128;
  AudioFifo fifo(kChannels, kMaxFrameCount);
  {
    scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount);
    EXPECT_TRUE(fifo.Push(bus.get()));
    EXPECT_EQ(fifo.frames_in_fifo(), kMaxFrameCount);
  }
  {
    SCOPED_TRACE("Consume 50%");
    scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount / 2);
    EXPECT_TRUE(fifo.Consume(bus.get(), bus->frames()));
    EXPECT_TRUE(fifo.frames_in_fifo() == bus->frames());
    EXPECT_TRUE(fifo.Push(bus.get()));
    EXPECT_EQ(fifo.frames_in_fifo(), kMaxFrameCount);
  }
  {
    SCOPED_TRACE("Consume 100%");
    scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount);
    EXPECT_TRUE(fifo.Consume(bus.get(), bus->frames()));
    EXPECT_EQ(fifo.frames_in_fifo(), 0);
    EXPECT_TRUE(fifo.Push(bus.get()));
    EXPECT_EQ(fifo.frames_in_fifo(), kMaxFrameCount);
  }
  {
    SCOPED_TRACE("Underrun");
    scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount + 1);
    EXPECT_FALSE(fifo.Consume(bus.get(), bus->frames()));
    EXPECT_EQ(fifo.frames_in_fifo(), kMaxFrameCount);
  }
}

// Verify that the frames_in_fifo() method of the FIFO works as intended while
// appending and removing audio bus elements to/from the FIFO.
TEST_F(AudioFifoTest, FramesInFifo) {
  static const int kChannels = 2;
  static const int kMaxFrameCount = 64;
  AudioFifo fifo(kChannels, kMaxFrameCount);

  // Fill up the FIFO and verify that the size grows as it should while adding
  // one audio frame each time.
  scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, 1);
  int n = 0;
  while (fifo.frames_in_fifo() < kMaxFrameCount) {
    EXPECT_TRUE(fifo.Push(bus.get()));
    EXPECT_EQ(fifo.frames_in_fifo(), ++n);
  }

  // Ensure that we can't append more data when the FIFO is full.
  EXPECT_EQ(fifo.frames_in_fifo(), kMaxFrameCount);
  EXPECT_FALSE(fifo.Push(bus.get()));

  // Empty the FIFO and verify that the size decreases as it should.
  // Reduce the size of the FIFO by one frame each time.
  while (fifo.frames_in_fifo() > 0) {
    EXPECT_TRUE(fifo.Consume(bus.get(), bus->frames()));
    EXPECT_EQ(fifo.frames_in_fifo(), --n);
  }

  // Ensure that we can't remove more data when the FIFO is empty.
  EXPECT_EQ(fifo.frames_in_fifo(), 0);
  EXPECT_FALSE(fifo.Consume(bus.get(), bus->frames()));

  // Verify that a steady-state size of #frames in the FIFO is maintained
  // during a sequence of Push/Consume calls which involves wrapping. We ensure
  // wrapping by selecting a buffer size which does divides the FIFO size
  // with a remainder of one.
  scoped_ptr<AudioBus> bus2 =
      AudioBus::Create(kChannels, (kMaxFrameCount / 4) - 1);
  const int frames_in_fifo = bus2->frames();
  EXPECT_TRUE(fifo.Push(bus2.get()));
  EXPECT_EQ(fifo.frames_in_fifo(), frames_in_fifo);
  for (int n = 0; n < kMaxFrameCount; ++n) {
    EXPECT_TRUE(fifo.Push(bus2.get()));
    EXPECT_TRUE(fifo.Consume(bus2.get(), frames_in_fifo));
    EXPECT_EQ(fifo.frames_in_fifo(), frames_in_fifo);
  }
}

// Perform a sequence of Push/Consume calls and verify that the data written
// to the FIFO is correctly retrieved, i.e., that the order is correct and the
// values are correct.
TEST_F(AudioFifoTest, VerifyDataValues) {
  static const int kChannels = 2;
  static const int kFrameCount = 2;
  static const int kFifoFrameCount = 5 * kFrameCount;

  AudioFifo fifo(kChannels, kFifoFrameCount);
  scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);
  EXPECT_EQ(fifo.frames_in_fifo(), 0);
  EXPECT_EQ(bus->frames(), kFrameCount);

  // Start by filling up the FIFO with audio frames. The first audio frame
  // will contain all 1's, the second all 2's etc. All channels contain the
  // same value.
  int value = 1;
  while (fifo.frames_in_fifo() < kFifoFrameCount) {
    for (int j = 0; j < bus->channels(); ++j)
      std::fill(bus->channel(j), bus->channel(j) + bus->frames(), value);
    EXPECT_TRUE(fifo.Push(bus.get()));
    EXPECT_EQ(fifo.frames_in_fifo(), bus->frames() * value);
    ++value;
  }

  // FIFO should be full now.
  EXPECT_EQ(fifo.frames_in_fifo(), kFifoFrameCount);

  // Consume all audio frames in the FIFO and verify that the stored values
  // are correct. In this example, we shall read out: 1, 2, 3, 4, 5 in that
  // order. Note that we set |frames_to_consume| to half the size of the bus.
  // It means that we shall read out the same value two times in row.
  value = 1;
  int n = 1;
  const int frames_to_consume = bus->frames() / 2;
  while (fifo.frames_in_fifo() > 0) {
    EXPECT_TRUE(fifo.Consume(bus.get(), frames_to_consume));
    for (int j = 0; j < bus->channels(); ++j)
      VerifyValue(bus->channel(j), frames_to_consume, value);
    if (n++ % 2 == 0)
      ++value;  // counts 1, 1, 2, 2, 3, 3,...
  }

  // FIFO should be empty now.
  EXPECT_EQ(fifo.frames_in_fifo(), 0);

  // Push one audio bus to the FIFO and fill it with 1's.
  value = 1;
  for (int j = 0; j < bus->channels(); ++j)
    std::fill(bus->channel(j), bus->channel(j) + bus->frames(), value);
  EXPECT_TRUE(fifo.Push(bus.get()));
  EXPECT_EQ(fifo.frames_in_fifo(), bus->frames());

  // Keep calling Consume/Push a few rounds and verify that we read out the
  // correct values. The number of elements shall be fixed (kFrameCount) during
  // this phase.
  for (int i = 0; i < 5 * kFifoFrameCount; i++) {
    EXPECT_TRUE(fifo.Consume(bus.get(), bus->frames()));
    for (int j = 0; j < bus->channels(); ++j) {
      VerifyValue(bus->channel(j), bus->channels(), value);
      std::fill(bus->channel(j), bus->channel(j) + bus->frames(), value + 1);
    }
    EXPECT_TRUE(fifo.Push(bus.get()));
    EXPECT_EQ(fifo.frames_in_fifo(), bus->frames());
    ++value;
  }
}

}  // namespace media
