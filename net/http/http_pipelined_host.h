// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_HOST_H_
#define NET_HTTP_HTTP_PIPELINED_HOST_H_
#pragma once

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
class NET_EXPORT_PRIVATE HttpPipelinedHost {
 public:
  enum Capability {
    UNKNOWN,
    INCAPABLE,
    CAPABLE,
    PROBABLY_CAPABLE,  // We are using pipelining, but haven't processed enough
                       // requests to record this host as known to be capable.
  };

  class Delegate {
   public:
    // Called when a pipelined host has no outstanding requests on any of its
    // pipelined connections.
    virtual void OnHostIdle(HttpPipelinedHost* host) = 0;

    // Called when a pipelined host has newly available pipeline capacity, like
    // when a request completes.
    virtual void OnHostHasAdditionalCapacity(HttpPipelinedHost* host) = 0;

    // Called when a host determines if pipelining can be used.
    virtual void OnHostDeterminedCapability(HttpPipelinedHost* host,
                                            Capability capability) = 0;
  };

  class Factory {
   public:
    virtual ~Factory() {}

    // Returns a new HttpPipelinedHost.
    virtual HttpPipelinedHost* CreateNewHost(
        Delegate* delegate, const HostPortPair& origin,
        HttpPipelinedConnection::Factory* factory,
        Capability capability) = 0;
  };

  virtual ~HttpPipelinedHost() {}

  // Constructs a new pipeline on |connection| and returns a new
  // HttpPipelinedStream that uses it.
  virtual HttpPipelinedStream* CreateStreamOnNewPipeline(
      ClientSocketHandle* connection,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated) = 0;

  // Tries to find an existing pipeline with capacity for a new request. If
  // successful, returns a new stream on that pipeline. Otherwise, returns NULL.
  virtual HttpPipelinedStream* CreateStreamOnExistingPipeline() = 0;

  // Returns true if we have a pipelined connection that can accept new
  // requests.
  virtual bool IsExistingPipelineAvailable() const = 0;

  // Returns the host and port associated with this class.
  virtual const HostPortPair& origin() const = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PIPELINED_HOST_H_
