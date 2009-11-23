// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpStream is an interface for reading and writing data to an HttpStream that
// keeps the client agnostic of the actual underlying transport layer.  This
// provides an abstraction for both a basic http stream as well as http
// pipelining implementations.

#ifndef NET_HTTP_HTTP_STREAM_H_
#define NET_HTTP_HTTP_STREAM_H_

#include <string>

#include "base/basictypes.h"
#include "net/socket/client_socket_handle.h"

namespace net {

class HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
class UploadDataStream;

class HttpStream {
 public:
  HttpStream() {}
  virtual ~HttpStream() {}

  // Writes the headers and uploads body data to the underlying socket.
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the result will be passed to the callback
  // when available. Returns OK on success. The HttpStream takes ownership
  // of the request_body.
  virtual int SendRequest(const HttpRequestInfo* request,
                          const std::string& request_headers,
                          UploadDataStream* request_body,
                          CompletionCallback* callback) = 0;

  // Queries the UploadDataStream for its progress (bytes sent).
  virtual uint64 GetUploadProgress() const = 0;

  // Reads from the underlying socket until the response headers have been
  // completely received. ERR_IO_PENDING is returned if the operation could
  // not be completed synchronously, in which case the result will be passed
  // to the callback when available. Returns OK on success.  The response
  // headers are available in the HttpResponseInfo returned by GetResponseInfo
  virtual int ReadResponseHeaders(CompletionCallback* callback) = 0;

  // Provides access to HttpResponseInfo (owned by HttpStream).
  virtual HttpResponseInfo* GetResponseInfo() const = 0;

  // Reads response body data, up to |buf_len| bytes. |buf_len| should be a
  // reasonable size (<256KB). The number of bytes read is returned, or an
  // error is returned upon failure.  ERR_CONNECTION_CLOSED is returned to
  // indicate end-of-connection.  ERR_IO_PENDING is returned if the operation
  // could not be completed synchronously, in which case the result will be
  // passed to the callback when available. If the operation is not completed
  // immediately, the socket acquires a reference to the provided buffer until
  // the callback is invoked or the socket is destroyed.
  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               CompletionCallback* callback) = 0;

  // Indicates if the response body has been completely read.
  virtual bool IsResponseBodyComplete() const = 0;

  // Indicates that the end of the response is detectable. This means that
  // the response headers indicate either chunked encoding or content length.
  // If neither is sent, the server must close the connection for us to detect
  // the end of the response.
  virtual bool CanFindEndOfResponse() const = 0;

  // After the response headers have been read and after the response body
  // is complete, this function indicates if more data (either erroneous or
  // as part of the next pipelined response) has been read from the socket.
  virtual bool IsMoreDataBuffered() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_H_
