// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_PARSER_H_
#define NET_HTTP_HTTP_STREAM_PARSER_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/io_buffer.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket_handle.h"

namespace net {

class ClientSocketHandle;
class HttpRequestInfo;

class HttpStreamParser {
 public:
  // Any data in |read_buffer| will be used before reading from the socket
  // and any data left over after parsing the stream will be put into
  // |read_buffer|.  The left over data will start at offset 0 and the
  // buffer's offset will be set to the first free byte. |read_buffer| may
  // have its capacity changed.
  HttpStreamParser(ClientSocketHandle* connection,
                   GrowableIOBuffer* read_buffer);
  ~HttpStreamParser() {}

  // These functions implement the interface described in HttpStream with
  // some additional functionality
  int SendRequest(const HttpRequestInfo* request, const std::string& headers,
                  UploadDataStream* request_body, CompletionCallback* callback);

  int ReadResponseHeaders(CompletionCallback* callback);

  int ReadResponseBody(IOBuffer* buf, int buf_len,
                       CompletionCallback* callback);

  uint64 GetUploadProgress() const;

  HttpResponseInfo* GetResponseInfo();

  bool IsResponseBodyComplete() const;

  bool CanFindEndOfResponse() const;

  bool IsMoreDataBuffered() const;

 private:
  // FOO_COMPLETE states implement the second half of potentially asynchronous
  // operations and don't necessarily mean that FOO is complete.
  enum State {
    STATE_NONE,
    STATE_SENDING_HEADERS,
    STATE_SENDING_BODY,
    STATE_REQUEST_SENT,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_BODY_PENDING,
    STATE_READ_BODY,
    STATE_READ_BODY_COMPLETE,
    STATE_DONE
  };

  // The number of bytes by which the header buffer is grown when it reaches
  // capacity.
  enum { kHeaderBufInitialSize = 4096 };

  // |kMaxHeaderBufSize| is the number of bytes that the response headers can
  // grow to. If the body start is not found within this range of the
  // response, the transaction will fail with ERR_RESPONSE_HEADERS_TOO_BIG.
  // Note: |kMaxHeaderBufSize| should be a multiple of |kHeaderBufInitialSize|.
  enum { kMaxHeaderBufSize = 256 * 1024 };  // 256 kilobytes.

  // Handle callbacks.
  void OnIOComplete(int result);

  // Try to make progress sending/receiving the request/response.
  int DoLoop(int result);

  // The implementations of each state of the state machine.
  int DoSendHeaders(int result);
  int DoSendBody(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoReadBody();
  int DoReadBodyComplete(int result);

  // Examines |read_buf_| to find the start and end of the headers. Return
  // the offset for the end of the headers, or -1 if the complete headers
  // were not found. If they are are found, parse them with
  // DoParseResponseHeaders().
  int ParseResponseHeaders();

  // Parse the headers into response_.
  void DoParseResponseHeaders(int end_of_header_offset);

  // Examine the parsed headers to try to determine the response body size.
  void CalculateResponseBodySize();

  // Current state of the request.
  State io_state_;

  // The request to send.
  const HttpRequestInfo* request_;

  // The request header data.
  scoped_refptr<DrainableIOBuffer> request_headers_;

  // The request body data.
  scoped_ptr<UploadDataStream> request_body_;

  // Temporary buffer for reading.
  scoped_refptr<GrowableIOBuffer> read_buf_;

  // Offset of the first unused byte in |read_buf_|.  May be nonzero due to
  // a 1xx header, or body data in the same packet as header data.
  int read_buf_unused_offset_;

  // The amount beyond |read_buf_unused_offset_| where the status line starts;
  // -1 if not found yet.
  int response_header_start_offset_;

  // The parsed response headers.
  HttpResponseInfo response_;

  // Indicates the content length.  If this value is less than zero
  // (and chunked_decoder_ is null), then we must read until the server
  // closes the connection.
  int64 response_body_length_;

  // Keep track of the number of response body bytes read so far.
  int64 response_body_read_;

  // Helper if the data is chunked.
  scoped_ptr<HttpChunkedDecoder> chunked_decoder_;

  // Where the caller wants the body data.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // The callback to notify a user that their request or response is
  // complete or there was an error
  CompletionCallback* user_callback_;

  // In the client callback, the client can do anything, including
  // destroying this class, so any pending callback must be issued
  // after everything else is done.  When it is time to issue the client
  // callback, move it from |user_callback_| to |scheduled_callback_|.
  CompletionCallback* scheduled_callback_;

  // The underlying socket.
  ClientSocketHandle* const connection_;

  // Callback to be used when doing IO.
  CompletionCallbackImpl<HttpStreamParser> io_callback_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamParser);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_PARSER_H_
