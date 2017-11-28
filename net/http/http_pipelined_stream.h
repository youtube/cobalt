// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_STREAM_H_
#define NET_HTTP_HTTP_PIPELINED_STREAM_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/net_log.h"
#include "net/http/http_stream.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class BoundNetLog;
class HttpPipelinedConnectionImpl;
class HttpResponseInfo;
class HttpRequestHeaders;
struct HttpRequestInfo;
class IOBuffer;
class ProxyInfo;
struct SSLConfig;

// HttpPipelinedStream is the pipelined implementation of HttpStream. It has
// very little code in it. Instead, it serves as the client's interface to the
// pipelined connection, where all the work happens.
//
// In the case of pipelining failures, these functions may return
// ERR_PIPELINE_EVICTION. In that case, the client should retry the HTTP
// request without pipelining.
class HttpPipelinedStream : public HttpStream {
 public:
  HttpPipelinedStream(HttpPipelinedConnectionImpl* pipeline,
                      int pipeline_id);
  virtual ~HttpPipelinedStream();

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

  // The SSLConfig used to establish this stream's pipeline.
  const SSLConfig& used_ssl_config() const;

  // The ProxyInfo used to establish this this stream's pipeline.
  const ProxyInfo& used_proxy_info() const;

  // The BoundNetLog of this stream's pipelined connection.
  const BoundNetLog& net_log() const;

  // True if this stream's pipeline was NPN negotiated.
  bool was_npn_negotiated() const;

  // Protocol negotiated with the server.
  NextProto protocol_negotiated() const;

 private:
  HttpPipelinedConnectionImpl* pipeline_;

  int pipeline_id_;

  const HttpRequestInfo* request_info_;

  DISALLOW_COPY_AND_ASSIGN(HttpPipelinedStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PIPELINED_STREAM_H_
