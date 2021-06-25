// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/websocket/cobalt_web_socket_event_handler.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "cobalt/websocket/web_socket_impl.h"

namespace cobalt {
namespace websocket {
namespace {
typedef std::vector<std::pair<scoped_refptr<net::IOBuffer>, size_t>>
    FrameDataVector;
std::size_t GetMessageLength(const FrameDataVector& frame_data) {
  std::size_t total_length = 0;
  for (const auto& i : frame_data) {
    total_length += i.second;
  }
  return total_length;
}
std::size_t CombineFramesChunks(FrameDataVector::const_iterator begin,
                                FrameDataVector::const_iterator end,
                                char* out_destination,
                                std::size_t buffer_length) {
  DCHECK(out_destination);
  std::size_t bytes_written = 0;
  std::size_t bytes_available = buffer_length;
  for (FrameDataVector::const_iterator iterator = begin; iterator != end;
       ++iterator) {
    const scoped_refptr<net::IOBuffer>& data = iterator->first;

    std::size_t frame_chunk_size = iterator->second;

    if (bytes_available >= frame_chunk_size) {
      memcpy(out_destination, data->data(), frame_chunk_size);
      out_destination += frame_chunk_size;
      bytes_written += frame_chunk_size;
      bytes_available -= frame_chunk_size;
    }
  }

  DCHECK_EQ(bytes_written, buffer_length);
  return bytes_written;
}
}  // namespace

void CobaltWebSocketEventHandler::OnAddChannelResponse(
    const std::string& selected_subprotocol, const std::string& extensions) {
  creator_->OnHandshakeComplete(selected_subprotocol);
}
void CobaltWebSocketEventHandler::OnDataFrame(
    bool fin, WebSocketMessageType type, scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size) {
  if (message_type_ == net::WebSocketFrameHeader::kOpCodeControlUnused) {
    message_type_ = type;
  }
  if (type != net::WebSocketFrameHeader::kOpCodeContinuation) {
    DCHECK_EQ(message_type_, type);
  }
  frame_data_.push_back(std::make_pair(std::move(buffer), buffer_size));
  if (fin) {
    std::size_t message_length = GetMessageLength(frame_data_);
    scoped_refptr<net::IOBufferWithSize> buf =
        base::WrapRefCounted(new net::IOBufferWithSize(message_length));
    CombineFramesChunks(frame_data_.begin(), frame_data_.end(), buf->data(),
                        message_length);
    frame_data_.clear();

    bool is_text_message =
        message_type_ == net::WebSocketFrameHeader::kOpCodeText;
    if (is_text_message && buf && (buf->size() > 0)) {
      base::StringPiece payload_string_piece(buf->data(), buf->size());
      if (!base::IsStringUTF8(payload_string_piece)) {
        WebSocketImpl::CloseInfo close_info(net::kWebSocketErrorProtocolError);
        creator_->TrampolineClose(close_info);
        return;
      }
    }
    creator_->OnWebSocketReceivedData(is_text_message, std::move(buf));
  }
}

void CobaltWebSocketEventHandler::OnClosingHandshake() {
  creator_->OnClose(true, net::kWebSocketNormalClosure,
                    "Received close handshake initiation.");
}

void CobaltWebSocketEventHandler::OnFailChannel(const std::string& message) {
  DLOG(WARNING) << "WebSocket channel failed due to: " << message;
  creator_->OnClose(true, net::kWebSocketErrorAbnormalClosure, message);
}

void CobaltWebSocketEventHandler::OnDropChannel(bool was_clean, uint16_t code,
                                                const std::string& reason) {
  creator_->OnClose(was_clean, code, reason);
}

void CobaltWebSocketEventHandler::OnSSLCertificateError(
    std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
        ssl_error_callbacks,
    const GURL& url, const net::SSLInfo& ssl_info, bool fatal) {
  // TODO: determine if there are circumstances we want to continue
  // the request.
  DLOG(WARNING) << "SSL cert failure occured, cancelling connection";
  ssl_error_callbacks->CancelSSLRequest(net::ERR_BAD_SSL_CLIENT_AUTH_CERT,
                                        nullptr);
}
int CobaltWebSocketEventHandler::OnAuthRequired(
    scoped_refptr<net::AuthChallengeInfo> auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const net::HostPortPair& host_port_pair,
    base::OnceCallback<void(const net::AuthCredentials*)> callback,
    base::Optional<net::AuthCredentials>* credentials) {
  NOTIMPLEMENTED();
  return net::OK;
}

void CobaltWebSocketEventHandler::OnWriteDone(uint64_t bytes_written) {
  creator_->OnWriteDone(bytes_written);
}

void CobaltWebSocketEventHandler::OnFlowControl(int64_t quota) {
  creator_->OnFlowControl(quota);
}

}  // namespace websocket
}  // namespace cobalt
