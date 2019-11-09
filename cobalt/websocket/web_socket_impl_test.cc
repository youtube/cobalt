// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
using cobalt::script::testing::MockExceptionState;

namespace cobalt {
namespace websocket {
namespace {
// These limits are copied from net::WebSocketChannel implementation.
// const int kDefaultSendQuotaLowWaterMark = 1 << 16;
const int kDefaultSendQuotaHighWaterMark = 1 << 17;

class FakeSettings : public dom::DOMSettings {
 public:
  FakeSettings()
      : dom::DOMSettings(0, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                         NULL, NULL),
        base_("https://localhost:1234") {
    network_module_.reset(new network::NetworkModule());
    this->set_network_module(network_module_.get());
  }
  const GURL& base_url() const override { return base_; }

  // public members, so that they're easier for testing.
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
    // Use local URL so that WebSocket will not complain about URL format.
    ws_ = new WebSocket(settings(), "ws://localhost:1234", sub_protocols,
                        &exception_state_, false);

    websocket_impl_ = ws_->impl_;
    mock_channel_ = new MockWebSocketChannel(websocket_impl_.get(),
                                             settings()->network_module());
    network_task_runner_ = settings_->network_module()
                               ->url_request_context_getter()
                               ->GetNetworkTaskRunner();
    // The holder is only created to be base::Passed() on the next line, it will
    // be empty so do not use it later.
    network_task_runner_->PostBlockingTask(
        FROM_HERE,
        base::Bind(
            [](scoped_refptr<WebSocketImpl> websocket_impl,
               net::WebSocketChannel* mock_channel) {
              websocket_impl->websocket_channel_ =
                  std::unique_ptr<net::WebSocketChannel>(mock_channel);
            },
            websocket_impl_, mock_channel_));
  }
  ~WebSocketImplTest() {
    network_task_runner_->PostBlockingTask(
        FROM_HERE,
        base::Bind(&WebSocketImpl::OnClose, websocket_impl_, true /*was_clan*/,
                   net::kWebSocketNormalClosure /*error_code*/,
                   "" /*close_reason*/));
  }

  base::test::ScopedTaskEnvironment env_;

  std::unique_ptr<FakeSettings> settings_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_refptr<WebSocket> ws_;
  scoped_refptr<WebSocketImpl> websocket_impl_;
  MockWebSocketChannel* mock_channel_;
  StrictMock<MockExceptionState> exception_state_;
};

TEST_F(WebSocketImplTest, NormalSizeRequest) {
  const int kNormalSize = 800;
  // Normally the high watermark quota is given at websocket connection success.
  AddQuota(kDefaultSendQuotaHighWaterMark);
  EXPECT_CALL(*mock_channel_,
              MockSendFrame(true, net::WebSocketFrameHeader::kOpCodeText, _,
                            kNormalSize))
      .Times(1);

  char data[kNormalSize];
  int32 buffered_amount = 0;
  std::string error;
  websocket_impl_->SendText(data, kNormalSize, &buffered_amount, &error);
}

TEST_F(WebSocketImplTest, LargeRequest) {
  AddQuota(kDefaultSendQuotaHighWaterMark);
  EXPECT_CALL(*mock_channel_,
              MockSendFrame(true, net::WebSocketFrameHeader::kOpCodeText, _,
                            kDefaultSendQuotaHighWaterMark))
      .Times(1);

  char data[kDefaultSendQuotaHighWaterMark];
  int32 buffered_amount = 0;
  std::string error;
  websocket_impl_->SendText(data, kDefaultSendQuotaHighWaterMark,
                            &buffered_amount, &error);
}

TEST_F(WebSocketImplTest, OverLimitRequest) {
  network_task_runner_->PostBlockingTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnFlowControl, websocket_impl_,
                            kDefaultSendQuotaHighWaterMark));
  const int kWayTooMuch = kDefaultSendQuotaHighWaterMark * 2 + 1;
  EXPECT_CALL(*mock_channel_,
              MockSendFrame(false, net::WebSocketFrameHeader::kOpCodeText, _,
                            kDefaultSendQuotaHighWaterMark))
      .Times(2);

  char data[kWayTooMuch];
  int32 buffered_amount = 0;
  std::string error;
  websocket_impl_->SendText(data, kWayTooMuch, &buffered_amount, &error);

  network_task_runner_->PostBlockingTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnFlowControl, websocket_impl_,
                            kDefaultSendQuotaHighWaterMark));

  EXPECT_CALL(*mock_channel_,
              MockSendFrame(true, net::WebSocketFrameHeader::kOpCodeText, _, 1))
      .Times(1);
  network_task_runner_->PostBlockingTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnFlowControl, websocket_impl_,
                            kDefaultSendQuotaHighWaterMark));
}


TEST_F(WebSocketImplTest, ReuseSocketForLargeRequest) {
  network_task_runner_->PostBlockingTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnFlowControl, websocket_impl_,
                            kDefaultSendQuotaHighWaterMark));
  const int kTooMuch = kDefaultSendQuotaHighWaterMark + 1;
  EXPECT_CALL(*mock_channel_,
              MockSendFrame(false, net::WebSocketFrameHeader::kOpCodeBinary, _,
                            kDefaultSendQuotaHighWaterMark))
      .Times(1);

  char data[kTooMuch];
  int32 buffered_amount = 0;
  std::string error;
  websocket_impl_->SendBinary(data, kTooMuch, &buffered_amount, &error);
  websocket_impl_->SendText(data, kTooMuch, &buffered_amount, &error);

  const int kSecondWaveofQuota = 512;
  EXPECT_CALL(
      *mock_channel_,
      MockSendFrame(true, net::WebSocketFrameHeader::kOpCodeBinary, _, 1))
      .Times(1);
  EXPECT_CALL(*mock_channel_,
              MockSendFrame(false, net::WebSocketFrameHeader::kOpCodeText, _,
                            kSecondWaveofQuota - 1))
      .Times(1);
  AddQuota(kSecondWaveofQuota);

  EXPECT_CALL(*mock_channel_,
              MockSendFrame(true, net::WebSocketFrameHeader::kOpCodeText, _,
                            kTooMuch - (kSecondWaveofQuota - 1)))
      .Times(1);
  AddQuota(kDefaultSendQuotaHighWaterMark);
}

}  // namespace websocket
}  // namespace cobalt
