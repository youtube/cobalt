// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include "testing/platform_test.h"

namespace net {

namespace test {

TEST(SpdyHttpUtilsTest, ConvertRequestPriorityToSpdy2Priority) {
  EXPECT_EQ(0, ConvertRequestPriorityToSpdyPriority(HIGHEST, 2));
  EXPECT_EQ(1, ConvertRequestPriorityToSpdyPriority(MEDIUM, 2));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOW, 2));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOWEST, 2));
  EXPECT_EQ(3, ConvertRequestPriorityToSpdyPriority(IDLE, 2));
}
TEST(SpdyHttpUtilsTest, ConvertRequestPriorityToSpdy3Priority) {
  EXPECT_EQ(0, ConvertRequestPriorityToSpdyPriority(HIGHEST, 3));
  EXPECT_EQ(1, ConvertRequestPriorityToSpdyPriority(MEDIUM, 3));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOW, 3));
  EXPECT_EQ(3, ConvertRequestPriorityToSpdyPriority(LOWEST, 3));
  EXPECT_EQ(4, ConvertRequestPriorityToSpdyPriority(IDLE, 3));
}

}  // namespace test

}  // namespace net
