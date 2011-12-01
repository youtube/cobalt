// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_CONNECTION_H_
#define NET_HTTP_HTTP_PIPELINED_CONNECTION_H_
#pragma once

#include "net/base/net_export.h"
#include "net/base/net_log.h"

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class HttpPipelinedStream;
class ProxyInfo;
struct SSLConfig;

class NET_EXPORT_PRIVATE HttpPipelinedConnection {
 public:
  enum Feedback {
    OK,
    PIPELINE_SOCKET_ERROR,
    OLD_HTTP_VERSION,
    MUST_CLOSE_CONNECTION,
  };

  class Delegate {
   public:
    // Called when a pipeline has newly available capacity. This may be because
    // the first request has been sent and the pipeline is now active. Or, it
    // may be because a request successfully completed.
    virtual void OnPipelineHasCapacity(HttpPipelinedConnection* pipeline) = 0;

    // Called every time a pipeline receives headers. Lets the delegate know if
    // the headers indicate that pipelining can be used.
    virtual void OnPipelineFeedback(HttpPipelinedConnection* pipeline,
                                    Feedback feedback) = 0;
  };

  class Factory {
   public:
    virtual ~Factory() {}

    virtual HttpPipelinedConnection* CreateNewPipeline(
        ClientSocketHandle* connection,
        Delegate* delegate,
        const SSLConfig& used_ssl_config,
        const ProxyInfo& used_proxy_info,
        const BoundNetLog& net_log,
        bool was_npn_negotiated) = 0;
  };

  virtual ~HttpPipelinedConnection() {}

  // Returns a new stream that uses this pipeline.
  virtual HttpPipelinedStream* CreateNewStream() = 0;

  // The number of streams currently associated with this pipeline.
  virtual int depth() const = 0;

  // True if this pipeline can accept new HTTP requests. False if a fatal error
  // has occurred.
  virtual bool usable() const = 0;

  // True if this pipeline has bound one request and is ready for additional
  // requests.
  virtual bool active() const = 0;

  // The SSLConfig used to establish this connection.
  virtual const SSLConfig& used_ssl_config() const = 0;

  // The ProxyInfo used to establish this connection.
  virtual const ProxyInfo& used_proxy_info() const = 0;

  // The source of this pipelined connection.
  virtual const NetLog::Source& source() const = 0;

  // True if this connection was NPN negotiated.
  virtual bool was_npn_negotiated() const = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PIPELINED_CONNECTION_H_
