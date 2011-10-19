// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_HOST_H_
#define NET_HTTP_HTTP_PIPELINED_HOST_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_pipelined_connection.h"

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class HttpPipelinedStream;
class ProxyInfo;
struct SSLConfig;

// Manages all of the pipelining state for specific host with active pipelined
// HTTP requests. Manages connection jobs, constructs pipelined streams, and
// assigns requests to the least loaded pipelined connection.
class NET_EXPORT_PRIVATE HttpPipelinedHost
    : public HttpPipelinedConnection::Delegate {
 public:
  class Delegate {
   public:
    // Called when a pipelined host has no outstanding requests on any of its
    // pipelined connections.
    virtual void OnHostIdle(HttpPipelinedHost* host) = 0;

    // Called when a pipelined host has newly available pipeline capacity, like
    // when a request completes.
    virtual void OnHostHasAdditionalCapacity(HttpPipelinedHost* host) = 0;
  };

  HttpPipelinedHost(Delegate* delegate, const HostPortPair& origin,
                    HttpPipelinedConnection::Factory* factory);
  virtual ~HttpPipelinedHost();

  // Constructs a new pipeline on |connection| and returns a new
  // HttpPipelinedStream that uses it.
  HttpPipelinedStream* CreateStreamOnNewPipeline(
      ClientSocketHandle* connection,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated);

  // Tries to find an existing pipeline with capacity for a new request. If
  // successful, returns a new stream on that pipeline. Otherwise, returns NULL.
  HttpPipelinedStream* CreateStreamOnExistingPipeline();

  // Returns true if we have a pipelined connection that can accept new
  // requests.
  bool IsExistingPipelineAvailable();

  // Callbacks for HttpPipelinedConnection.

  // Called when a pipelined connection completes a request. Adds a pending
  // request to the pipeline if the pipeline is still usable.
  virtual void OnPipelineHasCapacity(
      HttpPipelinedConnection* pipeline) OVERRIDE;

  const HostPortPair& origin() const { return origin_; }

 private:
  // Called when a pipeline is empty and there are no pending requests. Closes
  // the connection.
  void OnPipelineEmpty(HttpPipelinedConnection* pipeline);

  // Adds the next pending request to the pipeline if it's still usuable.
  void AddRequestToPipeline(HttpPipelinedConnection* connection);

  int max_pipeline_depth() const { return 3; }

  Delegate* delegate_;
  const HostPortPair origin_;
  std::set<HttpPipelinedConnection*> pipelines_;
  scoped_ptr<HttpPipelinedConnection::Factory> factory_;

  DISALLOW_COPY_AND_ASSIGN(HttpPipelinedHost);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PIPELINED_HOST_H_
