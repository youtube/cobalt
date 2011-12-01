// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_impl.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_pipelined_connection.h"
#include "net/proxy/proxy_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Ref;
using testing::Return;
using testing::ReturnNull;

namespace net {

namespace {

ClientSocketHandle* kDummyConnection =
    reinterpret_cast<ClientSocketHandle*>(84);
HttpPipelinedStream* kDummyStream =
    reinterpret_cast<HttpPipelinedStream*>(42);

class MockHostDelegate : public HttpPipelinedHost::Delegate {
 public:
  MOCK_METHOD1(OnHostIdle, void(HttpPipelinedHost* host));
  MOCK_METHOD1(OnHostHasAdditionalCapacity, void(HttpPipelinedHost* host));
  MOCK_METHOD2(OnHostDeterminedCapability,
               void(HttpPipelinedHost* host,
                    HttpPipelinedHost::Capability capability));
};

class MockPipelineFactory : public HttpPipelinedConnection::Factory {
 public:
  MOCK_METHOD6(CreateNewPipeline, HttpPipelinedConnection*(
      ClientSocketHandle* connection,
      HttpPipelinedConnection::Delegate* delegate,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated));
};

class MockPipeline : public HttpPipelinedConnection {
 public:
  MockPipeline(int depth, bool usable, bool active)
      : depth_(depth),
        usable_(usable),
        active_(active) {
  }

  void SetState(int depth, bool usable, bool active) {
    depth_ = depth;
    usable_ = usable;
    active_ = active;
  }

  virtual int depth() const OVERRIDE { return depth_; }
  virtual bool usable() const OVERRIDE { return usable_; }
  virtual bool active() const OVERRIDE { return active_; }

  MOCK_METHOD0(CreateNewStream, HttpPipelinedStream*());
  MOCK_METHOD1(OnStreamDeleted, void(int pipeline_id));
  MOCK_CONST_METHOD0(used_ssl_config, const SSLConfig&());
  MOCK_CONST_METHOD0(used_proxy_info, const ProxyInfo&());
  MOCK_CONST_METHOD0(source, const NetLog::Source&());
  MOCK_CONST_METHOD0(was_npn_negotiated, bool());

 private:
  int depth_;
  bool usable_;
  bool active_;
};

class HttpPipelinedHostImplTest : public testing::Test {
 public:
  HttpPipelinedHostImplTest()
      : origin_("host", 123),
        factory_(new MockPipelineFactory),  // Owned by host_.
        host_(new HttpPipelinedHostImpl(&delegate_, origin_, factory_,
                                        HttpPipelinedHost::CAPABLE)) {
  }

  void SetCapability(HttpPipelinedHost::Capability capability) {
    factory_ = new MockPipelineFactory;
    host_.reset(new HttpPipelinedHostImpl(
        &delegate_, origin_, factory_, capability));
  }

  MockPipeline* AddTestPipeline(int depth, bool usable, bool active) {
    MockPipeline* pipeline = new MockPipeline(depth, usable, active);
    EXPECT_CALL(*factory_, CreateNewPipeline(kDummyConnection, host_.get(),
                                             Ref(ssl_config_), Ref(proxy_info_),
                                             Ref(net_log_), true))
        .Times(1)
        .WillOnce(Return(pipeline));
    EXPECT_CALL(*pipeline, CreateNewStream())
        .Times(1)
        .WillOnce(Return(kDummyStream));
    EXPECT_EQ(kDummyStream, host_->CreateStreamOnNewPipeline(
        kDummyConnection, ssl_config_, proxy_info_, net_log_, true));
    return pipeline;
  }

  void ClearTestPipeline(MockPipeline* pipeline) {
    pipeline->SetState(0, true, true);
    host_->OnPipelineHasCapacity(pipeline);
  }

  NiceMock<MockHostDelegate> delegate_;
  HostPortPair origin_;
  MockPipelineFactory* factory_;
  scoped_ptr<HttpPipelinedHostImpl> host_;

  SSLConfig ssl_config_;
  ProxyInfo proxy_info_;
  BoundNetLog net_log_;
};

TEST_F(HttpPipelinedHostImplTest, Delegate) {
  EXPECT_TRUE(origin_.Equals(host_->origin()));
}

TEST_F(HttpPipelinedHostImplTest, OnUnusablePipelineHasCapacity) {
  MockPipeline* pipeline = AddTestPipeline(0, false, true);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(0);
  EXPECT_CALL(delegate_, OnHostIdle(host_.get()))
      .Times(1);
  host_->OnPipelineHasCapacity(pipeline);
}

TEST_F(HttpPipelinedHostImplTest, OnUsablePipelineHasCapacity) {
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  EXPECT_CALL(delegate_, OnHostIdle(host_.get()))
      .Times(0);

  host_->OnPipelineHasCapacity(pipeline);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  EXPECT_CALL(delegate_, OnHostIdle(host_.get()))
      .Times(1);
  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostImplTest, IgnoresUnusablePipeline) {
  MockPipeline* pipeline = AddTestPipeline(1, false, true);

  EXPECT_FALSE(host_->IsExistingPipelineAvailable());
  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());

  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostImplTest, IgnoresInactivePipeline) {
  MockPipeline* pipeline = AddTestPipeline(1, true, false);

  EXPECT_FALSE(host_->IsExistingPipelineAvailable());
  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());

  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostImplTest, IgnoresFullPipeline) {
  MockPipeline* pipeline = AddTestPipeline(
      HttpPipelinedHostImpl::max_pipeline_depth(), true, true);

  EXPECT_FALSE(host_->IsExistingPipelineAvailable());
  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());

  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostImplTest, PicksLeastLoadedPipeline) {
  MockPipeline* full_pipeline = AddTestPipeline(
      HttpPipelinedHostImpl::max_pipeline_depth(), true, true);
  MockPipeline* usable_pipeline = AddTestPipeline(
      HttpPipelinedHostImpl::max_pipeline_depth() - 1, true, true);
  MockPipeline* empty_pipeline = AddTestPipeline(0, true, true);

  EXPECT_TRUE(host_->IsExistingPipelineAvailable());
  EXPECT_CALL(*empty_pipeline, CreateNewStream())
      .Times(1)
      .WillOnce(ReturnNull());
  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());

  ClearTestPipeline(full_pipeline);
  ClearTestPipeline(usable_pipeline);
  ClearTestPipeline(empty_pipeline);
}

TEST_F(HttpPipelinedHostImplTest, OpensUpOnPipelineSuccess) {
  SetCapability(HttpPipelinedHost::UNKNOWN);
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());
  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  host_->OnPipelineFeedback(pipeline, HttpPipelinedConnection::OK);

  EXPECT_CALL(*pipeline, CreateNewStream())
      .Times(1)
      .WillOnce(Return(kDummyStream));
  EXPECT_EQ(kDummyStream, host_->CreateStreamOnExistingPipeline());

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostImplTest, OpensAllPipelinesOnPipelineSuccess) {
  SetCapability(HttpPipelinedHost::UNKNOWN);
  MockPipeline* pipeline1 = AddTestPipeline(1, false, true);
  MockPipeline* pipeline2 = AddTestPipeline(1, true, true);

  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());
  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  host_->OnPipelineFeedback(pipeline1, HttpPipelinedConnection::OK);

  EXPECT_CALL(*pipeline2, CreateNewStream())
      .Times(1)
      .WillOnce(Return(kDummyStream));
  EXPECT_EQ(kDummyStream, host_->CreateStreamOnExistingPipeline());

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(2);
  ClearTestPipeline(pipeline1);
  ClearTestPipeline(pipeline2);
}

TEST_F(HttpPipelinedHostImplTest, ShutsDownOnOldVersion) {
  SetCapability(HttpPipelinedHost::UNKNOWN);
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());
  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(0);
  EXPECT_CALL(delegate_,
              OnHostDeterminedCapability(host_.get(),
                                         HttpPipelinedHost::INCAPABLE))
      .Times(1);
  host_->OnPipelineFeedback(pipeline,
                            HttpPipelinedConnection::OLD_HTTP_VERSION);

  ClearTestPipeline(pipeline);
  EXPECT_EQ(NULL, host_->CreateStreamOnNewPipeline(
      kDummyConnection, ssl_config_, proxy_info_, net_log_, true));
}

TEST_F(HttpPipelinedHostImplTest, ConnectionCloseHasNoEffect) {
  SetCapability(HttpPipelinedHost::UNKNOWN);
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(0);
  EXPECT_CALL(delegate_, OnHostDeterminedCapability(host_.get(), _))
      .Times(0);
  host_->OnPipelineFeedback(pipeline,
                            HttpPipelinedConnection::MUST_CLOSE_CONNECTION);
  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostImplTest, SuccessesLeadToCapable) {
  SetCapability(HttpPipelinedHost::UNKNOWN);
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  EXPECT_CALL(delegate_,
              OnHostDeterminedCapability(host_.get(),
                                         HttpPipelinedHost::CAPABLE))
      .Times(1);
  host_->OnPipelineFeedback(pipeline, HttpPipelinedConnection::OK);

  pipeline->SetState(3, true, true);
  host_->OnPipelineFeedback(pipeline, HttpPipelinedConnection::OK);
  host_->OnPipelineFeedback(pipeline, HttpPipelinedConnection::OK);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  ClearTestPipeline(pipeline);
}

}  // anonymous namespace

}  // namespace net
