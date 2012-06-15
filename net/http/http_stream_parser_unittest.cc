// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_parser.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
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

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_NoBody) {
  // Shouldn't be merged if upload data is non-existent.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", NULL));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_EmptyBody) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  scoped_ptr<UploadDataStream> body(new UploadDataStream(upload_data));
  ASSERT_EQ(OK, body->Init());
  // Shouldn't be merged if upload data is empty.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_ChunkedBody) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  upload_data->set_is_chunked(true);
  const std::string payload = "123";
  upload_data->AppendChunk(payload.data(), payload.size(), true);

  scoped_ptr<UploadDataStream> body(new UploadDataStream(upload_data));
  ASSERT_EQ(OK, body->Init());
  // Shouldn't be merged if upload data carries chunked data.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_FileBody) {
  scoped_refptr<UploadData> upload_data = new UploadData;

  // Create an empty temporary file.
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir.path(),
                                                  &temp_file_path));

  upload_data->AppendFileRange(temp_file_path, 0, 0, base::Time());

  scoped_ptr<UploadDataStream> body(new UploadDataStream(upload_data));
  ASSERT_EQ(OK, body->Init());
  // Shouldn't be merged if upload data carries a file, as it's not in-memory.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_SmallBodyInMemory) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  const std::string payload = "123";
  upload_data->AppendBytes(payload.data(), payload.size());

  scoped_ptr<UploadDataStream> body(new UploadDataStream(upload_data));
  ASSERT_EQ(OK, body->Init());
  // Yes, should be merged if the in-memory body is small here.
  ASSERT_TRUE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_LargeBodyInMemory) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  const std::string payload(10000, 'a');  // 'a' x 10000.
  upload_data->AppendBytes(payload.data(), payload.size());

  scoped_ptr<UploadDataStream> body(new UploadDataStream(upload_data));
  ASSERT_EQ(OK, body->Init());
  // Shouldn't be merged if the in-memory body is large here.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

// Test to ensure the HttpStreamParser state machine does not get confused
// when sending a request with a chunked body, where chunks become available
// asynchronously, over a socket where writes may also complete
// asynchronously.
// This is a regression test for http://crbug.com/132243
TEST(HttpStreamParser, AsyncChunkAndAsyncSocket) {
  // The chunks that will be written in the request, as reflected in the
  // MockWrites below.
  static const char kChunk1[] = "Chunk 1";
  static const char kChunk2[] = "Chunky 2";
  static const char kChunk3[] = "Test 3";

  MockWrite writes[] = {
    MockWrite(ASYNC, 0,
              "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Transfer-Encoding: chunked\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(ASYNC, 1, "7\r\nChunk 1\r\n"),
    MockWrite(ASYNC, 2, "8\r\nChunky 2\r\n"),
    MockWrite(ASYNC, 3, "6\r\nTest 3\r\n"),
    MockWrite(ASYNC, 4, "0\r\n\r\n"),
  };

  // The size of the response body, as reflected in the Content-Length of the
  // MockRead below.
  static const int kBodySize = 8;

  MockRead reads[] = {
    MockRead(ASYNC, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(ASYNC, 7, "one.html"),
    MockRead(SYNCHRONOUS, 8, 0),  // EOF
  };

  scoped_refptr<DeterministicSocketData> data(
      new DeterministicSocketData(reads, arraysize(reads),
                                  writes, arraysize(writes)));
  data->set_connect_data(MockConnect(SYNCHRONOUS, OK));

  scoped_ptr<DeterministicMockTCPClientSocket> transport(
      new DeterministicMockTCPClientSocket(NULL, data));
  data->set_socket(transport->AsWeakPtr());

  TestCompletionCallback callback;
  int rv = transport->Connect(callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_EQ(OK, rv);

  scoped_ptr<ClientSocketHandle> socket_handle(new ClientSocketHandle);
  socket_handle->set_socket(transport.release());

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://localhost");
  request_info.load_flags = LOAD_NORMAL;

  scoped_refptr<GrowableIOBuffer> read_buffer(new GrowableIOBuffer);
  HttpStreamParser parser(socket_handle.get(), &request_info, read_buffer,
                          BoundNetLog());

  scoped_refptr<UploadData> upload_data(new UploadData);
  upload_data->set_is_chunked(true);

  upload_data->AppendChunk(kChunk1, arraysize(kChunk1) - 1, false);

  scoped_ptr<UploadDataStream> upload_stream(
      new UploadDataStream(upload_data));
  ASSERT_EQ(OK, upload_stream->Init());

  HttpRequestHeaders request_headers;
  request_headers.SetHeader("Host", "localhost");
  request_headers.SetHeader("Transfer-Encoding", "chunked");
  request_headers.SetHeader("Connection", "keep-alive");

  HttpResponseInfo response_info;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  rv = parser.SendRequest("GET /one.html HTTP/1.1\r\n", request_headers,
                          upload_stream.Pass(), &response_info,
                          callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);

  // Complete the initial request write. Additionally, this should enqueue the
  // first chunk.
  data->RunFor(1);
  ASSERT_FALSE(callback.have_result());

  // Now append another chunk (while the first write is still pending), which
  // should not confuse the state machine.
  upload_data->AppendChunk(kChunk2, arraysize(kChunk2) - 1, false);
  ASSERT_FALSE(callback.have_result());

  // Complete writing the first chunk, which should then enqueue the second
  // chunk for writing and return, because it is set to complete
  // asynchronously.
  data->RunFor(1);
  ASSERT_FALSE(callback.have_result());

  // Complete writing the second chunk. However, because no chunks are
  // available yet, no further writes should be called until a new chunk is
  // added.
  data->RunFor(1);
  ASSERT_FALSE(callback.have_result());

  // Add the final chunk. This will enqueue another write, but it will not
  // complete due to the async nature.
  upload_data->AppendChunk(kChunk3, arraysize(kChunk3) - 1, true);
  ASSERT_FALSE(callback.have_result());

  // Finalize writing the last chunk, which will enqueue the trailer.
  data->RunFor(1);
  ASSERT_FALSE(callback.have_result());

  // Finalize writing the trailer.
  data->RunFor(1);
  ASSERT_TRUE(callback.have_result());

  // Warning: This will hang if the callback doesn't already have a result,
  // due to the deterministic socket provider. Do not remove the above
  // ASSERT_TRUE, which will avoid this hang.
  rv = callback.WaitForResult();
  ASSERT_EQ(OK, rv);

  // Attempt to read the response status and the response headers.
  rv = parser.ReadResponseHeaders(callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  data->RunFor(2);

  ASSERT_TRUE(callback.have_result());
  rv = callback.WaitForResult();
  ASSERT_GT(rv, 0);

  // Finally, attempt to read the response body.
  scoped_refptr<IOBuffer> body_buffer(new IOBuffer(kBodySize));
  rv = parser.ReadResponseBody(body_buffer, kBodySize, callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  data->RunFor(1);

  ASSERT_TRUE(callback.have_result());
  rv = callback.WaitForResult();
  ASSERT_EQ(kBodySize, rv);
}

}  // namespace net
