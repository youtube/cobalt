// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpBasicStream is a simple implementation of HttpStream.  It assumes it is
// not sharing a sharing with any other HttpStreams, therefore it just reads and
// writes directly to the Http Stream.

#ifndef NET_HTTP_HTTP_BASIC_STREAM_H_
#define NET_HTTP_HTTP_BASIC_STREAM_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/http/http_stream.h"

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class GrowableIOBuffer;
class HttpResponseInfo;
struct HttpRequestInfo;
class HttpStreamParser;
class IOBuffer;
class UploadDataStream;

class HttpBasicStream : public HttpStream {
 public:
  explicit HttpBasicStream(ClientSocketHandle* connection);
  virtual ~HttpBasicStream();

  // HttpStream methods:
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               const BoundNetLog& net_log,
                               CompletionCallback* callback);

  virtual int SendRequest(const std::string& headers,
                          UploadDataStream* request_body,
                          HttpResponseInfo* response,
                          CompletionCallback* callback);

  virtual uint64 GetUploadProgress() const;

  virtual int ReadResponseHeaders(CompletionCallback* callback);

  virtual const HttpResponseInfo* GetResponseInfo() const;

  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               CompletionCallback* callback);

  virtual void Close(bool not_reusable);

  virtual bool IsResponseBodyComplete() const;

  virtual bool CanFindEndOfResponse() const;

  virtual bool IsMoreDataBuffered() const;

  virtual bool IsConnectionReused() const;

  virtual void SetConnectionReused();

  virtual void GetSSLInfo(SSLInfo* ssl_info);

  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);

 private:
  scoped_refptr<GrowableIOBuffer> read_buf_;

  scoped_ptr<HttpStreamParser> parser_;

  scoped_ptr<ClientSocketHandle> connection_;

  DISALLOW_COPY_AND_ASSIGN(HttpBasicStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_BASIC_STREAM_H_
