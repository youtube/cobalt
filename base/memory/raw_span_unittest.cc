// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_span.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file contains tests related to raw_span, to show that they
// convert to span.

TEST(RawSpan, ConvertToSpan) {
  int arr[3] = {100, 101, 102};
  base::raw_span<int> span1(arr);
  base::span<int> span2(span1);
  base::span<int> span3;
  span3 = span1;

  EXPECT_THAT(span1, ::testing::ElementsAre(100, 101, 102));
  EXPECT_THAT(span2, ::testing::ElementsAre(100, 101, 102));
  EXPECT_THAT(span3, ::testing::ElementsAre(100, 101, 102));
}

TEST(RawSpan, ConvertFromSpan) {
  int arr[3] = {100, 101, 102};
  base::span<int> span1(arr);
  base::raw_span<int> span2(span1);
  base::raw_span<int> span3;
  span3 = span1;

  EXPECT_THAT(span1, ::testing::ElementsAre(100, 101, 102));
  EXPECT_THAT(span2, ::testing::ElementsAre(100, 101, 102));
  EXPECT_THAT(span3, ::testing::ElementsAre(100, 101, 102));
}

TEST(RawSpan, UnderstandsDanglingAttribute) {
  // Test passes if it doesn't trip Dangling Ptr Detectors.
  int* arr = new int[3];
  base::raw_span<int, DisableDanglingPtrDetection> span(arr, 3u);
  delete[] span.data();
}

TEST(RawSpan, ExtractAsDangling) {
  // Test passes if it doesn't trip Dangling Ptr Detectors.
  int* arr = new int[3];
  base::raw_span<int> span(arr, 3u);
  delete[] base::ExtractAsDanglingSpan(span).data();
}
