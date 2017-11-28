// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PIPELINED_HOST_IMPL_H_
#define NET_HTTP_HTTP_PIPELINED_HOST_IMPL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_pipelined_connection.h"
#include "net/http/http_pipelined_host.h"
#include "net/http/http_pipelined_host_capability.h"

namespace base {
class Value;
}

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class HttpPipelinedStream;
class ProxyInfo;
struct SSLConfig;

// Manages all of the pipelining state for specific host with active pipelined
// HTTP requests. Manages connection jobs, constructs pipelined streams, and
// assigns requests to the least loaded pipelined connection.
class NET_EXPORT_PRIVATE HttpPipelinedHostImpl
    : public HttpPipelinedHost,
      public HttpPipelinedConnection::Delegate {
 public:
  HttpPipelinedHostImpl(HttpPipelinedHost::Delegate* delegate,
                        const HttpPipelinedHost::Key& key,
                        HttpPipelinedConnection::Factory* factory,
                        HttpPipelinedHostCapability capability);
  virtual ~HttpPipelinedHostImpl();

  // HttpPipelinedHost interface
  virtual HttpPipelinedStream* CreateStreamOnNewPipeline(
      ClientSocketHandle* connection,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated,
      NextProto protocol_negotiated) override;

  virtual HttpPipelinedStream* CreateStreamOnExistingPipeline() override;

  virtual bool IsExistingPipelineAvailable() const override;

  // HttpPipelinedConnection::Delegate interface

  // Called when a pipelined connection completes a request. Adds a pending
  // request to the pipeline if the pipeline is still usable.
  virtual void OnPipelineHasCapacity(
      HttpPipelinedConnection* pipeline) override;

  virtual void OnPipelineFeedback(
      HttpPipelinedConnection* pipeline,
      HttpPipelinedConnection::Feedback feedback) override;

  virtual const Key& GetKey() const override;

  // Creates a Value summary of this host's |pipelines_|. Caller assumes
  // ownership of the returned Value.
  virtual base::Value* PipelineInfoToValue() const override;

  // Returns the maximum number of in-flight pipelined requests we'll allow on a
  // single connection.
  static int max_pipeline_depth() { return 3; }

 private:
  struct PipelineInfo {
    PipelineInfo();

    int num_successes;
  };
  typedef std::map<HttpPipelinedConnection*, PipelineInfo> PipelineInfoMap;

  // Called when a pipeline is empty and there are no pending requests. Closes
  // the connection.
  void OnPipelineEmpty(HttpPipelinedConnection* pipeline);

  // Adds the next pending request to the pipeline if it's still usuable.
  void AddRequestToPipeline(HttpPipelinedConnection* pipeline);

  // Returns the current pipeline capacity based on |capability_|. This should
  // not be called if |capability_| is INCAPABLE.
  int GetPipelineCapacity() const;

  // Returns true if |pipeline| can handle a new request. This is true if the
  // |pipeline| is active, usable, has capacity, and |capability_| is
  // sufficient.
  bool CanPipelineAcceptRequests(HttpPipelinedConnection* pipeline) const;

  // Called when |this| moves from UNKNOWN |capability_| to PROBABLY_CAPABLE.
  // Causes all pipelines to increase capacity to start pipelining.
  void NotifyAllPipelinesHaveCapacity();

  HttpPipelinedHost::Delegate* delegate_;
  const Key key_;
  PipelineInfoMap pipelines_;
  scoped_ptr<HttpPipelinedConnection::Factory> factory_;
  HttpPipelinedHostCapability capability_;

  DISALLOW_COPY_AND_ASSIGN(HttpPipelinedHostImpl);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PIPELINED_HOST_IMPL_H_
