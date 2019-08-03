// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_forced.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_pipelined_host_test_util.h"
#include "net/proxy/proxy_info.h"
#include "net/socket/client_socket_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Ref;
using testing::Return;

namespace net {

namespace {

HttpPipelinedStream* kDummyStream =
    reinterpret_cast<HttpPipelinedStream*>(24);

class HttpPipelinedHostForcedTest : public testing::Test {
 public:
  HttpPipelinedHostForcedTest()
      : key_(HostPortPair("host", 123)),
        factory_(new MockPipelineFactory),  // Owned by |host_|.
        host_(new HttpPipelinedHostForced(&delegate_, key_, factory_)) {
  }

  MockPipeline* AddTestPipeline() {
    MockPipeline* pipeline = new MockPipeline(0, true, true);
    EXPECT_CALL(*factory_, CreateNewPipeline(&connection_, host_.get(),
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
        &connection_, ssl_config_, proxy_info_, net_log_, true,
        kProtoSPDY2));
    return pipeline;
  }

  ClientSocketHandle connection_;
  NiceMock<MockHostDelegate> delegate_;
  HttpPipelinedHost::Key key_;
  MockPipelineFactory* factory_;
  scoped_ptr<HttpPipelinedHostForced> host_;

  SSLConfig ssl_config_;
  ProxyInfo proxy_info_;
  BoundNetLog net_log_;
};

TEST_F(HttpPipelinedHostForcedTest, Delegate) {
  EXPECT_TRUE(key_.origin().Equals(host_->GetKey().origin()));
}

TEST_F(HttpPipelinedHostForcedTest, SingleUser) {
  EXPECT_FALSE(host_->IsExistingPipelineAvailable());

  MockPipeline* pipeline = AddTestPipeline();
  EXPECT_TRUE(host_->IsExistingPipelineAvailable());

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  EXPECT_CALL(delegate_, OnHostIdle(host_.get()))
      .Times(1);
  host_->OnPipelineHasCapacity(pipeline);
}

TEST_F(HttpPipelinedHostForcedTest, ReuseExisting) {
  EXPECT_EQ(NULL, host_->CreateStreamOnExistingPipeline());

  MockPipeline* pipeline = AddTestPipeline();
  EXPECT_CALL(*pipeline, CreateNewStream())
      .Times(1)
      .WillOnce(Return(kDummyStream));
  EXPECT_EQ(kDummyStream, host_->CreateStreamOnExistingPipeline());

  pipeline->SetState(1, true, true);
  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  EXPECT_CALL(delegate_, OnHostIdle(host_.get()))
      .Times(0);
  host_->OnPipelineHasCapacity(pipeline);

  pipeline->SetState(0, true, true);
  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(host_.get()))
      .Times(1);
  EXPECT_CALL(delegate_, OnHostIdle(host_.get()))
      .Times(1);
  host_->OnPipelineHasCapacity(pipeline);
}

}  // anonymous namespace

}  // namespace net
