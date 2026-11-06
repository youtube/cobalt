// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_stream_map.h"

#include <optional>

#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_export.h"

namespace moqt {

namespace test {

class QUICHE_EXPORT StreamMapTest : public quic::test::QuicTest {
 public:
  StreamMapTest() {}
};

TEST_F(StreamMapTest, AddQueryRemoveStreamIdSubgroup) {
  SendStreamMap stream_map;
  stream_map.AddStream(DataStreamIndex{4, 0}, 2);
  EXPECT_EQ(stream_map.GetStreamFor(DataStreamIndex(5, 0)), std::nullopt);
  stream_map.AddStream(DataStreamIndex{5, 0}, 6);
  stream_map.AddStream(DataStreamIndex{5, 1}, 7);
  EXPECT_QUICHE_BUG(stream_map.AddStream(DataStreamIndex{5, 0}, 6),
                    "Stream already added");
  EXPECT_EQ(stream_map.GetStreamFor(DataStreamIndex(4, 0)), 2);
  stream_map.RemoveStream(DataStreamIndex{5, 1});
  EXPECT_EQ(stream_map.GetStreamFor(DataStreamIndex(5, 1)), std::nullopt);
}

}  // namespace test

}  // namespace moqt
