// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/byte_buffer_queue.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

using testing::ElementsAre;

TEST(ByteBufferQueueTest, DefaultConstructor) {
  ByteBufferQueue buffer_queue;
  EXPECT_EQ(0u, buffer_queue.size());
  EXPECT_TRUE(buffer_queue.empty());
}

TEST(ByteBufferQueueTest, AppendEmpty) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Append({});
  EXPECT_TRUE(buffer_queue.empty());
}

TEST(ByteBufferQueueTest, AppendOneSegment) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Append({1, 2, 3});
  EXPECT_EQ(3u, buffer_queue.size());
}

TEST(ByteBufferQueueTest, AppendTwoSegments) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Append({1, 2, 3});
  buffer_queue.Append({4, 5});
  EXPECT_EQ(5u, buffer_queue.size());
}

TEST(ByteBufferQueueTest, ReadIntoEmpty) {
  ByteBufferQueue buffer_queue;
  Vector<uint8_t> data(100);
  EXPECT_EQ(0u, buffer_queue.ReadInto(base::make_span(data)));
}

TEST(ByteBufferQueueTest, ReadIntoLessThanOneSegment) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Append({1, 2, 3});
  Vector<uint8_t> data(2);
  EXPECT_EQ(2u, buffer_queue.ReadInto(base::make_span(data)));
  EXPECT_EQ(1u, buffer_queue.size());
  EXPECT_THAT(data, ElementsAre(1, 2));
}

TEST(ByteBufferQueueTest, ReadIntoExactOneSegmentSize) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Append({1, 2, 3});
  Vector<uint8_t> data(3);
  EXPECT_EQ(3u, buffer_queue.ReadInto(base::make_span(data)));
  EXPECT_EQ(0u, buffer_queue.size());
  EXPECT_THAT(data, ElementsAre(1, 2, 3));
}

TEST(ByteBufferQueueTest, ReadIntoOverOneSegmentSize) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Append({1, 2, 3});
  Vector<uint8_t> data(5);
  EXPECT_EQ(3u, buffer_queue.ReadInto(base::make_span(data)));
  EXPECT_EQ(0u, buffer_queue.size());
  EXPECT_THAT(data, ElementsAre(1, 2, 3, 0, 0));
}

TEST(ByteBufferQueueTest, ReadIntoEmptyData) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Append({1, 2, 3});
  Vector<uint8_t> data;
  EXPECT_EQ(0u, buffer_queue.ReadInto(base::make_span(data)));
  EXPECT_EQ(3u, buffer_queue.size());
}

TEST(ByteBufferQueueTest, ReadIntoExactlyTwoSegments) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Append({1, 2, 3});
  buffer_queue.Append({4, 5});
  Vector<uint8_t> data(5);
  EXPECT_EQ(5u, buffer_queue.ReadInto(base::make_span(data)));
  EXPECT_EQ(0u, buffer_queue.size());
  EXPECT_THAT(data, ElementsAre(1, 2, 3, 4, 5));
}

TEST(ByteBufferQueueTest, ReadIntoAcrossTwoSegmentsMisaligned) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Append({1, 2, 3});
  buffer_queue.Append({4, 5});

  Vector<uint8_t> data(2);
  EXPECT_EQ(2u, buffer_queue.ReadInto(base::make_span(data)));
  EXPECT_THAT(data, ElementsAre(1, 2));

  EXPECT_EQ(2u, buffer_queue.ReadInto(base::make_span(data)));
  EXPECT_THAT(data, ElementsAre(3, 4));

  EXPECT_EQ(1u, buffer_queue.ReadInto(base::make_span(data)));
  EXPECT_THAT(data, ElementsAre(5, 4));
}

TEST(ByteBufferQueueTest, ClearEmptyBuffer) {
  ByteBufferQueue buffer_queue;
  buffer_queue.Clear();
  EXPECT_EQ(0u, buffer_queue.size());
  EXPECT_TRUE(buffer_queue.empty());
}

TEST(ByteBufferQueueTest, ReadIntoAfterClearThenAppend) {
  ByteBufferQueue buffer_queue;

  buffer_queue.Append({1, 2, 3});
  Vector<uint8_t> data(2);
  buffer_queue.ReadInto(base::make_span(data));

  buffer_queue.Clear();
  EXPECT_EQ(0u, buffer_queue.size());
  EXPECT_EQ(0u, buffer_queue.ReadInto(base::make_span(data)));

  buffer_queue.Append({4, 5});
  EXPECT_EQ(2u, buffer_queue.ReadInto(base::make_span(data)));
  EXPECT_THAT(data, ElementsAre(4, 5));
}

}  // namespace blink
