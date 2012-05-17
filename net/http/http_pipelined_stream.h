// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_STREAM_H_
#define NET_HTTP_HTTP_PIPELINED_STREAM_H_
#pragma once

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
class UploadDataStream;

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
                               const CompletionCallback& callback) OVERRIDE;

  virtual int SendRequest(const HttpRequestHeaders& headers,
                          UploadDataStream* request_body,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) OVERRIDE;

  virtual uint64 GetUploadProgress() const OVERRIDE;

  virtual int ReadResponseHeaders(const CompletionCallback& callback) OVERRIDE;

  virtual const HttpResponseInfo* GetResponseInfo() const OVERRIDE;

  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) OVERRIDE;

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

  virtual void Drain(HttpNetworkSession* session) OVERRIDE;

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
