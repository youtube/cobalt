// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_HOST_POOL_H_
#define NET_HTTP_HTTP_PIPELINED_HOST_POOL_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "net/http/http_pipelined_host.h"

namespace net {

class HostPortPair;
class HttpPipelinedStream;
class HttpStreamFactoryImpl;

// Manages all of the pipelining state for specific host with active pipelined
// HTTP requests. Manages connection jobs, constructs pipelined streams, and
// assigns requests to the least loaded pipelined connection.
class HttpPipelinedHostPool : public HttpPipelinedHost::Delegate {
 public:
  explicit HttpPipelinedHostPool(HttpStreamFactoryImpl* factory);
  ~HttpPipelinedHostPool();

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

 private:
  typedef std::map<const HostPortPair, HttpPipelinedHost*> HostMap;

  HttpPipelinedHost* GetPipelinedHost(const HostPortPair& origin,
                                      bool create_if_not_found);

  HttpStreamFactoryImpl* factory_;
  HostMap host_map_;

  DISALLOW_COPY_AND_ASSIGN(HttpPipelinedHostPool);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PIPELINED_HOST_POOL_H_
