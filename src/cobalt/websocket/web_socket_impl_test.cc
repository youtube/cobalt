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

#include "cobalt/websocket/web_socket.h"

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/test/scoped_task_environment.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/window.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/websocket/mock_websocket_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::DefaultValue;
using ::testing::Return;
using cobalt::script::testing::MockExceptionState;

namespace cobalt {
namespace websocket {
namespace {
// These limits are copied from net::WebSocketChannel implementation.
const int kDefaultSendQuotaHighWaterMark = 1 << 17;
const int k800KB = 800;
const int kTooMuch = kDefaultSendQuotaHighWaterMark + 1;
const int kWayTooMuch = kDefaultSendQuotaHighWaterMark * 2 + 1;
const int k512KB = 512;

class FakeSettings : public dom::DOMSettings {
 public:
  FakeSettings()
      : dom::DOMSettings(0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                         null_debugger_hooks_, NULL),
        base_("https://127.0.0.1:1234") {
    network_module_.reset(new network::NetworkModule());
    this->set_network_module(network_module_.get());
  }
  const GURL& base_url() const override { return base_; }

  // public members, so that they're easier for testing.
  base::NullDebuggerHooks null_debugger_hooks_;
  GURL base_;
  std::unique_ptr<network::NetworkModule> network_module_;
};
}  // namespace

class WebSocketImplTest : public ::testing::Test {
 public:
  dom::DOMSettings* settings() const { return settings_.get(); }
  void AddQuota(int quota) {
    network_task_runner_->PostBlockingTask(
        FROM_HERE,
        base::Bind(&WebSocketImpl::OnFlowControl, websocket_impl_, quota));
  }

 protected:
  WebSocketImplTest() : settings_(new FakeSettings()) {
    std::vector<std::string> sub_protocols;
    sub_protocols.push_back("chat");
    network_task_runner_ = settings_->network_module()
                               ->url_request_context_getter()
                               ->GetNetworkTaskRunner();
  }

  void SetUp() override {
    websocket_impl_ = new WebSocketImpl(settings_->network_module(), nullptr);
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
               MockWebSocketChannel** mock_channel_slot,
               dom::DOMSettings* settings) {
              *mock_channel_slot = new MockWebSocketChannel(
                  websocket_impl.get(), settings->network_module());
              websocket_impl->websocket_channel_ =
                  std::unique_ptr<net::WebSocketChannel>(*mock_channel_slot);
            },
            websocket_impl_, &mock_channel_, settings()));
  }

  void TearDown() override {
    network_task_runner_->PostBlockingTask(
        FROM_HERE,
        base::Bind(&WebSocketImpl::OnClose, websocket_impl_, true /*was_clan*/,
                   net::kWebSocketNormalClosure /*error_code*/,
                   "" /*close_reason*/));
  }

  base::test::ScopedTaskEnvironment env_;

  std::unique_ptr<FakeSettings> settings_;
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
    EXPECT_CALL(
        *mock_channel_,
        MockSendFrame(true, net::WebSocketFrameHeader::kOpCodeContinuation, _, 1))
        .Times(1)
        .WillOnce(Return(net::WebSocketChannel::CHANNEL_ALIVE));
    EXPECT_CALL(*mock_channel_,
                MockSendFrame(false, net::WebSocketFrameHeader::kOpCodeText, _,
                              k512KB - 1))
        .Times(1)
        .WillOnce(Return(net::WebSocketChannel::CHANNEL_ALIVE));
    EXPECT_CALL(*mock_channel_,
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
