// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
                               const CompletionCallback& callback) override;

  virtual int SendRequest(const HttpRequestHeaders& headers,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) override;

  virtual UploadProgress GetUploadProgress() const override;

  virtual int ReadResponseHeaders(const CompletionCallback& callback) override;

  virtual const HttpResponseInfo* GetResponseInfo() const override;

  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) override;

  virtual void Close(bool not_reusable) override;

  virtual HttpStream* RenewStreamForAuth() override;

  virtual bool IsResponseBodyComplete() const override;

  virtual bool CanFindEndOfResponse() const override;

  virtual bool IsMoreDataBuffered() const override;

  virtual bool IsConnectionReused() const override;

  virtual void SetConnectionReused() override;

  virtual bool IsConnectionReusable() const override;

  virtual void GetSSLInfo(SSLInfo* ssl_info) override;

  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) override;

  virtual bool IsSpdyHttpStream() const override;

  virtual void LogNumRttVsBytesMetrics() const override;

  virtual void Drain(HttpNetworkSession* session) override;

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
