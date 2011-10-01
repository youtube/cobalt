// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/memory/scoped_ptr.h"
#include "net/http/http_stream.h"

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class GrowableIOBuffer;
class HttpResponseInfo;
struct HttpRequestInfo;
class HttpRequestHeaders;
class HttpStreamParser;
class IOBuffer;
class UploadDataStream;

class HttpBasicStream : public HttpStream {
 public:
  // Constructs a new HttpBasicStream.  If |parser| is NULL, then
  // InitializeStream should be called to initialize it correctly.  If
  // |parser| is non-null, then InitializeStream should not be called,
  // as the stream is already initialized.
  HttpBasicStream(ClientSocketHandle* connection,
                  HttpStreamParser* parser,
                  bool using_proxy);
  virtual ~HttpBasicStream();

  // HttpStream methods:
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               const BoundNetLog& net_log,
                               OldCompletionCallback* callback) OVERRIDE;

  virtual int SendRequest(const HttpRequestHeaders& headers,
                          UploadDataStream* request_body,
                          HttpResponseInfo* response,
                          OldCompletionCallback* callback) OVERRIDE;

  virtual uint64 GetUploadProgress() const OVERRIDE;

  virtual int ReadResponseHeaders(OldCompletionCallback* callback) OVERRIDE;

  virtual const HttpResponseInfo* GetResponseInfo() const OVERRIDE;

  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               OldCompletionCallback* callback) OVERRIDE;

  virtual void Close(bool not_reusable) OVERRIDE;

  virtual HttpStream* RenewStreamForAuth() OVERRIDE;

  virtual bool IsResponseBodyComplete() const OVERRIDE;

  virtual bool CanFindEndOfResponse() const OVERRIDE;

  virtual bool IsMoreDataBuffered() const OVERRIDE;

  virtual bool IsConnectionReused() const OVERRIDE;

  virtual void SetConnectionReused() OVERRIDE;

  virtual bool IsConnectionReusable() const OVERRIDE;

  virtual void GetSSLInfo(SSLInfo* ssl_info) OVERRIDE;

  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) OVERRIDE;

  virtual bool IsSpdyHttpStream() const OVERRIDE;

  virtual void LogNumRttVsBytesMetrics() const OVERRIDE;

 private:
  scoped_refptr<GrowableIOBuffer> read_buf_;

  scoped_ptr<HttpStreamParser> parser_;

  scoped_ptr<ClientSocketHandle> connection_;

  bool using_proxy_;

  std::string request_line_;

  const HttpRequestInfo* request_info_;

  const HttpResponseInfo* response_;

  int64 bytes_read_offset_;

  DISALLOW_COPY_AND_ASSIGN(HttpBasicStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_BASIC_STREAM_H_
