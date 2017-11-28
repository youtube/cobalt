// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_pool.h"

#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
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
               void(HttpPipelinedHost* host));
};

class MockHostFactory : public HttpPipelinedHost::Factory {
 public:
  MOCK_METHOD5(CreateNewHost, HttpPipelinedHost*(
      HttpPipelinedHost::Delegate* delegate,
      const HttpPipelinedHost::Key& key,
      HttpPipelinedConnection::Factory* factory,
      HttpPipelinedHostCapability capability,
      bool force_pipelining));
};

class MockHost : public HttpPipelinedHost {
 public:
  MockHost(const Key& key)
      : key_(key) {
  }

  MOCK_METHOD6(CreateStreamOnNewPipeline, HttpPipelinedStream*(
      ClientSocketHandle* connection,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated,
      NextProto protocol_negotiated));
  MOCK_METHOD0(CreateStreamOnExistingPipeline, HttpPipelinedStream*());
  MOCK_CONST_METHOD0(IsExistingPipelineAvailable, bool());
  MOCK_CONST_METHOD0(PipelineInfoToValue, base::Value*());

  virtual const Key& GetKey() const override { return key_; }

 private:
  Key key_;
};

class HttpPipelinedHostPoolTest : public testing::Test {
 public:
  HttpPipelinedHostPoolTest()
      : key_(HostPortPair("host", 123)),
        factory_(new MockHostFactory),  // Owned by pool_.
        host_(new MockHost(key_)),  // Owned by pool_.
        http_server_properties_(new HttpServerPropertiesImpl),
        pool_(new HttpPipelinedHostPool(&delegate_, factory_,
                                        http_server_properties_.get(), false)),
        was_npn_negotiated_(false),
        protocol_negotiated_(kProtoUnknown) {
  }

  void CreateDummyStream(const HttpPipelinedHost::Key& key,
                         ClientSocketHandle* connection,
                         HttpPipelinedStream* stream,
                         MockHost* host) {
    EXPECT_CALL(*host, CreateStreamOnNewPipeline(connection,
                                                 Ref(ssl_config_),
                                                 Ref(proxy_info_),
                                                 Ref(net_log_),
                                                 was_npn_negotiated_,
                                                 protocol_negotiated_))
        .Times(1)
        .WillOnce(Return(stream));
    EXPECT_EQ(stream,
              pool_->CreateStreamOnNewPipeline(key, connection,
                                               ssl_config_, proxy_info_,
                                               net_log_, was_npn_negotiated_,
                                               protocol_negotiated_));
  }

  MockHost* CreateDummyHost(const HttpPipelinedHost::Key& key) {
    MockHost* mock_host = new MockHost(key);
    EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(key), _,
                                         PIPELINE_UNKNOWN, false))
        .Times(1)
        .WillOnce(Return(mock_host));
    ClientSocketHandle* dummy_connection =
        reinterpret_cast<ClientSocketHandle*>(
            static_cast<uintptr_t>(base::RandUint64()));
    HttpPipelinedStream* dummy_stream =
        reinterpret_cast<HttpPipelinedStream*>(
          static_cast<uintptr_t>(base::RandUint64()));
    CreateDummyStream(key, dummy_connection, dummy_stream, mock_host);
    return mock_host;
  }

  HttpPipelinedHost::Key key_;
  MockPoolDelegate delegate_;
  MockHostFactory* factory_;
  MockHost* host_;
  scoped_ptr<HttpServerPropertiesImpl> http_server_properties_;
  scoped_ptr<HttpPipelinedHostPool> pool_;

  const SSLConfig ssl_config_;
  const ProxyInfo proxy_info_;
  const BoundNetLog net_log_;
  bool was_npn_negotiated_;
  NextProto protocol_negotiated_;
};

TEST_F(HttpPipelinedHostPoolTest, DefaultUnknown) {
  EXPECT_TRUE(pool_->IsKeyEligibleForPipelining(key_));
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(key_), _,
                                       PIPELINE_UNKNOWN, false))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream(key_, kDummyConnection, kDummyStream, host_);
  pool_->OnHostIdle(host_);
}

TEST_F(HttpPipelinedHostPoolTest, RemembersIncapable) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(key_), _,
                                       PIPELINE_UNKNOWN, false))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream(key_, kDummyConnection, kDummyStream, host_);
  pool_->OnHostDeterminedCapability(host_, PIPELINE_INCAPABLE);
  pool_->OnHostIdle(host_);
  EXPECT_FALSE(pool_->IsKeyEligibleForPipelining(key_));
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(key_), _,
                                       PIPELINE_INCAPABLE, false))
      .Times(0);
  EXPECT_EQ(NULL,
            pool_->CreateStreamOnNewPipeline(key_, kDummyConnection,
                                             ssl_config_, proxy_info_, net_log_,
                                             was_npn_negotiated_,
                                             protocol_negotiated_));
}

TEST_F(HttpPipelinedHostPoolTest, RemembersCapable) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(key_), _,
                                       PIPELINE_UNKNOWN, false))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream(key_, kDummyConnection, kDummyStream, host_);
  pool_->OnHostDeterminedCapability(host_, PIPELINE_CAPABLE);
  pool_->OnHostIdle(host_);
  EXPECT_TRUE(pool_->IsKeyEligibleForPipelining(key_));

  host_ = new MockHost(key_);
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(key_), _,
                                       PIPELINE_CAPABLE, false))
      .Times(1)
      .WillOnce(Return(host_));
  CreateDummyStream(key_, kDummyConnection, kDummyStream, host_);
  pool_->OnHostIdle(host_);
}

TEST_F(HttpPipelinedHostPoolTest, IncapableIsSticky) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(key_), _,
                                       PIPELINE_UNKNOWN, false))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream(key_, kDummyConnection, kDummyStream, host_);
  pool_->OnHostDeterminedCapability(host_, PIPELINE_CAPABLE);
  pool_->OnHostDeterminedCapability(host_, PIPELINE_INCAPABLE);
  pool_->OnHostDeterminedCapability(host_, PIPELINE_CAPABLE);
  pool_->OnHostIdle(host_);
  EXPECT_FALSE(pool_->IsKeyEligibleForPipelining(key_));
}

TEST_F(HttpPipelinedHostPoolTest, RemainsUnknownWithoutFeedback) {
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(key_), _,
                                       PIPELINE_UNKNOWN, false))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream(key_, kDummyConnection, kDummyStream, host_);
  pool_->OnHostIdle(host_);
  EXPECT_TRUE(pool_->IsKeyEligibleForPipelining(key_));

  host_ = new MockHost(key_);
  EXPECT_CALL(*factory_, CreateNewHost(pool_.get(), Ref(key_), _,
                                       PIPELINE_UNKNOWN, false))
      .Times(1)
      .WillOnce(Return(host_));

  CreateDummyStream(key_, kDummyConnection, kDummyStream, host_);
  pool_->OnHostIdle(host_);
}

TEST_F(HttpPipelinedHostPoolTest, PopulatesServerProperties) {
  EXPECT_EQ(PIPELINE_UNKNOWN,
            http_server_properties_->GetPipelineCapability(
                host_->GetKey().origin()));
  pool_->OnHostDeterminedCapability(host_, PIPELINE_CAPABLE);
  EXPECT_EQ(PIPELINE_CAPABLE,
            http_server_properties_->GetPipelineCapability(
                host_->GetKey().origin()));
  delete host_;  // Must manually delete, because it's never added to |pool_|.
}

TEST_F(HttpPipelinedHostPoolTest, MultipleKeys) {
  HttpPipelinedHost::Key key1(HostPortPair("host", 123));
  HttpPipelinedHost::Key key2(HostPortPair("host", 456));
  HttpPipelinedHost::Key key3(HostPortPair("other", 456));
  HttpPipelinedHost::Key key4(HostPortPair("other", 789));
  MockHost* host1 = CreateDummyHost(key1);
  MockHost* host2 = CreateDummyHost(key2);
  MockHost* host3 = CreateDummyHost(key3);

  EXPECT_CALL(*host1, IsExistingPipelineAvailable())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(pool_->IsExistingPipelineAvailableForKey(key1));

  EXPECT_CALL(*host2, IsExistingPipelineAvailable())
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(pool_->IsExistingPipelineAvailableForKey(key2));

  EXPECT_CALL(*host3, IsExistingPipelineAvailable())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(pool_->IsExistingPipelineAvailableForKey(key3));

  EXPECT_FALSE(pool_->IsExistingPipelineAvailableForKey(key4));

  pool_->OnHostIdle(host1);
  pool_->OnHostIdle(host2);
  pool_->OnHostIdle(host3);

  delete host_;  // Must manually delete, because it's never added to |pool_|.
}

}  // anonymous namespace

}  // namespace net
