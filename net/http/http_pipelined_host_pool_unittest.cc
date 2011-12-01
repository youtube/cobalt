// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_pool.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_pipelined_host.h"
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
      HttpPipelinedHost::Capability capability));
};

class MockHost : public HttpPipelinedHost {
 public:
  MockHost(const HostPortPair& origin)
      : origin_(origin) {
  }

  MOCK_METHOD5(CreateStreamOnNewPipeline, HttpPipelinedStream*(
      ClientSocketHandle* connection,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated));
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
        pool_(new HttpPipelinedHostPool(&delegate_, factory_)),
        was_npn_negotiated_(false) {
  }

  void CreateDummyStream() {
    EXPECT_CALL(*host_, CreateStreamOnNewPipeline(kDummyConnection,
                                                  Ref(ssl_config_),
                                                  Ref(proxy_info_),
                                                  Ref(net_log_),
                                                  was_npn_negotiated_))
        .Times(1)
        .WillOnce(Return(kDummyStream));
    EXPECT_EQ(kDummyStream,
              pool_->CreateStreamOnNewPipeline(origin_, kDummyConnection,
                                               ssl_config_, proxy_info_,
                                               net_log_, was_npn_negotiated_));
  }

  HostPortPair origin_;
  MockPoolDelegate delegate_;
  MockHostFactory* factory_;
  MockHost* host_;
  scoped_ptr<HttpPipelinedHostPool> pool_;

  const SSLConfig ssl_config_;
  const ProxyInfo proxy_info_;
  const BoundNetLog net_log_;
  bool was_npn_negotiated_;
};

TEST_F(HttpPipelinedHostPoolTest, DefaultUnknown) {
  EXPECT_TRUE(pool_->IsHostEligibleForPipelining(origin_));
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       HttpPipelinedHost::UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostIdle(host_);
}

TEST_F(HttpPipelinedHostPoolTest, RemembersIncapable) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       HttpPipelinedHost::UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostDeterminedCapability(host_, HttpPipelinedHost::INCAPABLE);
  pool_->OnHostIdle(host_);
  EXPECT_FALSE(pool_->IsHostEligibleForPipelining(origin_));
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       HttpPipelinedHost::INCAPABLE))
      .Times(0);
  EXPECT_EQ(NULL,
            pool_->CreateStreamOnNewPipeline(origin_, kDummyConnection,
                                             ssl_config_, proxy_info_, net_log_,
                                             was_npn_negotiated_));
}

TEST_F(HttpPipelinedHostPoolTest, RemembersCapable) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       HttpPipelinedHost::UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostDeterminedCapability(host_, HttpPipelinedHost::CAPABLE);
  pool_->OnHostIdle(host_);
  EXPECT_TRUE(pool_->IsHostEligibleForPipelining(origin_));

  host_ = new MockHost(origin_);
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       HttpPipelinedHost::CAPABLE))
      .Times(1)
      .WillOnce(Return(host_));
  CreateDummyStream();
  pool_->OnHostIdle(host_);
}

TEST_F(HttpPipelinedHostPoolTest, IncapableIsSticky) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       HttpPipelinedHost::UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostDeterminedCapability(host_, HttpPipelinedHost::CAPABLE);
  pool_->OnHostDeterminedCapability(host_, HttpPipelinedHost::INCAPABLE);
  pool_->OnHostDeterminedCapability(host_, HttpPipelinedHost::CAPABLE);
  pool_->OnHostIdle(host_);
  EXPECT_FALSE(pool_->IsHostEligibleForPipelining(origin_));
}

TEST_F(HttpPipelinedHostPoolTest, RemainsUnknownWithoutFeedback) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       HttpPipelinedHost::UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostIdle(host_);
  EXPECT_TRUE(pool_->IsHostEligibleForPipelining(origin_));

  host_ = new MockHost(origin_);
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(origin_), _,
                                       HttpPipelinedHost::UNKNOWN))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream();
  pool_->OnHostIdle(host_);
}

}  // anonymous namespace

}  // namespace net
