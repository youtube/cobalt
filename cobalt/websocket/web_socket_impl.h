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

#ifndef COBALT_WEBSOCKET_WEB_SOCKET_IMPL_H_
#define COBALT_WEBSOCKET_WEB_SOCKET_IMPL_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cobalt/network/network_module.h"
#include "cobalt/websocket/buffered_amount_tracker.h"
#include "cobalt/websocket/cobalt_web_socket_event_handler.h"
#include "cobalt/websocket/web_socket_frame_container.h"
#include "cobalt/websocket/web_socket_handshake_helper.h"
#include "cobalt/websocket/web_socket_message_container.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_frame_parser.h"
#include "net/websockets/websocket_handshake_stream_create_helper.h"
#include "url/gurl.h"

namespace cobalt {
namespace websocket {
// Owner of WebSocketImpl.
class WebSocket;

typedef uint16 SerializedCloseStatusCodeType;

// According to https://tools.ietf.org/html/rfc6455#section-5.5.2
// All control frames MUST have a payload length of 125 bytes or less
// and MUST NOT be fragmented.
static const std::size_t kMaxControlPayloadSizeInBytes = 125;
const int kMaxCloseReasonSize =
    kMaxControlPayloadSizeInBytes - sizeof(SerializedCloseStatusCodeType);

class WebSocketImpl : public base::RefCountedThreadSafe<WebSocketImpl> {
 public:
  typedef std::vector<std::unique_ptr<net::WebSocketFrameChunk>>
      WebSocketFrameChunkVector;

  explicit WebSocketImpl(cobalt::network::NetworkModule* network_module,
                         WebSocket* delegate);

  void ResetWebSocketEventDelegate();

  // These functions are meant to be called from the Web Module thread.
  void Connect(const std::string& origin, const GURL& url,
               const std::vector<std::string>& sub_protocols);

  // Following functions return false if something went wrong.
  bool SendText(const char* data, std::size_t length, int32* buffered_amount,
                std::string* error_message);
  bool SendBinary(const char* data, std::size_t length, int32* buffered_amount,
                  std::string* error_message);

  void Close(const net::WebSocketError code, const std::string& reason);

  // These are legacy API from old net::SocketStream::Delegate.
  // TODO: investigate if we need to remove these.
  // Called when the socket stream has been closed.
  void OnClose(bool was_clean = true,
               int error_code = net::kWebSocketNormalClosure,
               const std::string& close_reason = std::string());

  void OnWriteDone(uint64_t bytes_written);

  void OnWebSocketReceivedData(bool is_text_frame,
                               scoped_refptr<net::IOBufferWithSize> data);

  void OnHandshakeComplete(const std::string& selected_subprotocol);

  void OnFlowControl(int64_t quota);

  struct CloseInfo {
    CloseInfo(const net::WebSocketError code, const std::string& reason)
        : code(code), reason(reason) {}
    explicit CloseInfo(const net::WebSocketError code) : code(code) {}

    net::WebSocketError code;
    std::string reason;
  };
  void TrampolineClose(const CloseInfo& close_info);

 private:
  void DoClose(const CloseInfo& close_info);
  void SendOnDelegateThread(const net::WebSocketFrameHeader::OpCode op_code,
                            scoped_refptr<net::IOBuffer> io_buffer,
                            std::size_t length);
  void DoConnect(
      scoped_refptr<cobalt::network::URLRequestContextGetter> context,
      const GURL& url, base::WaitableEvent* channel_created_event);

  // Note that |payload_length| in |header| will define the payload length.
  bool SendHelper(const net::WebSocketFrameHeader::OpCode op_code,
                  const char* data, std::size_t length,
                  std::string* error_message);
  void ProcessSendQueue();

  void OnWebSocketConnected(const std::string& selected_subprotocol);
  void OnWebSocketDisconnected(bool was_clean, uint16 code,
                               const std::string& reason);
  void OnWebSocketWriteDone(uint64_t bytes_written);

  void ResetChannel();

  THREAD_CHECKER(thread_checker_);

  std::vector<std::string> desired_sub_protocols_;
  network::NetworkModule* network_module_;
  std::unique_ptr<net::WebSocketChannel> websocket_channel_;
  WebSocket* delegate_;
  std::string origin_;
  GURL connect_url_;

  // Data buffering and flow control.
  // Should only be modified on delegate(network) thread.
  int64_t current_quota_ = 0;
  struct SendQueueMessage {
    scoped_refptr<net::IOBuffer> io_buffer;
    size_t length;
    net::WebSocketFrameHeader::OpCode op_code;
  };
  std::queue<SendQueueMessage> send_queue_;
  size_t sent_size_of_top_message_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> owner_task_runner_;

  ~WebSocketImpl();
  friend class base::RefCountedThreadSafe<WebSocketImpl>;
  friend class WebSocketImplTest;

  DISALLOW_COPY_AND_ASSIGN(WebSocketImpl);
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_WEB_SOCKET_IMPL_H_
