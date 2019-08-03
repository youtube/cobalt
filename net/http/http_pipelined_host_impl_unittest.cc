// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_impl.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_pipelined_connection.h"
#include "net/http/http_pipelined_host_test_util.h"
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

class HttpPipelinedHostImplTest : public testing::Test {
 public:
  HttpPipelinedHostImplTest()
      : key_(HostPortPair("host", 123)),
        factory_(new MockPipelineFactory),  // Owned by host_.
        host_(new HttpPipelinedHostImpl(&delegate_, key_, factory_,
                                        PIPELINE_CAPABLE)) {
  }

  void SetCapability(HttpPipelinedHostCapability capability) {
    factory_ = new MockPipelineFactory;
    host_.reset(new HttpPipelinedHostImpl(
        &delegate_, key_, factory_, capability));
  }

  MockPipeline* AddTestPipeline(int depth, bool usable, bool active) {
    MockPipeline* pipeline = new MockPipeline(depth, usable, active);
    EXPECT_CALL(*factory_, CreateNewPipeline(kDummyConnection, host_.get(),
                                             MatchesOrigin(key_.origin()),
                                             Ref(ssl_config_), Ref(proxy_info_),
                                             Ref(net_log_), true,
                                             kProtoSPDY2))
        .Times(1)
        .WillOnce(Return(pipeline));
    EXPECT_CALL(*pipeline, CreateNewStream())
        .Times(1)
        .WillOnce(Return(kDummyStream));
    EXPECT_EQ(kDummyStream, host_->CreateStreamOnNewPipeline(
        kDummyConnection, ssl_config_, proxy_info_, net_log_, true,
        kProtoSPDY2));
    return pipeline;
  }

  void ClearTestPipeline(MockPipeline* pipeline) {
    pipeline->SetState(0, true, true);
    host_->OnPipelineHasCapacity(pipeline);
  }

  NiceMock<MockHostDelegate> delegate_;
  HttpPipelinedHost::Key key_;
  MockPipelineFactory* factory_;
  scoped_ptr<HttpPipelinedHostImpl> host_;

  SSLConfig ssl_config_;
  ProxyInfo proxy_info_;
  BoundNetLog net_log_;
};

TEST_F(HttpPipelinedHostImplTest, Delegate) {
  EXPECT_TRUE(key_.origin().Equals(host_->GetKey().origin()));
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
  SetCapability(PIPELINE_UNKNOWN);
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
  SetCapability(PIPELINE_UNKNOWN);
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
  SetCapability(PIPELINE_UNKNOWN);
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());
  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(0);
  EXPECT_CALL(delegate_,
              OnHostDeterminedCapability(host_.get(), PIPELINE_INCAPABLE))
      .Times(1);
  host_->OnPipelineFeedback(pipeline,
                            HttpPipelinedConnection::OLD_HTTP_VERSION);

  ClearTestPipeline(pipeline);
  EXPECT_EQ(NULL, host_->CreateStreamOnNewPipeline(
      kDummyConnection, ssl_config_, proxy_info_, net_log_, true,
      kProtoSPDY2));
}

TEST_F(HttpPipelinedHostImplTest, ShutsDownOnAuthenticationRequired) {
  SetCapability(PIPELINE_UNKNOWN);
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());
  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(0);
  EXPECT_CALL(delegate_,
              OnHostDeterminedCapability(host_.get(), PIPELINE_INCAPABLE))
      .Times(1);
  host_->OnPipelineFeedback(pipeline,
                            HttpPipelinedConnection::AUTHENTICATION_REQUIRED);

  ClearTestPipeline(pipeline);
  EXPECT_EQ(NULL, host_->CreateStreamOnNewPipeline(
      kDummyConnection, ssl_config_, proxy_info_, net_log_, true,
      kProtoSPDY2));
}

TEST_F(HttpPipelinedHostImplTest, ConnectionCloseHasNoEffect) {
  SetCapability(PIPELINE_UNKNOWN);
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
  SetCapability(PIPELINE_UNKNOWN);
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  EXPECT_CALL(delegate_,
              OnHostDeterminedCapability(host_.get(), PIPELINE_CAPABLE))
      .Times(1);
  host_->OnPipelineFeedback(pipeline, HttpPipelinedConnection::OK);

  pipeline->SetState(3, true, true);
  host_->OnPipelineFeedback(pipeline, HttpPipelinedConnection::OK);
  host_->OnPipelineFeedback(pipeline, HttpPipelinedConnection::OK);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostImplTest, IgnoresSocketErrorOnFirstRequest) {
  SetCapability(PIPELINE_UNKNOWN);
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_CALL(delegate_, OnHostDeterminedCapability(host_.get(), _))
      .Times(0);
  host_->OnPipelineFeedback(pipeline,
                            HttpPipelinedConnection::PIPELINE_SOCKET_ERROR);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  host_->OnPipelineFeedback(pipeline,
                            HttpPipelinedConnection::OK);

  EXPECT_CALL(delegate_,
              OnHostDeterminedCapability(host_.get(), PIPELINE_INCAPABLE))
      .Times(1);
  host_->OnPipelineFeedback(pipeline,
                            HttpPipelinedConnection::PIPELINE_SOCKET_ERROR);

  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostImplTest, HeedsSocketErrorOnFirstRequestWithPipeline) {
  SetCapability(PIPELINE_UNKNOWN);
  MockPipeline* pipeline = AddTestPipeline(2, true, true);

  EXPECT_CALL(delegate_,
              OnHostDeterminedCapability(host_.get(), PIPELINE_INCAPABLE))
      .Times(1);
  host_->OnPipelineFeedback(pipeline,
                            HttpPipelinedConnection::PIPELINE_SOCKET_ERROR);

  ClearTestPipeline(pipeline);
}

}  // anonymous namespace

}  // namespace net
