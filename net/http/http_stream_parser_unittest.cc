// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_parser.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/base/upload_data.h"
#include "net/base/upload_data_stream.h"
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

TEST(HttpStreamParser, ShouldMerge_NoBody) {
  // Shouldn't be merged if upload data is non-existent.
  ASSERT_FALSE(HttpStreamParser::ShouldMerge("some header", NULL));
}

TEST(HttpStreamParser, ShouldMerge_EmptyBody) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  scoped_ptr<UploadDataStream> body(
      UploadDataStream::Create(upload_data.get(), NULL));
  // Shouldn't be merged if upload data is empty.
  ASSERT_FALSE(HttpStreamParser::ShouldMerge("some header", body.get()));
}

TEST(HttpStreamParser, ShouldMerge_ChunkedBody) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  upload_data->set_is_chunked(true);
  const std::string payload = "123";
  upload_data->AppendChunk(payload.data(), payload.size(), true);

  scoped_ptr<UploadDataStream> body(
      UploadDataStream::Create(upload_data.get(), NULL));
  // Shouldn't be merged if upload data carries chunked data.
  ASSERT_FALSE(HttpStreamParser::ShouldMerge("some header", body.get()));
}

TEST(HttpStreamParser, ShouldMerge_FileBody) {
  scoped_refptr<UploadData> upload_data = new UploadData;

  // Create an empty temporary file.
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir.path(),
                                                  &temp_file_path));

  upload_data->AppendFileRange(temp_file_path, 0, 0, base::Time());

  scoped_ptr<UploadDataStream> body(
      UploadDataStream::Create(upload_data.get(), NULL));
  // Shouldn't be merged if upload data carries a file, as it's not in-memory.
  ASSERT_FALSE(HttpStreamParser::ShouldMerge("some header", body.get()));
}

TEST(HttpStreamParser, ShouldMerge_SmallBodyInMemory) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  const std::string payload = "123";
  upload_data->AppendBytes(payload.data(), payload.size());

  scoped_ptr<UploadDataStream> body(
      UploadDataStream::Create(upload_data.get(), NULL));
  // Yes, should be merged if the in-memory body is small here.
  ASSERT_TRUE(HttpStreamParser::ShouldMerge("some header", body.get()));
}

TEST(HttpStreamParser, ShouldMerge_LargeBodyInMemory) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  const std::string payload(10000, 'a');  // 'a' x 10000.
  upload_data->AppendBytes(payload.data(), payload.size());

  scoped_ptr<UploadDataStream> body(
      UploadDataStream::Create(upload_data.get(), NULL));
  // Shouldn't be merged if the in-memory body is large here.
  ASSERT_FALSE(HttpStreamParser::ShouldMerge("some header", body.get()));
}

}  // namespace net
