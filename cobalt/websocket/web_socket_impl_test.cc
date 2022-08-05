// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/websocket/web_socket_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/testing/stub_web_context.h"
#include "cobalt/websocket/mock_websocket_channel.h"
#include "cobalt/websocket/web_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::script::testing::MockExceptionState;
using ::testing::_;
using ::testing::DefaultValue;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace cobalt {
namespace websocket {
namespace {
// These limits are copied from net::WebSocketChannel implementation.
const int kDefaultSendQuotaHighWaterMark = 1 << 17;
const int k800KB = 800;
const int kTooMuch = kDefaultSendQuotaHighWaterMark + 1;
const int kWayTooMuch = kDefaultSendQuotaHighWaterMark * 2 + 1;
const int k512KB = 512;
}  // namespace

class WebSocketImplTest : public ::testing::Test {
 public:
  web::EnvironmentSettings* settings() const {
    return web_context_->environment_settings();
  }
  void AddQuota(int quota) {
    network_task_runner_->PostBlockingTask(
        FROM_HERE,
        base::Bind(&WebSocketImpl::OnFlowControl, websocket_impl_, quota));
  }

 protected:
  WebSocketImplTest() : web_context_(new web::testing::StubWebContext()) {
    web_context_->setup_environment_settings(
        new dom::testing::StubEnvironmentSettings());
    web_context_->environment_settings()->set_creation_url(
        GURL("https://127.0.0.1:1234"));
    std::vector<std::string> sub_protocols;
    sub_protocols.push_back("chat");
    network_task_runner_ = web_context_->network_module()
                               ->url_request_context_getter()
                               ->GetNetworkTaskRunner();
  }

  void SetUp() override {
    websocket_impl_ =
        new WebSocketImpl(web_context_->network_module(), nullptr);
    // Setting this was usually done by WebSocketImpl::Connect, but since we do
    // not do Connect for every test, we have to make sure its task runner is
    // set.
    websocket_impl_->delegate_task_runner_ = network_task_runner_;
    // The holder is only created to be base::Passed() on the next line, it will
    // be empty so do not use it later.
    network_task_runner_->PostBlockingTask(
        FROM_HERE,
        base::Bind(
            [](scoped_refptr<WebSocketImpl> websocket_impl,
               web::Context* web_context,
               MockWebSocketChannel** mock_channel_slot) {
              *mock_channel_slot = new MockWebSocketChannel(
                  websocket_impl, web_context->network_module());
              websocket_impl->websocket_channel_ =
                  std::unique_ptr<net::WebSocketChannel>(*mock_channel_slot);
            },
            websocket_impl_, web_context_.get(), &mock_channel_));
  }

  void TearDown() override {
    network_task_runner_->PostBlockingTask(
        FROM_HERE,
        base::Bind(&WebSocketImpl::OnClose, websocket_impl_, true /*was_clan*/,
                   net::kWebSocketNormalClosure /*error_code*/,
                   "" /*close_reason*/));
  }

  std::unique_ptr<web::testing::StubWebContext> web_context_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_refptr<WebSocketImpl> websocket_impl_;
  MockWebSocketChannel* mock_channel_;
  StrictMock<MockExceptionState> exception_state_;
};

TEST_F(WebSocketImplTest, NormalSizeRequest) {
  // Normally the high watermark quota is given at websocket connection success.
  AddQuota(kDefaultSendQuotaHighWaterMark);

  {
    base::AutoLock scoped_lock(mock_channel_->lock());
    // mock_channel_ is created and used on network thread.
    EXPECT_CALL(
        *mock_channel_,
        MockSendFrame(true, net::WebSocketFrameHeader::kOpCodeText, _, k800KB))
        .Times(1)
        .WillOnce(Return(net::WebSocketChannel::CHANNEL_ALIVE));
  }

  char data[k800KB];
  int32 buffered_amount = 0;
  std::string error;
  websocket_impl_->SendText(data, k800KB, &buffered_amount, &error);
}

TEST_F(WebSocketImplTest, LargeRequest) {
  AddQuota(kDefaultSendQuotaHighWaterMark);

  // mock_channel_ is created and used on network thread.
  {
    base::AutoLock scoped_lock(mock_channel_->lock());
    EXPECT_CALL(*mock_channel_,
                MockSendFrame(true, net::WebSocketFrameHeader::kOpCodeText, _,
                              kDefaultSendQuotaHighWaterMark))
        .Times(1)
        .WillOnce(Return(net::WebSocketChannel::CHANNEL_ALIVE));
  }

  char data[kDefaultSendQuotaHighWaterMark];
  int32 buffered_amount = 0;
  std::string error;
  websocket_impl_->SendText(data, kDefaultSendQuotaHighWaterMark,
                            &buffered_amount, &error);
}

TEST_F(WebSocketImplTest, OverLimitRequest) {
  AddQuota(kDefaultSendQuotaHighWaterMark);

  // mock_channel_ is created and used on network thread.
  {
    base::AutoLock scoped_lock(mock_channel_->lock());
    EXPECT_CALL(*mock_channel_,
                MockSendFrame(false, net::WebSocketFrameHeader::kOpCodeText, _,
                              kDefaultSendQuotaHighWaterMark))
        .Times(1)
        .WillRepeatedly(Return(net::WebSocketChannel::CHANNEL_ALIVE));

    EXPECT_CALL(
        *mock_channel_,
        MockSendFrame(false, net::WebSocketFrameHeader::kOpCodeContinuation, _,
                      kDefaultSendQuotaHighWaterMark))
        .Times(1)
        .WillRepeatedly(Return(net::WebSocketChannel::CHANNEL_ALIVE));

    EXPECT_CALL(*mock_channel_,
                MockSendFrame(
                    true, net::WebSocketFrameHeader::kOpCodeContinuation, _, 1))
        .Times(1)
        .WillOnce(Return(net::WebSocketChannel::CHANNEL_ALIVE));
  }

  char data[kWayTooMuch];
  int32 buffered_amount = 0;
  std::string error;
  websocket_impl_->SendText(data, kWayTooMuch, &buffered_amount, &error);

  AddQuota(kDefaultSendQuotaHighWaterMark);
  AddQuota(kDefaultSendQuotaHighWaterMark);
}

TEST_F(WebSocketImplTest, ReuseSocketForLargeRequest) {
  AddQuota(kDefaultSendQuotaHighWaterMark);

  // mock_channel_ is created and used on network thread.
  {
    base::AutoLock scoped_lock(mock_channel_->lock());
    EXPECT_CALL(*mock_channel_,
                MockSendFrame(false, net::WebSocketFrameHeader::kOpCodeBinary,
                              _, kDefaultSendQuotaHighWaterMark))
        .Times(1)
        .WillOnce(Return(net::WebSocketChannel::CHANNEL_ALIVE));
    EXPECT_CALL(*mock_channel_,
                MockSendFrame(
                    true, net::WebSocketFrameHeader::kOpCodeContinuation, _, 1))
        .Times(1)
        .WillOnce(Return(net::WebSocketChannel::CHANNEL_ALIVE));
    EXPECT_CALL(*mock_channel_,
                MockSendFrame(false, net::WebSocketFrameHeader::kOpCodeText, _,
                              k512KB - 1))
        .Times(1)
        .WillOnce(Return(net::WebSocketChannel::CHANNEL_ALIVE));
    EXPECT_CALL(
        *mock_channel_,
        MockSendFrame(true, net::WebSocketFrameHeader::kOpCodeContinuation, _,
                      kTooMuch - (k512KB - 1)))
        .Times(1)
        .WillOnce(Return(net::WebSocketChannel::CHANNEL_ALIVE));
  }

  char data[kTooMuch];
  int32 buffered_amount = 0;
  std::string error;
  websocket_impl_->SendBinary(data, kTooMuch, &buffered_amount, &error);
  websocket_impl_->SendText(data, kTooMuch, &buffered_amount, &error);

  AddQuota(k512KB);
  AddQuota(kDefaultSendQuotaHighWaterMark);
}

}  // namespace websocket
}  // namespace cobalt
