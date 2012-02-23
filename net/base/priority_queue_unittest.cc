// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/priority_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

typedef PriorityQueue<int>::Priority Priority;
const Priority kPriorities[] = { 2, 1, 2, 0, 4, 3, 1, 4, 0 };
const Priority kNumPriorities = 5;  // max(kPriorities) + 1
const size_t kNumElements = arraysize(kPriorities);
const int kFirstMinOrder[kNumElements] = { 3, 8, 1, 6, 0, 2, 5, 4, 7 };
const int kLastMaxOrder[kNumElements] = { 7, 4, 5, 2, 0, 6, 1, 8, 3 };
const int kFirstMaxOrder[kNumElements] = { 4, 7, 5, 0, 2, 1, 6, 3, 8 };
const int kLastMinOrder[kNumElements] = { 8, 3, 6, 1, 2, 0, 5, 7, 4 };

void CheckEmpty(PriorityQueue<int>* queue) {
  EXPECT_EQ(0u, queue->size());
  EXPECT_TRUE(queue->FirstMin().is_null());
  EXPECT_TRUE(queue->LastMin().is_null());
  EXPECT_TRUE(queue->FirstMax().is_null());
  EXPECT_TRUE(queue->LastMax().is_null());
}

TEST(PriorityQueueTest, AddAndClear) {
  PriorityQueue<int> queue(kNumPriorities);
  PriorityQueue<int>::Pointer pointers[kNumElements];

  CheckEmpty(&queue);
  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(i, queue.size());
    pointers[i] = queue.Insert(static_cast<int>(i), kPriorities[i]);
  }
  EXPECT_EQ(kNumElements, queue.size());

  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kPriorities[i], pointers[i].priority());
    EXPECT_EQ(static_cast<int>(i), pointers[i].value());
  }

  queue.Clear();
  CheckEmpty(&queue);
}

TEST(PriorityQueueTest, FirstMinOrder) {
  PriorityQueue<int> queue(kNumPriorities);
  PriorityQueue<int>::Pointer pointers[kNumElements];

  for (size_t i = 0; i < kNumElements; ++i) {
    pointers[i] = queue.Insert(static_cast<int>(i), kPriorities[i]);
  }

  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kNumElements - i, queue.size());
    // Also check Equals.
    EXPECT_TRUE(queue.FirstMin().Equals(pointers[kFirstMinOrder[i]]));
    EXPECT_EQ(kFirstMinOrder[i], queue.FirstMin().value());
    queue.Erase(queue.FirstMin());
  }
  CheckEmpty(&queue);
}

TEST(PriorityQueueTest, LastMinOrder) {
  PriorityQueue<int> queue(kNumPriorities);

  for (size_t i = 0; i < kNumElements; ++i) {
    queue.Insert(static_cast<int>(i), kPriorities[i]);
  }

  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kLastMinOrder[i], queue.LastMin().value());
    queue.Erase(queue.LastMin());
  }
  CheckEmpty(&queue);
}

TEST(PriorityQueueTest, FirstMaxOrder) {
  PriorityQueue<int> queue(kNumPriorities);

  for (size_t i = 0; i < kNumElements; ++i) {
    queue.Insert(static_cast<int>(i), kPriorities[i]);
  }

  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kFirstMaxOrder[i], queue.FirstMax().value());
    queue.Erase(queue.FirstMax());
  }
  CheckEmpty(&queue);
}

TEST(PriorityQueueTest, LastMaxOrder) {
  PriorityQueue<int> queue(kNumPriorities);

  for (size_t i = 0; i < kNumElements; ++i) {
    queue.Insert(static_cast<int>(i), kPriorities[i]);
  }

  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kLastMaxOrder[i], queue.LastMax().value());
    queue.Erase(queue.LastMax());
  }
  CheckEmpty(&queue);
}

TEST(PriorityQueueTest, EraseFromMiddle) {
  PriorityQueue<int> queue(kNumPriorities);
  PriorityQueue<int>::Pointer pointers[kNumElements];

  for (size_t i = 0; i < kNumElements; ++i) {
    pointers[i] = queue.Insert(static_cast<int>(i), kPriorities[i]);
  }

  queue.Erase(pointers[2]);
  queue.Erase(pointers[3]);

  int expected_order[] = { 8, 1, 6, 0, 5, 4, 7 };

  for (size_t i = 0; i < arraysize(expected_order); ++i) {
    EXPECT_EQ(expected_order[i], queue.FirstMin().value());
    queue.Erase(queue.FirstMin());
  }
  CheckEmpty(&queue);
}

}  // namespace

}  // namespace net

