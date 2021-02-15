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

#ifndef COBALT_WEBSOCKET_COBALT_WEB_SOCKET_EVENT_HANDLER_H_
#define COBALT_WEBSOCKET_COBALT_WEB_SOCKET_EVENT_HANDLER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/websocket/web_socket_frame_container.h"
#include "cobalt/websocket/web_socket_handshake_helper.h"
#include "net/base/io_buffer.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_frame_parser.h"

namespace cobalt {
namespace websocket {

class WebSocketImpl;

// This is the event interface that net takes to process all the events that
// happen during websocket connections.
// The lifetime of a connection process sometimes is shorter than the websocket
// so it has to be another class different from WebSocketChannel's creator
// which is our WebSocketImpl.
class CobaltWebSocketEventHandler : public net::WebSocketEventInterface {
 public:
  explicit CobaltWebSocketEventHandler(WebSocketImpl* creator)
      : creator_(creator) {
    DCHECK(creator);
  }

  // Called when a URLRequest is created for handshaking.
  void OnCreateURLRequest(net::URLRequest* request) override {}

  // Called in response to an AddChannelRequest. This means that a response has
  // been received from the remote server.
  void OnAddChannelResponse(const std::string& selected_subprotocol,
                            const std::string& extensions) override;

  // Called when a data frame has been received from the remote host and needs
  // to be forwarded to the renderer process.
  void OnDataFrame(bool fin, WebSocketMessageType type,
                   scoped_refptr<net::IOBuffer> buffer,
                   size_t buffer_size) override;

  // Called to provide more send quota for this channel to the renderer
  // process. Currently the quota units are always bytes of message body
  // data. In future it might depend on the type of multiplexing in use.
  void OnFlowControl(int64_t quota) override;

  // Called when the remote server has Started the WebSocket Closing
  // Handshake. The client should not attempt to send any more messages after
  // receiving this message. It will be followed by OnDropChannel() when the
  // closing handshake is complete.
  void OnClosingHandshake() override;

  // Called when the channel has been dropped, either due to a network close, a
  // network error, or a protocol error. This may or may not be preceded by a
  // call to OnClosingHandshake().
  //
  // Warning: Both the |code| and |reason| are passed through to Javascript, so
  // callers must take care not to provide details that could be useful to
  // attackers attempting to use WebSocketMessageType to probe networks.
  //
  // |was_clean| should be true if the closing handshake completed successfully.
  //
  // The channel should not be used again after OnDropChannel() has been
  // called.
  //
  // This function deletes the Channel.
  void OnDropChannel(bool was_clean, uint16_t code,
                     const std::string& reason) override;

  // Called when the browser fails the channel, as specified in the spec.
  //
  // The channel should not be used again after OnFailChannel() has been
  // called.
  //
  // This function deletes the Channel.
  void OnFailChannel(const std::string& message) override;

  // Called when the browser starts the WebSocket Opening Handshake.
  void OnStartOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) override {}

  // Called when the browser finishes the WebSocket Opening Handshake.
  void OnFinishOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) override {}

  // Called on SSL Certificate Error during the SSL handshake. Should result in
  // a call to either ssl_error_callbacks->ContinueSSLRequest() or
  // ssl_error_callbacks->CancelSSLRequest(). Normally the implementation of
  // this method will delegate to content::SSLManager::OnSSLCertificateError to
  // make the actual decision. The callbacks must not be called after the
  // WebSocketChannel has been destroyed.
  void OnSSLCertificateError(
      std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
          ssl_error_callbacks,
      const GURL& url, const net::SSLInfo& ssl_info, bool fatal) override;

  // Called when authentication is required. Returns a net error. The opening
  // handshake is blocked when this function returns ERR_IO_PENDING.
  // In that case calling |callback| resumes the handshake. |callback| can be
  // called during the opening handshake. An implementation can rewrite
  // |*credentials| (in the sync case) or provide new credentials (in the
  // async case).
  // Providing null credentials (nullopt in the sync case and nullptr in the
  // async case) cancels authentication. Otherwise the new credentials are set
  // and the opening handshake will be retried with the credentials.
  int OnAuthRequired(
      scoped_refptr<net::AuthChallengeInfo> auth_info,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      const net::HostPortPair& host_port_pair,
      base::OnceCallback<void(const net::AuthCredentials*)> callback,
      base::Optional<net::AuthCredentials>* credentials) override;

  // Called when a write completes, and |bytes_written| indicates how many bytes
  // were written.
  void OnWriteDone(uint64_t bytes_written) override;

 protected:
  CobaltWebSocketEventHandler() {}

 private:
  // For now, we forward most event to the impl to handle. We might want to
  // move more thing
  WebSocketImpl* creator_;
  // This vector should store data of data frames from the same message.
  typedef std::vector<std::pair<scoped_refptr<net::IOBuffer>, size_t>>
      FrameDataVector;
  FrameDataVector frame_data_;
  WebSocketMessageType message_type_ =
      net::WebSocketFrameHeader::kOpCodeControlUnused;
  DISALLOW_COPY_AND_ASSIGN(CobaltWebSocketEventHandler);
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_COBALT_WEB_SOCKET_EVENT_HANDLER_H_
