// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpBasicStream is a simple implementation of HttpStream.  It assumes it is
// not sharing a sharing with any other HttpStreams, therefore it just reads and
// writes directly to the Http Stream.

#ifndef NET_HTTP_HTTP_BASIC_STREAM_H_
#define NET_HTTP_HTTP_BASIC_STREAM_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/io_buffer.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_parser.h"

namespace net {

class ClientSocketHandle;
class HttpRequestInfo;
class HttpResponseInfo;
class UploadDataStream;

class HttpBasicStream : public HttpStream {
 public:
  explicit HttpBasicStream(ClientSocketHandle* handle);
  virtual ~HttpBasicStream() {}

  // HttpStream methods:
  virtual int SendRequest(const HttpRequestInfo* request,
                          const std::string& headers,
                          UploadDataStream* request_body,
                          CompletionCallback* callback);

  virtual uint64 GetUploadProgress() const;

  virtual int ReadResponseHeaders(CompletionCallback* callback);

  virtual HttpResponseInfo* GetResponseInfo() const;

  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               CompletionCallback* callback);

  virtual bool IsResponseBodyComplete() const;

  virtual bool CanFindEndOfResponse() const;

  virtual bool IsMoreDataBuffered() const;

 private:
  scoped_refptr<GrowableIOBuffer> read_buf_;

  scoped_ptr<HttpStreamParser> parser_;

  DISALLOW_COPY_AND_ASSIGN(HttpBasicStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_BASIC_STREAM_H_
