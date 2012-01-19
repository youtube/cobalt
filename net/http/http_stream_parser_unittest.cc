// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_parser.h"

#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

const size_t kOutputSize = 1024;  // Just large enough for this test.
// The number of bytes that can fit in a buffer of kOutputSize.
const size_t kMaxPayloadSize =
    kOutputSize - HttpStreamParser::kChunkHeaderFooterSize;

// The empty payload is how the last chunk is encoded.
TEST(HttpStreamParser, EncodeChunk_EmptyPayload) {
  char output[kOutputSize];

  const base::StringPiece kPayload = "";
  const base::StringPiece kExpected = "0\r\n\r\n";
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, base::StringPiece(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_ShortPayload) {
  char output[kOutputSize];

  const std::string kPayload("foo\x00\x11\x22", 6);
  // 11 = payload size + sizeof("6") + CRLF x 2.
  const std::string kExpected("6\r\nfoo\x00\x11\x22\r\n", 11);
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, base::StringPiece(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_LargePayload) {
  char output[kOutputSize];

  const std::string kPayload(1000, '\xff');  // '\xff' x 1000.
  // 3E8 = 1000 in hex.
  const std::string kExpected = "3E8\r\n" + kPayload + "\r\n";
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, base::StringPiece(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_FullPayload) {
  char output[kOutputSize];

  const std::string kPayload(kMaxPayloadSize, '\xff');
  // 3F4 = 1012 in hex.
  const std::string kExpected = "3F4\r\n" + kPayload + "\r\n";
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, base::StringPiece(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_TooLargePayload) {
  char output[kOutputSize];

  // The payload is one byte larger the output buffer size.
  const std::string kPayload(kMaxPayloadSize + 1, '\xff');
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(ERR_INVALID_ARGUMENT, num_bytes_written);
}

}  // namespace net
