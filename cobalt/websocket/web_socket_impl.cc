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

#include <algorithm>
#include <cstdint>
#include <memory>

#include "base/basictypes.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/websocket/web_socket.h"
#include "net/http/http_util.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_handshake_stream_create_helper.h"
#include "starboard/memory.h"

namespace cobalt {
namespace websocket {

WebSocketImpl::WebSocketImpl(cobalt::network::NetworkModule *network_module,
                             WebSocket *delegate)
    : network_module_(network_module), delegate_(delegate) {
  DCHECK(base::MessageLoop::current());
  owner_task_runner_ = base::MessageLoop::current()->task_runner();
}

void WebSocketImpl::ResetWebSocketEventDelegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = NULL;
}

void WebSocketImpl::Connect(const std::string &origin, const GURL &url,
                            const std::vector<std::string> &sub_protocols) {
  if (!network_module_) {
    DLOG(WARNING) << "Trying to make web socket connection without network "
                     "module, aborting.";
    return;
  }
  DCHECK(network_module_->url_request_context_getter());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  origin_ = origin;

  DLOG(INFO) << "Connecting to websocket at " << url.spec();

  connect_url_ = url;
  desired_sub_protocols_ = sub_protocols;

  // TODO: Network Task Runner might not be the optimal task runner.
  // To achieve low latency communication via websockets, a dedicated high
  // priority thread might be required.  Investigation is needed.
  delegate_task_runner_ =
      network_module_->url_request_context_getter()->GetNetworkTaskRunner();
  base::WaitableEvent channel_created_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::Closure create_socket_stream_closure(
      base::Bind(&WebSocketImpl::DoConnect, this,
                 network_module_->url_request_context_getter(), url,
                 base::Unretained(&channel_created_event)));
  delegate_task_runner_->PostTask(FROM_HERE, create_socket_stream_closure);

  // Wait for the channel to be completed.  This event is signaled after
  // websocket_channel_ has been assigned.
  channel_created_event.Wait();
}

void WebSocketImpl::DoConnect(
    scoped_refptr<cobalt::network::URLRequestContextGetter> context,
    const GURL &url, base::WaitableEvent *channel_created_event) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(url.is_valid());
  DCHECK(channel_created_event);
  DCHECK(context->GetURLRequestContext());

  websocket_channel_ = std::make_unique<net::WebSocketChannel>(
      std::unique_ptr<net::WebSocketEventInterface>(
          new CobaltWebSocketEventHandler(this)),
      context->GetURLRequestContext());
  websocket_channel_->SendAddChannelRequest(
      url, desired_sub_protocols_, url::Origin::Create(GURL(origin_)),
      GURL() /*site_for_cookies*/,
      net::HttpRequestHeaders() /*additional_headers*/);
  channel_created_event
      ->Signal();  // Signal that this->websocket_channel_ has been assigned.
}

void WebSocketImpl::Close(const net::WebSocketError code,
                          const std::string &reason) {
  CloseInfo close_info(code, reason);
  delegate_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebSocketImpl::DoClose, this, close_info));
}

void WebSocketImpl::DoClose(const CloseInfo &close_info) {
  if (!websocket_channel_) {
    return;
  }
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  auto channel_state = websocket_channel_->StartClosingHandshake(
      close_info.code, close_info.reason);
  if (channel_state == net::WebSocketChannel::CHANNEL_DELETED) {
    websocket_channel_.reset();
  }
}

WebSocketImpl::~WebSocketImpl() {
  if (websocket_channel_) {
    delegate_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WebSocketImpl::DoClose, base::Unretained(this),
                              CloseInfo(net::kWebSocketNormalClosure)));
  }
}

// The main reason to call TrampolineClose is to ensure messages that are posted
// from this thread prior to this function call are processed before the
// connection is closed.
void WebSocketImpl::TrampolineClose(const CloseInfo &close_info) {
  base::Closure no_op_closure =
      base::Closure(base::Bind([]() {} /*Do nothing*/));

  base::Closure do_close_closure(
      base::Bind(&WebSocketImpl::DoClose, this, close_info));
  owner_task_runner_->PostTaskAndReply(FROM_HERE, no_op_closure,
                                       do_close_closure);
}


void WebSocketImpl::OnHandshakeComplete(
    const std::string &selected_subprotocol) {
  owner_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnWebSocketConnected, this,
                            selected_subprotocol));
}

void WebSocketImpl::OnWebSocketConnected(
    const std::string &selected_subprotocol) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_) {
    delegate_->OnConnected(selected_subprotocol);
  }
}

void WebSocketImpl::OnWebSocketDisconnected(bool was_clean, uint16 code,
                                            const std::string &reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_) {
    delegate_->OnDisconnected(was_clean, code, reason);
  }
}

void WebSocketImpl::OnWebSocketReceivedData(
    bool is_text_frame, scoped_refptr<net::IOBufferWithSize> data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_) {
    delegate_->OnReceivedData(is_text_frame, data);
  }
}

void WebSocketImpl::OnClose(bool was_clean, int error_code,
                            const std::string &close_reason) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  std::uint16_t close_code = static_cast<std::uint16_t>(error_code);

  DLOG(INFO) << "WebSocket is closing."
             << " code[" << close_code << "] reason[" << close_reason << "]"
             << " was_clean: " << was_clean;

  websocket_channel_.reset();
  owner_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnWebSocketDisconnected, this,
                            was_clean, close_code, close_reason));
}

// Currently only called in SocketStream::Finish(), so it is meant
// as an informative message.
// SocketStream code will soon call OnClose after this.
// Note: SocketStream will not call OnClose in some cases with SPDY, but that
// is legacy code, as SPDY is not used in Cobalt.

bool WebSocketImpl::SendHelper(const net::WebSocketFrameHeader::OpCode op_code,
                               const char *data, std::size_t length,
                               std::string * /*error_message*/) {
  scoped_refptr<net::IOBuffer> io_buffer(new net::IOBuffer(length));
  SbMemoryCopy(io_buffer->data(), data, length);

  if (delegate_task_runner_->BelongsToCurrentThread()) {
    SendOnDelegateThread(op_code, std::move(io_buffer), length);
  } else {
    base::Closure do_send_closure(
        base::Bind(&WebSocketImpl::SendOnDelegateThread, this, op_code,
                   std::move(io_buffer), length));
    delegate_task_runner_->PostTask(FROM_HERE, do_send_closure);
  }

  // TODO[***REMOVED***]:Change to void function?
  return true;
}

void WebSocketImpl::SendOnDelegateThread(
    const net::WebSocketFrameHeader::OpCode op_code,
    scoped_refptr<net::IOBuffer> io_buffer, std::size_t length) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  // this behavior is not just an optimization, but required in case
  // we are closing the connection
  auto channel_state =
      websocket_channel_->SendFrame(true /*fin*/, op_code, io_buffer, length);
  if (channel_state == net::WebSocketChannel::CHANNEL_DELETED) {
    websocket_channel_.reset();
  }
}

bool WebSocketImpl::SendText(const char *data, std::size_t length,
                             int *buffered_amount, std::string *error_message) {
  DCHECK(error_message);
  DCHECK(error_message->empty());
  error_message->clear();
  DCHECK(buffered_amount);
  *buffered_amount += static_cast<int>(length);

  return SendHelper(net::WebSocketFrameHeader::kOpCodeText, data, length,
                    error_message);
}

bool WebSocketImpl::SendBinary(const char *data, std::size_t length,
                               int *buffered_amount,
                               std::string *error_message) {
  DCHECK(error_message);
  DCHECK(error_message->empty());
  error_message->clear();
  DCHECK(buffered_amount);
  *buffered_amount += static_cast<int>(length);

  return SendHelper(net::WebSocketFrameHeader::kOpCodeBinary, data, length,
                    error_message);
}

}  // namespace websocket
}  // namespace cobalt
