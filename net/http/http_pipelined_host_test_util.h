// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_connection.h"
#include "net/http/http_pipelined_host.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class MockHostDelegate : public HttpPipelinedHost::Delegate {
 public:
  MockHostDelegate();
  virtual ~MockHostDelegate();

  MOCK_METHOD1(OnHostIdle, void(HttpPipelinedHost* host));
  MOCK_METHOD1(OnHostHasAdditionalCapacity, void(HttpPipelinedHost* host));
  MOCK_METHOD2(OnHostDeterminedCapability,
               void(HttpPipelinedHost* host,
                    HttpPipelinedHostCapability capability));
};

class MockPipelineFactory : public HttpPipelinedConnection::Factory {
 public:
  MockPipelineFactory();
  virtual ~MockPipelineFactory();

  MOCK_METHOD8(CreateNewPipeline, HttpPipelinedConnection*(
      ClientSocketHandle* connection,
      HttpPipelinedConnection::Delegate* delegate,
      const HostPortPair& origin,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated,
      NextProto protocol_negotiated));
};

class MockPipeline : public HttpPipelinedConnection {
 public:
  MockPipeline(int depth, bool usable, bool active);
  virtual ~MockPipeline();

  void SetState(int depth, bool usable, bool active) {
    depth_ = depth;
    usable_ = usable;
    active_ = active;
  }

  virtual int depth() const override { return depth_; }
  virtual bool usable() const override { return usable_; }
  virtual bool active() const override { return active_; }

  MOCK_METHOD0(CreateNewStream, HttpPipelinedStream*());
  MOCK_METHOD1(OnStreamDeleted, void(int pipeline_id));
  MOCK_CONST_METHOD0(used_ssl_config, const SSLConfig&());
  MOCK_CONST_METHOD0(used_proxy_info, const ProxyInfo&());
  MOCK_CONST_METHOD0(net_log, const BoundNetLog&());
  MOCK_CONST_METHOD0(was_npn_negotiated, bool());
  MOCK_CONST_METHOD0(protocol_negotiated, NextProto());

 private:
  int depth_;
  bool usable_;
  bool active_;
};

MATCHER_P(MatchesOrigin, expected, "") { return expected.Equals(arg); }

}  // namespace net
