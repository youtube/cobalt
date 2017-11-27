// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cobalt/network/network_module.h"
#include "cobalt/websocket/buffered_amount_tracker.h"
#include "cobalt/websocket/web_socket_event_interface.h"
#include "cobalt/websocket/web_socket_frame_container.h"
#include "cobalt/websocket/web_socket_handshake_helper.h"
#include "cobalt/websocket/web_socket_message_container.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/websockets/websocket_frame_parser.h"
#include "net/websockets/websocket_job.h"

namespace cobalt {
namespace websocket {

typedef uint16 SerializedCloseStatusCodeType;

// According to https://tools.ietf.org/html/rfc6455#section-5.5.2
// All control frames MUST have a payload length of 125 bytes or less
// and MUST NOT be fragmented.
static const std::size_t kMaxControlPayloadSizeInBytes = 125;
const int kMaxCloseReasonSize =
    kMaxControlPayloadSizeInBytes - sizeof(SerializedCloseStatusCodeType);

class WebSocketImpl : public net::SocketStream::Delegate,
                      public base::RefCountedThreadSafe<WebSocketImpl> {
 public:
  typedef ScopedVector<net::WebSocketFrameChunk> WebSocketFrameChunkVector;

  explicit WebSocketImpl(cobalt::network::NetworkModule* network_module,
                         WebsocketEventInterface* delegate);

  void SetWebSocketEventDelegate(WebsocketEventInterface* delegate);

  // These functions are meant to be called from the Web Module thread.
  void Connect(const std::string& origin, const GURL& url,
               const std::vector<std::string>& sub_protocols);

  // Following functions return false if something went wrong.
  bool SendText(const char* data, std::size_t length, int32* buffered_amount,
                std::string* error_message);
  bool SendBinary(const char* data, std::size_t length, int32* buffered_amount,
                  std::string* error_message);

  void Close(const net::WebSocketError code, const std::string& reason);

  // Following functions are from net::SocketStream::Delegate, and are called on
  // the IO thread.

  void OnConnected(net::SocketStream* socket,
                   int max_pending_send_allowed) override;
  // Called when |amount_sent| bytes of data are sent.
  void OnSentData(net::SocketStream* socket, int amount_sent) override;
  // Called when |len| bytes of |data| are received.
  void OnReceivedData(net::SocketStream* socket, const char* data,
                      int len) override;
  // Called when the socket stream has been closed.
  void OnClose(net::SocketStream* socket) override;

  void OnError(const net::SocketStream* socket, int error) override;

 private:
  struct CloseInfo {
    CloseInfo(const net::WebSocketError code, const std::string& reason)
        : code(code), reason(reason) {}
    explicit CloseInfo(const net::WebSocketError code) : code(code) {}

    net::WebSocketError code;
    std::string reason;
  };

  void DoDetach(base::WaitableEvent* waitable_event);
  void DoClose(const CloseInfo& close_info);
  void DoPong(const scoped_refptr<net::IOBufferWithSize> payload);
  void DoConnect(
      scoped_refptr<cobalt::network::URLRequestContextGetter> context,
      const GURL& url, base::WaitableEvent* job_created_event);
  void SendFrame(const scoped_array<char> data, const int length,
                 const int overhead_bytes);
  void OnHandshakeComplete(const std::string& selected_subprotocol);

  void ProcessCompleteMessage(
      const WebSocketMessageContainer& message_container);
  void ProcessControlMessage(
      const WebSocketMessageContainer& message_container);

  bool ProcessCompleteFrame(WebSocketFrameContainer* frame);

  void HandleClose(const net::WebSocketFrameHeader& header,
                   const scoped_refptr<net::IOBufferWithSize>& close_data);
  void HandlePing(const net::WebSocketFrameHeader& header,
                  const scoped_refptr<net::IOBufferWithSize>& ping_data);
  void HandlePong(const net::WebSocketFrameHeader& header,
                  const scoped_refptr<net::IOBufferWithSize>& pong_data);

  bool SendClose(const net::WebSocketError status_code,
                 const std::string& reason, std::string* error_message);
  bool SendPong(const base::StringPiece payload, std::string* error_message);
  // Note that |payload_length| in |header| will define the payload length.
  bool SendHelper(const net::WebSocketFrameHeader& header, const char* data,
                  std::string* error_message);

  // Returns true if the handshake has been fully processed.
  bool ProcessHandshake(std::size_t* payload_offset);

  void TrampolineClose(const CloseInfo& close_info);

  void OnWebSocketConnected(const std::string& selected_subprotocol);
  void OnWebSocketDisconnected(bool was_clean, uint16 code,
                               const std::string& reason);
  void OnWebSocketSentData(int amount_sent);
  void OnWebSocketReceivedData(bool is_text_frame,
                               scoped_refptr<net::IOBufferWithSize> data);
  void OnWebSocketError();

  base::ThreadChecker thread_checker_;
  net::WebSocketJob::State GetCurrentState() const;

  std::vector<std::string> desired_sub_protocols_;
  network::NetworkModule* network_module_;
  scoped_refptr<net::WebSocketJob> job_;
  WebsocketEventInterface* delegate_;
  std::string origin_;
  GURL connect_url_;
  WebSocketHandshakeHelper handshake_helper_;
  bool handshake_completed_;
  net::WebSocketFrameParser frame_parser_;
  WebSocketFrameContainer current_frame_container_;
  WebSocketMessageContainer current_message_container_;

  base::optional<CloseInfo> peer_close_info_;
  BufferedAmountTracker buffered_amount_tracker_;

  scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> owner_task_runner_;

  virtual ~WebSocketImpl();
  friend class base::RefCountedThreadSafe<WebSocketImpl>;

  DISALLOW_COPY_AND_ASSIGN(WebSocketImpl);
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_WEB_SOCKET_IMPL_H_
