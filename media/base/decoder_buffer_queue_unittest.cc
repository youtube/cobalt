// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"
#include "media/base/decoder_buffer_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static base::TimeDelta ToTimeDelta(int seconds) {
  if (seconds < 0)
    return kNoTimestamp();
  return base::TimeDelta::FromSeconds(seconds);
}

// Helper to create buffers with specified timestamp in seconds.
//
// Negative numbers will be converted to kNoTimestamp();
static scoped_refptr<DecoderBuffer> CreateBuffer(int timestamp) {
  scoped_refptr<DecoderBuffer> buffer = new DecoderBuffer(0);
  buffer->SetTimestamp(ToTimeDelta(timestamp));
  buffer->SetDuration(ToTimeDelta(0));
  return buffer;
}

TEST(DecoderBufferQueueTest, IsEmpty) {
  DecoderBufferQueue queue;
  EXPECT_TRUE(queue.IsEmpty());

  queue.Push(CreateBuffer(0));
  EXPECT_FALSE(queue.IsEmpty());
}

TEST(DecoderBufferQueueTest, Clear) {
  DecoderBufferQueue queue;
  queue.Push(CreateBuffer(0));
  queue.Push(CreateBuffer(1));
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(1, queue.Duration().InSeconds());

  queue.Clear();
  EXPECT_TRUE(queue.IsEmpty());
  EXPECT_EQ(0, queue.Duration().InSeconds());
}

TEST(DecoderBufferQueueTest, Duration) {
  DecoderBufferQueue queue;
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(0));
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(1));
  EXPECT_EQ(1, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(2));
  EXPECT_EQ(2, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(4));
  EXPECT_EQ(4, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(3, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(2, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());
}

TEST(DecoderBufferQueueTest, Duration_OutOfOrder) {
  DecoderBufferQueue queue;
  queue.Push(CreateBuffer(10));
  queue.Push(CreateBuffer(12));
  EXPECT_EQ(2, queue.Duration().InSeconds());

  // Out of order: duration shouldn't change.
  queue.Push(CreateBuffer(8));
  EXPECT_EQ(2, queue.Duration().InSeconds());

  // Removing first buffer should leave the second buffer as the only buffer
  // included in the duration calculation.
  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  // Removing second buffer leaves the out-of-order buffer. It shouldn't be
  // included in duration calculations.
  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  // Push a still-too-early buffer. It shouldn't be included in duration
  // calculations.
  queue.Push(CreateBuffer(11));
  EXPECT_EQ(0, queue.Duration().InSeconds());

  // Push a buffer that's after the earliest valid time. It's a singular valid
  // buffer so duration is still zero.
  queue.Push(CreateBuffer(14));
  EXPECT_EQ(0, queue.Duration().InSeconds());

  // Push a second valid buffer. We should now have a duration.
  queue.Push(CreateBuffer(17));
  EXPECT_EQ(3, queue.Duration().InSeconds());
}

TEST(DecoderBufferQueueTest, Duration_NoTimestamp) {
  // Buffers with no timestamp don't affect duration.
  DecoderBufferQueue queue;
  queue.Push(CreateBuffer(0));
  queue.Push(CreateBuffer(4));
  EXPECT_EQ(4, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(-1));
  EXPECT_EQ(4, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(6));
  EXPECT_EQ(6, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(2, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());
}

}  // namespace media
