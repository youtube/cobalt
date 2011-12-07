// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_HOST_POOL_H_
#define NET_HTTP_HTTP_PIPELINED_HOST_POOL_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/mru_cache.h"
#include "base/memory/scoped_ptr.h"
#include "net/http/http_pipelined_host.h"

namespace net {

class HostPortPair;
class HttpPipelinedStream;

// Manages all of the pipelining state for specific host with active pipelined
// HTTP requests. Manages connection jobs, constructs pipelined streams, and
// assigns requests to the least loaded pipelined connection.
class NET_EXPORT_PRIVATE HttpPipelinedHostPool
    : public HttpPipelinedHost::Delegate {
 public:
  class Delegate {
   public:
    // Called when a HttpPipelinedHost has new capacity. Attempts to allocate
    // any pending pipeline-capable requests to pipelines.
    virtual void OnHttpPipelinedHostHasAdditionalCapacity(
        const HostPortPair& origin) = 0;
  };

  HttpPipelinedHostPool(Delegate* delegate,
                        HttpPipelinedHost::Factory* factory);
  virtual ~HttpPipelinedHostPool();

  // Returns true if pipelining might work for |origin|. Generally, this returns
  // true, unless |origin| is known to have failed pipelining recently.
  bool IsHostEligibleForPipelining(const HostPortPair& origin);

  // Constructs a new pipeline on |connection| and returns a new
  // HttpPipelinedStream that uses it.
  HttpPipelinedStream* CreateStreamOnNewPipeline(
      const HostPortPair& origin,
      ClientSocketHandle* connection,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated);

  // Tries to find an existing pipeline with capacity for a new request. If
  // successful, returns a new stream on that pipeline. Otherwise, returns NULL.
  HttpPipelinedStream* CreateStreamOnExistingPipeline(
      const HostPortPair& origin);

  // Returns true if a pipelined connection already exists for this origin and
  // can accept new requests.
  bool IsExistingPipelineAvailableForOrigin(const HostPortPair& origin);

  // Callbacks for HttpPipelinedHost.
  virtual void OnHostIdle(HttpPipelinedHost* host) OVERRIDE;

  virtual void OnHostHasAdditionalCapacity(HttpPipelinedHost* host) OVERRIDE;

  virtual void OnHostDeterminedCapability(
      HttpPipelinedHost* host,
      HttpPipelinedHost::Capability capability) OVERRIDE;

 private:
  typedef base::MRUCache<HostPortPair,
                         HttpPipelinedHost::Capability> CapabilityMap;
  typedef std::map<const HostPortPair, HttpPipelinedHost*> HostMap;

  HttpPipelinedHost* GetPipelinedHost(const HostPortPair& origin,
                                      bool create_if_not_found);

  HttpPipelinedHost::Capability GetHostCapability(const HostPortPair& origin);

  Delegate* delegate_;
  scoped_ptr<HttpPipelinedHost::Factory> factory_;
  HostMap host_map_;
  CapabilityMap known_capability_map_;

  DISALLOW_COPY_AND_ASSIGN(HttpPipelinedHostPool);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PIPELINED_HOST_POOL_H_
