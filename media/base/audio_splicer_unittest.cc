// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/base/audio_splicer.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/buffers.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kBytesPerFrame = 4;
static const int kDefaultSampleRate = 44100;
static const int kDefaultBufferSize = 100 * kBytesPerFrame;

class AudioSplicerTest : public ::testing::Test {
 public:
  AudioSplicerTest()
      : splicer_(kBytesPerFrame, kDefaultSampleRate),
        input_timestamp_helper_(kBytesPerFrame, kDefaultSampleRate) {
    input_timestamp_helper_.SetBaseTimestamp(base::TimeDelta());
  }

  scoped_refptr<Buffer> GetNextInputBuffer(uint8 value) {
    return GetNextInputBuffer(value, kDefaultBufferSize);
  }

  scoped_refptr<Buffer> GetNextInputBuffer(uint8 value, int size) {
    scoped_refptr<DataBuffer> buffer = new DataBuffer(size);
    buffer->SetDataSize(size);
    memset(buffer->GetWritableData(), value, buffer->GetDataSize());
    buffer->SetTimestamp(input_timestamp_helper_.GetTimestamp());
    buffer->SetDuration(
        input_timestamp_helper_.GetDuration(buffer->GetDataSize()));
    input_timestamp_helper_.AddBytes(buffer->GetDataSize());
    return buffer;
  }

  bool VerifyData(const uint8* data, int size, int value) {
    for (int i = 0; i < size; ++i) {
      if (data[i] != value)
        return false;
    }
    return true;
  }

 protected:
  AudioSplicer splicer_;
  AudioTimestampHelper input_timestamp_helper_;

  DISALLOW_COPY_AND_ASSIGN(AudioSplicerTest);
};

TEST_F(AudioSplicerTest, PassThru) {
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Test single buffer pass-thru behavior.
  scoped_refptr<Buffer> input_1 = GetNextInputBuffer(1);
  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  scoped_refptr<Buffer> output_1 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_1->GetTimestamp(), output_1->GetTimestamp());
  EXPECT_EQ(input_1->GetDuration(), output_1->GetDuration());
  EXPECT_EQ(input_1->GetDataSize(), output_1->GetDataSize());

  // Test that multiple buffers can be queued in the splicer.
  scoped_refptr<Buffer> input_2 = GetNextInputBuffer(2);
  scoped_refptr<Buffer> input_3 = GetNextInputBuffer(3);
  EXPECT_TRUE(splicer_.AddInput(input_2));
  EXPECT_TRUE(splicer_.AddInput(input_3));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  scoped_refptr<Buffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_TRUE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_2->GetTimestamp(), output_2->GetTimestamp());
  EXPECT_EQ(input_2->GetDuration(), output_2->GetDuration());
  EXPECT_EQ(input_2->GetDataSize(), output_2->GetDataSize());

  scoped_refptr<Buffer> output_3 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_3->GetTimestamp(), output_3->GetTimestamp());
  EXPECT_EQ(input_3->GetDuration(), output_3->GetDuration());
  EXPECT_EQ(input_3->GetDataSize(), output_3->GetDataSize());
}

TEST_F(AudioSplicerTest, Reset) {
  scoped_refptr<Buffer> input_1 = GetNextInputBuffer(1);
  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  splicer_.Reset();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Add some bytes to the timestamp helper so that the
  // next buffer starts many frames beyond the end of
  // |input_1|. This is to make sure that Reset() actually
  // clears its state and doesn't try to insert a gap.
  input_timestamp_helper_.AddBytes(100 * kBytesPerFrame);

  // Verify that a new input buffer passes through as expected.
  scoped_refptr<Buffer> input_2 = GetNextInputBuffer(2);
  EXPECT_TRUE(splicer_.AddInput(input_2));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  scoped_refptr<Buffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_2->GetTimestamp(), output_2->GetTimestamp());
  EXPECT_EQ(input_2->GetDuration(), output_2->GetDuration());
  EXPECT_EQ(input_2->GetDataSize(), output_2->GetDataSize());
}

TEST_F(AudioSplicerTest, EndOfStream) {
  scoped_refptr<Buffer> input_1 = GetNextInputBuffer(1);
  scoped_refptr<Buffer> input_2 = new DataBuffer(0);  // End of stream.
  scoped_refptr<Buffer> input_3 = GetNextInputBuffer(2);
  EXPECT_TRUE(input_2->IsEndOfStream());

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.AddInput(input_2));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  scoped_refptr<Buffer> output_1 = splicer_.GetNextBuffer();
  scoped_refptr<Buffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_1->GetTimestamp(), output_1->GetTimestamp());
  EXPECT_EQ(input_1->GetDuration(), output_1->GetDuration());
  EXPECT_EQ(input_1->GetDataSize(), output_1->GetDataSize());

  EXPECT_TRUE(output_2->IsEndOfStream());

  // Verify that buffers can be added again after Reset().
  splicer_.Reset();
  EXPECT_TRUE(splicer_.AddInput(input_3));
  scoped_refptr<Buffer> output_3 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_3->GetTimestamp(), output_3->GetTimestamp());
  EXPECT_EQ(input_3->GetDuration(), output_3->GetDuration());
  EXPECT_EQ(input_3->GetDataSize(), output_3->GetDataSize());
}


// Test the gap insertion code.
// +--------------+    +--------------+
// |11111111111111|    |22222222222222|
// +--------------+    +--------------+
// Results in:
// +--------------+----+--------------+
// |11111111111111|0000|22222222222222|
// +--------------+----+--------------+
TEST_F(AudioSplicerTest, GapInsertion) {
  scoped_refptr<Buffer> input_1 = GetNextInputBuffer(1);

  // Add bytes to the timestamp helper so that the next buffer
  // will have a starting timestamp that indicates a gap is
  // present.
  const int kGapSize = 7 * kBytesPerFrame;
  input_timestamp_helper_.AddBytes(kGapSize);
  scoped_refptr<Buffer> input_2 = GetNextInputBuffer(2);

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.AddInput(input_2));

  // Verify that a gap buffer is generated.
  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<Buffer> output_1 = splicer_.GetNextBuffer();
  scoped_refptr<Buffer> output_2 = splicer_.GetNextBuffer();
  scoped_refptr<Buffer> output_3 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Verify that the first input buffer passed through unmodified.
  EXPECT_EQ(input_1->GetTimestamp(), output_1->GetTimestamp());
  EXPECT_EQ(input_1->GetDuration(), output_1->GetDuration());
  EXPECT_EQ(input_1->GetDataSize(), output_1->GetDataSize());
  EXPECT_TRUE(VerifyData(output_1->GetData(), output_1->GetDataSize(), 1));

  // Verify the contents of the gap buffer.
  base::TimeDelta gap_timestamp =
      input_1->GetTimestamp() + input_1->GetDuration();
  base::TimeDelta gap_duration = input_2->GetTimestamp() - gap_timestamp;
  EXPECT_GT(gap_duration, base::TimeDelta());
  EXPECT_EQ(gap_timestamp, output_2->GetTimestamp());
  EXPECT_EQ(gap_duration, output_2->GetDuration());
  EXPECT_EQ(kGapSize, output_2->GetDataSize());
  EXPECT_TRUE(VerifyData(output_2->GetData(), output_2->GetDataSize(), 0));

  // Verify that the second input buffer passed through unmodified.
  EXPECT_EQ(input_2->GetTimestamp(), output_3->GetTimestamp());
  EXPECT_EQ(input_2->GetDuration(), output_3->GetDuration());
  EXPECT_EQ(input_2->GetDataSize(), output_3->GetDataSize());
  EXPECT_TRUE(VerifyData(output_3->GetData(), output_3->GetDataSize(), 2));
}


// Test that an error is signalled when the gap between input buffers is
// too large.
TEST_F(AudioSplicerTest, GapTooLarge) {
  scoped_refptr<Buffer> input_1 = GetNextInputBuffer(1);

  // Add a seconds worth of bytes so that an unacceptably large
  // gap exists between |input_1| and |input_2|.
  const int kGapSize = kDefaultSampleRate * kBytesPerFrame;
  input_timestamp_helper_.AddBytes(kGapSize);
  scoped_refptr<Buffer> input_2 = GetNextInputBuffer(2);

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_FALSE(splicer_.AddInput(input_2));

  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<Buffer> output_1 = splicer_.GetNextBuffer();

  // Verify that the first input buffer passed through unmodified.
  EXPECT_EQ(input_1->GetTimestamp(), output_1->GetTimestamp());
  EXPECT_EQ(input_1->GetDuration(), output_1->GetDuration());
  EXPECT_EQ(input_1->GetDataSize(), output_1->GetDataSize());
  EXPECT_TRUE(VerifyData(output_1->GetData(), output_1->GetDataSize(), 1));

  // Verify that the second buffer is not available.
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Reset the timestamp helper so it can generate a buffer that is
  // right after |input_1|.
  input_timestamp_helper_.SetBaseTimestamp(
      input_1->GetTimestamp() + input_1->GetDuration());

  // Verify that valid buffers are still accepted.
  scoped_refptr<Buffer> input_3 = GetNextInputBuffer(3);
  EXPECT_TRUE(splicer_.AddInput(input_3));
  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<Buffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_3->GetTimestamp(), output_2->GetTimestamp());
  EXPECT_EQ(input_3->GetDuration(), output_2->GetDuration());
  EXPECT_EQ(input_3->GetDataSize(), output_2->GetDataSize());
  EXPECT_TRUE(VerifyData(output_2->GetData(), output_2->GetDataSize(), 3));
}


// Verifies that an error is signalled if AddInput() is called
// with a timestamp that is earlier than the first buffer added.
TEST_F(AudioSplicerTest, BufferAddedBeforeBase) {
  input_timestamp_helper_.SetBaseTimestamp(
      base::TimeDelta::FromMicroseconds(10));
  scoped_refptr<Buffer> input_1 = GetNextInputBuffer(1);

  // Reset the timestamp helper so the next buffer will have a timestamp earlier
  // than |input_1|.
  input_timestamp_helper_.SetBaseTimestamp(base::TimeDelta::FromSeconds(0));
  scoped_refptr<Buffer> input_2 = GetNextInputBuffer(1);

  EXPECT_GT(input_1->GetTimestamp(), input_2->GetTimestamp());
  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_FALSE(splicer_.AddInput(input_2));
}


// Test when one buffer partially overlaps another.
// +--------------+
// |11111111111111|
// +--------------+
//            +--------------+
//            |22222222222222|
//            +--------------+
// Results in:
// +--------------+----------+
// |11111111111111|2222222222|
// +--------------+----------+
TEST_F(AudioSplicerTest, PartialOverlap) {
  scoped_refptr<Buffer> input_1 = GetNextInputBuffer(1);

  // Reset timestamp helper so that the next buffer will have a
  // timestamp that starts in the middle of |input_1|.
  const int kOverlapSize = input_1->GetDataSize() / 4;
  input_timestamp_helper_.SetBaseTimestamp(input_1->GetTimestamp());
  input_timestamp_helper_.AddBytes(input_1->GetDataSize() - kOverlapSize);

  scoped_refptr<Buffer> input_2 = GetNextInputBuffer(2);

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.AddInput(input_2));

  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<Buffer> output_1 = splicer_.GetNextBuffer();
  scoped_refptr<Buffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Verify that the first input buffer passed through unmodified.
  EXPECT_EQ(input_1->GetTimestamp(), output_1->GetTimestamp());
  EXPECT_EQ(input_1->GetDuration(), output_1->GetDuration());
  EXPECT_EQ(input_1->GetDataSize(), output_1->GetDataSize());
  EXPECT_TRUE(VerifyData(output_1->GetData(), output_1->GetDataSize(), 1));


  // Verify that the second input buffer was truncated to only contain
  // the samples that are after the end of |input_1|.
  base::TimeDelta expected_timestamp =
      input_1->GetTimestamp() + input_1->GetDuration();
  base::TimeDelta expected_duration =
      (input_2->GetTimestamp() + input_2->GetDuration()) - expected_timestamp;
  EXPECT_EQ(expected_timestamp, output_2->GetTimestamp());
  EXPECT_EQ(expected_duration, output_2->GetDuration());
  EXPECT_EQ(input_2->GetDataSize() - kOverlapSize, output_2->GetDataSize());
  EXPECT_TRUE(VerifyData(output_2->GetData(), output_2->GetDataSize(), 2));
}


// Test that an input buffer that is completely overlapped by a buffer
// that was already added is dropped.
// +--------------+
// |11111111111111|
// +--------------+
//       +-----+
//       |22222|
//       +-----+
//                +-------------+
//                |3333333333333|
//                +-------------+
// Results in:
// +--------------+-------------+
// |11111111111111|3333333333333|
// +--------------+-------------+
TEST_F(AudioSplicerTest, DropBuffer) {
  scoped_refptr<Buffer> input_1 = GetNextInputBuffer(1);

  // Reset timestamp helper so that the next buffer will have a
  // timestamp that starts in the middle of |input_1|.
  const int kOverlapOffset = input_1->GetDataSize() / 2;
  const int kOverlapSize = input_1->GetDataSize() / 4;
  input_timestamp_helper_.SetBaseTimestamp(input_1->GetTimestamp());
  input_timestamp_helper_.AddBytes(kOverlapOffset);

  scoped_refptr<Buffer> input_2 = GetNextInputBuffer(2, kOverlapSize);

  // Reset the timestamp helper so the next buffer will be right after
  // |input_1|.
  input_timestamp_helper_.SetBaseTimestamp(input_1->GetTimestamp());
  input_timestamp_helper_.AddBytes(input_1->GetDataSize());
  scoped_refptr<Buffer> input_3 = GetNextInputBuffer(3);

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.AddInput(input_2));
  EXPECT_TRUE(splicer_.AddInput(input_3));

  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<Buffer> output_1 = splicer_.GetNextBuffer();
  scoped_refptr<Buffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Verify that the first input buffer passed through unmodified.
  EXPECT_EQ(input_1->GetTimestamp(), output_1->GetTimestamp());
  EXPECT_EQ(input_1->GetDuration(), output_1->GetDuration());
  EXPECT_EQ(input_1->GetDataSize(), output_1->GetDataSize());
  EXPECT_TRUE(VerifyData(output_1->GetData(), output_1->GetDataSize(), 1));

  // Verify that the second output buffer only contains
  // the samples that are in |input_3|.
  EXPECT_EQ(input_3->GetTimestamp(), output_2->GetTimestamp());
  EXPECT_EQ(input_3->GetDuration(), output_2->GetDuration());
  EXPECT_EQ(input_3->GetDataSize(), output_2->GetDataSize());
  EXPECT_TRUE(VerifyData(output_2->GetData(), output_2->GetDataSize(), 3));
}

}  // namespace media
