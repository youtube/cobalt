// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_HOST_POOL_H_
#define NET_HTTP_HTTP_PIPELINED_HOST_POOL_H_

#include <map>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "net/http/http_pipelined_host.h"
#include "net/http/http_pipelined_host_capability.h"

namespace base {
class Value;
}

namespace net {

class HostPortPair;
class HttpPipelinedStream;
class HttpServerProperties;

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
        HttpPipelinedHost* host) = 0;
  };

  HttpPipelinedHostPool(Delegate* delegate,
                        HttpPipelinedHost::Factory* factory,
                        HttpServerProperties* http_server_properties_,
                        bool force_pipelining);
  virtual ~HttpPipelinedHostPool();

  // Returns true if pipelining might work for |key|. Generally, this returns
  // true, unless |key| is known to have failed pipelining recently.
  bool IsKeyEligibleForPipelining(const HttpPipelinedHost::Key& key);

  // Constructs a new pipeline on |connection| and returns a new
  // HttpPipelinedStream that uses it.
  HttpPipelinedStream* CreateStreamOnNewPipeline(
      const HttpPipelinedHost::Key& key,
      ClientSocketHandle* connection,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated,
      NextProto protocol_negotiated);

  // Tries to find an existing pipeline with capacity for a new request. If
  // successful, returns a new stream on that pipeline. Otherwise, returns NULL.
  HttpPipelinedStream* CreateStreamOnExistingPipeline(
      const HttpPipelinedHost::Key& key);

  // Returns true if a pipelined connection already exists for |key| and
  // can accept new requests.
  bool IsExistingPipelineAvailableForKey(const HttpPipelinedHost::Key& key);

  // Callbacks for HttpPipelinedHost.
  virtual void OnHostIdle(HttpPipelinedHost* host) override;

  virtual void OnHostHasAdditionalCapacity(HttpPipelinedHost* host) override;

  virtual void OnHostDeterminedCapability(
      HttpPipelinedHost* host,
      HttpPipelinedHostCapability capability) override;

  // Creates a Value summary of this pool's |host_map_|. Caller assumes
  // ownership of the returned Value.
  base::Value* PipelineInfoToValue() const;

 private:
#if defined(__LB_SHELL__) || defined(OS_STARBOARD)
  typedef std::map<HttpPipelinedHost::Key, HttpPipelinedHost*> HostMap;
#else
  typedef std::map<const HttpPipelinedHost::Key, HttpPipelinedHost*> HostMap;
#endif

  HttpPipelinedHost* GetPipelinedHost(const HttpPipelinedHost::Key& key,
                                      bool create_if_not_found);

  Delegate* delegate_;
  scoped_ptr<HttpPipelinedHost::Factory> factory_;
  HostMap host_map_;
  HttpServerProperties* http_server_properties_;
  bool force_pipelining_;

  DISALLOW_COPY_AND_ASSIGN(HttpPipelinedHostPool);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PIPELINED_HOST_POOL_H_
