// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_HOST_H_
#define NET_HTTP_HTTP_PIPELINED_HOST_H_

#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_pipelined_connection.h"
#include "net/http/http_pipelined_host_capability.h"

namespace base {
class Value;
}

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class HostPortPair;
class HttpPipelinedStream;
class ProxyInfo;
struct SSLConfig;

// Manages all of the pipelining state for specific host with active pipelined
// HTTP requests. Manages connection jobs, constructs pipelined streams, and
// assigns requests to the least loaded pipelined connection.
class NET_EXPORT_PRIVATE HttpPipelinedHost {
 public:
  class NET_EXPORT_PRIVATE Key {
   public:
    Key(const HostPortPair& origin);

    // The host and port associated with this key.
    const HostPortPair& origin() const { return origin_; }

    bool operator<(const Key& rhs) const;

   private:
    const HostPortPair origin_;
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
    virtual void OnHostDeterminedCapability(
        HttpPipelinedHost* host,
        HttpPipelinedHostCapability capability) = 0;
  };

  class Factory {
   public:
    virtual ~Factory() {}

    // Returns a new HttpPipelinedHost.
    virtual HttpPipelinedHost* CreateNewHost(
        Delegate* delegate, const Key& key,
        HttpPipelinedConnection::Factory* factory,
        HttpPipelinedHostCapability capability,
        bool force_pipelining) = 0;
  };

  virtual ~HttpPipelinedHost() {}

  // Constructs a new pipeline on |connection| and returns a new
  // HttpPipelinedStream that uses it.
  virtual HttpPipelinedStream* CreateStreamOnNewPipeline(
      ClientSocketHandle* connection,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated,
      NextProto protocol_negotiated) = 0;

  // Tries to find an existing pipeline with capacity for a new request. If
  // successful, returns a new stream on that pipeline. Otherwise, returns NULL.
  virtual HttpPipelinedStream* CreateStreamOnExistingPipeline() = 0;

  // Returns true if we have a pipelined connection that can accept new
  // requests.
  virtual bool IsExistingPipelineAvailable() const = 0;

  // Returns a Key that uniquely identifies this host.
  virtual const Key& GetKey() const = 0;

  // Creates a Value summary of this host's pipelines. Caller assumes
  // ownership of the returned Value.
  virtual base::Value* PipelineInfoToValue() const = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PIPELINED_HOST_H_
