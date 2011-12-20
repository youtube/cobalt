// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_pool.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_pipelined_host.h"
#include "net/http/http_pipelined_host_capability.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Ref;
using testing::Return;
using testing::ReturnNull;

namespace net {

namespace {

ClientSocketHandle* kDummyConnection =
    reinterpret_cast<ClientSocketHandle*>(188);
HttpPipelinedStream* kDummyStream =
    reinterpret_cast<HttpPipelinedStream*>(99);

class MockPoolDelegate : public HttpPipelinedHostPool::Delegate {
 public:
  MOCK_METHOD1(OnHttpPipelinedHostHasAdditionalCapacity,
               void(const HostPortPair& origin));
};

class MockHostFactory : public HttpPipelinedHost::Factory {
 public:
  MOCK_METHOD4(CreateNewHost, HttpPipelinedHost*(
      HttpPipelinedHost::Delegate* delegate, const HostPortPair& origin,
      HttpPipelinedConnection::Factory* factory,
      HttpPipelinedHostCapability capability));
};

class MockHost : public HttpPipelinedHost {
 public:
  MockHost(const HostPortPair& origin)
      : origin_(origin) {
  }

  MOCK_METHOD6(CreateStreamOnNewPipeline, HttpPipelinedStream*(
      ClientSocketHandle* connection,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated,
      SSLClientSocket::NextProto protocol_negotiated));
  MOCK_METHOD0(CreateStreamOnExistingPipeline, HttpPipelinedStream*());
  MOCK_CONST_METHOD0(IsExistingPipelineAvailable, bool());

  virtual const HostPortPair& origin() const OVERRIDE { return origin_; }

 private:
  HostPortPair origin_;
};

class HttpPipelinedHostPoolTest : public testing::Test {
 public:
  HttpPipelinedHostPoolTest()
      : origin_("host", 123),
        factory_(new MockHostFactory),  // Owned by pool_.
        host_(new MockHost(origin_)),  // Owned by pool_.
        http_server_properties_(new HttpServerPropertiesImpl),
        pool_(new HttpPipelinedHostPool(&delegate_, factory_,
                                        http_server_properties_.get())),
        was_npn_negotiated_(false),
        protocol_negotiated_(SSLClientSocket::kProtoUnknown) {
  }

  void CreateDummyStream() {
    EXPECT_CALL(*host_, CreateStreamOnNewPipeline(kDummyConnection,
                                                  Ref(ssl_config_),
                                                  Ref(proxy_info_),
                                                  Ref(net_log_),
                                                  was_npn_negotiated_,
                                                  protocol_negotiated_))
        .Times(1)
        .WillOnce(Return(kDummyStream));
    EXPECT_EQ(kDummyStream,
              pool_->CreateStreamOnNewPipeline(origin_, kDummyConnection,
                                               ssl_config_, proxy_info_,
                                               net_log_, was_npn_negotiated_,
                                               protocol_negotiated_));
  }

  HostPortPair origin_;
  MockPoolDelegate delegate_;
  MockHostFactory* factory_;
  MockHost* host_;
  scoped_ptr<HttpServerPropertiesImpl> http_server_properties_;
  scoped_ptr<HttpPipelinedHostPool> pool_;

  const SSLConfig ssl_config_;
  const ProxyInfo proxy_info_;
  const BoundNetLog net_log_;
  bool was_npn_negotiated_;
  SSLClientSocket::NextProto protocol_negotiated_;
};

TEST_F(HttpPipelinedHostPoolTest, DefaultUnknown) {
  EXPECT_TRUE(pool_->IsHostEligibleForPipelining(origin_));
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       PIPELINE_UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostIdle(host_);
}

TEST_F(HttpPipelinedHostPoolTest, RemembersIncapable) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       PIPELINE_UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostDeterminedCapability(host_, PIPELINE_INCAPABLE);
  pool_->OnHostIdle(host_);
  EXPECT_FALSE(pool_->IsHostEligibleForPipelining(origin_));
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       PIPELINE_INCAPABLE))
      .Times(0);
  EXPECT_EQ(NULL,
            pool_->CreateStreamOnNewPipeline(origin_, kDummyConnection,
                                             ssl_config_, proxy_info_, net_log_,
                                             was_npn_negotiated_,
                                             protocol_negotiated_));
}

TEST_F(HttpPipelinedHostPoolTest, RemembersCapable) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       PIPELINE_UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostDeterminedCapability(host_, PIPELINE_CAPABLE);
  pool_->OnHostIdle(host_);
  EXPECT_TRUE(pool_->IsHostEligibleForPipelining(origin_));

  host_ = new MockHost(origin_);
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       PIPELINE_CAPABLE))
      .Times(1)
      .WillOnce(Return(host_));
  CreateDummyStream();
  pool_->OnHostIdle(host_);
}

TEST_F(HttpPipelinedHostPoolTest, IncapableIsSticky) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       PIPELINE_UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostDeterminedCapability(host_, PIPELINE_CAPABLE);
  pool_->OnHostDeterminedCapability(host_, PIPELINE_INCAPABLE);
  pool_->OnHostDeterminedCapability(host_, PIPELINE_CAPABLE);
  pool_->OnHostIdle(host_);
  EXPECT_FALSE(pool_->IsHostEligibleForPipelining(origin_));
}

TEST_F(HttpPipelinedHostPoolTest, RemainsUnknownWithoutFeedback) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       PIPELINE_UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostIdle(host_);
  EXPECT_TRUE(pool_->IsHostEligibleForPipelining(origin_));

  host_ = new MockHost(origin_);
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       PIPELINE_UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostIdle(host_);
}

TEST_F(HttpPipelinedHostPoolTest, PopulatesServerProperties) {
  EXPECT_EQ(PIPELINE_UNKNOWN,
            http_server_properties_->GetPipelineCapability(host_->origin()));
  pool_->OnHostDeterminedCapability(host_, PIPELINE_CAPABLE);
  EXPECT_EQ(PIPELINE_CAPABLE,
            http_server_properties_->GetPipelineCapability(host_->origin()));
  delete host_;  // Must manually delete, because it's never added to |pool_|.
}

}  // anonymous namespace

}  // namespace net
