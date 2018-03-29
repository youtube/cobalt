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

#include "cobalt/websocket/web_socket_impl.h"

#include <algorithm>
#include <cstdint>

#include "base/basictypes.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/websocket/web_socket_message_container.h"
#include "net/base/big_endian.h"
#include "net/http/http_util.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_handshake_handler.h"
#include "net/websockets/websocket_job.h"
#include "starboard/memory.h"

namespace {

using cobalt::websocket::WebSocketFrameContainer;
using cobalt::websocket::WebSocketImpl;
bool AreAnyReservedBitsSet(const net::WebSocketFrameHeader &header) {
  return (header.reserved1 || header.reserved2 || header.reserved3);
}

bool IsValidOpCode(net::WebSocketFrameHeader::OpCode op_code) {
  return ((op_code >= net::WebSocketFrameHeader::kOpCodeContinuation) &&
          (op_code <= net::WebSocketFrameHeader::kOpCodeBinary)) ||
         ((op_code >= net::WebSocketFrameHeader::kOpCodeClose) &&
          (op_code <= net::WebSocketFrameHeader::kOpCodePong));
}

}  // namespace

namespace cobalt {
namespace websocket {

WebSocketImpl::WebSocketImpl(cobalt::network::NetworkModule *network_module,
                             WebsocketEventInterface *delegate)
    : network_module_(network_module),
      delegate_(delegate),
      handshake_helper_(network_module ? network_module->GetUserAgent() : ""),
      handshake_completed_(false) {
  DCHECK(delegate_);
  DCHECK(MessageLoop::current());
  net::WebSocketJob::EnsureInit();
  owner_task_runner_ = MessageLoop::current()->message_loop_proxy();
}

void WebSocketImpl::SetWebSocketEventDelegate(
    WebsocketEventInterface *delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_ = delegate;
}

void WebSocketImpl::Connect(const std::string &origin, const GURL &url,
                            const std::vector<std::string> &sub_protocols) {
  if (!network_module_) {
    return;
  }
  DCHECK(network_module_->url_request_context_getter());
  thread_checker_.CalledOnValidThread();
  origin_ = origin;

  DLOG(INFO) << "Connecting to websocket at " << url.spec();

  connect_url_ = url;
  desired_sub_protocols_ = sub_protocols;

  // TODO: Network Task Runner might not be the optimal task runner.
  // To achieve low latency communication via websockets, a dedicated high
  // priority thread might be required.  Investigation is needed.
  delegate_task_runner_ =
      network_module_->url_request_context_getter()->GetNetworkTaskRunner();
  base::WaitableEvent socket_stream_job_created(true, false);
  base::Closure create_socket_stream_closure(
      base::Bind(&WebSocketImpl::DoConnect, this,
                 network_module_->url_request_context_getter(), url,
                 base::Unretained(&socket_stream_job_created)));
  delegate_task_runner_->PostTask(FROM_HERE, create_socket_stream_closure);

  // Wait for the job to be completed.  This event is signaled after job_ has
  // been assigned.
  socket_stream_job_created.Wait();
}

void WebSocketImpl::DoConnect(
    scoped_refptr<cobalt::network::URLRequestContextGetter> context,
    const GURL &url, base::WaitableEvent *job_created_event) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(url.is_valid());
  DCHECK(job_created_event);
  DCHECK(context->GetURLRequestContext());
  DCHECK(!job_);

  job_ = base::polymorphic_downcast<net::WebSocketJob *>(
      net::SocketStreamJob::CreateSocketStreamJob(url, this, NULL, NULL));
  DCHECK(job_);
  job_created_event->Signal();  // Signal that this->job_ has been assigned.

  job_->set_context(context->GetURLRequestContext());
  job_->set_network_task_runner(context->GetNetworkTaskRunner());
  job_->Connect();

  DCHECK_EQ(GetCurrentState(), net::WebSocketJob::CONNECTING);
}

void WebSocketImpl::Close(const net::WebSocketError code,
                          const std::string &reason) {
  DCHECK(job_);
  CloseInfo close_info(code, reason);
  delegate_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebSocketImpl::DoClose, this, close_info));
}

void WebSocketImpl::DoClose(const CloseInfo &close_info) {
  DCHECK(job_);
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  net::WebSocketJob::State current_state = GetCurrentState();
  switch (current_state) {
    case net::WebSocketJob::CONNECTING:
    case net::WebSocketJob::OPEN: {
      // Write the close frame
      std::string error_message;
      if (!SendClose(close_info.code, close_info.reason, &error_message)) {
        DLOG(ERROR) << "Error while sending websocket close: " << error_message;
      }
      net::WebSocketJob::State new_state;
      if (peer_close_info_) {
        // We have received a Close frame from the peer.
        new_state = net::WebSocketJob::RECV_CLOSED;
      } else {
        // We have not received a Close frame from the peer.
        new_state = net::WebSocketJob::SEND_CLOSED;
      }
      job_->SetState(new_state);
      job_->Close();
      break;
    }
    case net::WebSocketJob::SEND_CLOSED:
      if (peer_close_info_) {
        // Closing handshake is now complete.
        job_->SetState(net::WebSocketJob::CLOSE_WAIT);
        job_->Close();
      }
    case net::WebSocketJob::CLOSE_WAIT:
    case net::WebSocketJob::CLOSED:
    case net::WebSocketJob::INITIALIZED:
    case net::WebSocketJob::RECV_CLOSED:
      break;
  }
}

void WebSocketImpl::DoPong(const scoped_refptr<net::IOBufferWithSize> payload) {
  DCHECK(job_);
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  net::WebSocketJob::State current_state = GetCurrentState();

  if (!((current_state == net::WebSocketJob::CONNECTING) ||
        (current_state == net::WebSocketJob::OPEN))) {
    return;
  }

  std::string error_message;
  base::StringPiece payload_data(NULL, 0);
  if (payload) {
    payload_data = base::StringPiece(payload->data(), payload->size());
  }

  if (!SendPong(payload_data, &error_message)) {
    DLOG(ERROR) << "Error while sending websocket pong: " << error_message;
  }
}

void WebSocketImpl::DoDetach(base::WaitableEvent *waitable_event) {
  DCHECK(waitable_event);
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  if (job_) {
    job_->DetachDelegate();
  }
  waitable_event->Signal();
  // After call to Signal() in the line above, |this| likely has been deleted.
}

WebSocketImpl::~WebSocketImpl() {
  if (job_) {
    base::WaitableEvent wait_for_detach(true, false);

    // Call using base::Unretained, so that we can finish destroying
    // the object, and we know that there will be no attempt at ref-counting
    // by the task we post.
    delegate_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WebSocketImpl::DoDetach, base::Unretained(this),
                              base::Unretained(&wait_for_detach)));
    wait_for_detach.Wait();
  }
}

bool WebSocketImpl::ProcessCompleteFrame(
    WebSocketFrameContainer *frame_container) {
  if (!frame_container) {
    NOTREACHED() << "frame_container must not be NULL.";
    return false;
  }

  WebSocketFrameContainer &frame(*frame_container);

  if (frame.IsControlFrame()) {
    WebSocketMessageContainer control_message;
    control_message.Take(&frame);
    ProcessControlMessage(control_message);
    // Note that the chunks are freed here by the message destructor,
    // since we do not need them any longer.
  } else if (frame.IsDataFrame() || frame.IsContinuationFrame()) {
    if (!current_message_container_.Take(&frame)) {
      return false;
    }
  } else {
    NOTREACHED() << "Frame must be a (continued) data frame or a control frame";
    return false;
  }

  // Yay, we have the final frame of the message.
  if (current_message_container_.IsMessageComplete()) {
    ProcessCompleteMessage(current_message_container_);
    current_message_container_.clear();
  }

  return true;
}

void WebSocketImpl::OnReceivedData(net::SocketStream *socket, const char *data,
                                   int len) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  UNREFERENCED_PARAMETER(socket);
  if (len <= 0) {
    NOTREACHED() << "Invalid length.";
    return;
  }

  std::size_t payload_offset = 0;
  if (!ProcessHandshake(&payload_offset)) {
    return;  // Handshake is still in progress.
  }
  if (payload_offset >= static_cast<std::size_t>(len)) {
    return;
  }

  WebSocketFrameChunkVector frame_chunks;
  std::size_t payload_size = len - payload_offset;
  bool successful_decode =
      frame_parser_.Decode(data + payload_offset, payload_size, &frame_chunks);
  if (!successful_decode) {
    net::WebSocketError websocket_error = frame_parser_.websocket_error();
    DLOG(INFO) << "Error while decoding websocket: " << websocket_error;
    return;
  }

  bool protocol_violation_occured = false;

  WebSocketFrameChunkVector::iterator iterator = frame_chunks.begin();
  while (iterator != frame_chunks.end()) {
    net::WebSocketFrameChunk *chunk = *iterator;
    DCHECK(chunk);
    if (!chunk) {
      break;
    }
    const net::WebSocketFrameHeader *const header = chunk->header.get();

    if (header) {
      net::WebSocketFrameHeader::OpCode op_code = header->opcode;
      if (AreAnyReservedBitsSet(*header) || !IsValidOpCode(op_code)) {
        break;
      }

      if (op_code == net::WebSocketFrameHeader::kOpCodeContinuation) {
        if (current_message_container_.empty()) {
          // If we get a continuation frame, there should have been a previous
          // data frame.  This data frame should be stored in
          // |current_message_container_|.
          break;
        }
      }
    }

    // Note that |MoveInto| transfers ownership of the pointer.
    WebSocketFrameContainer::ErrorCode move_into_retval =
        current_frame_container_.Take(*iterator);
    // Increment iterator, since ownership is transfered.
    ++iterator;

    if (move_into_retval != WebSocketFrameContainer::kErrorNone) {
      break;
    }

    if (current_frame_container_.IsFrameComplete()) {
      // Move chunks from frame into the message.

      if (!ProcessCompleteFrame(&current_frame_container_)) {
        protocol_violation_occured = true;

        // Note that |protocol_violation_occured| variable is needed in case
        // |iterator == frame_chunks.end()|.
        break;
      }
    }
  }  // end of while().

  // If we exited earlier from the while loop because peer was behaving badly,
  // then we know |iterator| != |frame_chunks.end()|.  So close the connection,
  // and free unprocessed frames.
  protocol_violation_occured |= (iterator != frame_chunks.end());

  if (protocol_violation_occured) {
    CloseInfo close_info(net::kWebSocketErrorProtocolError);
    TrampolineClose(close_info);
    frame_chunks.erase(iterator, frame_chunks.end());
  }

  frame_chunks.weak_clear();
}

// The main reason to call TrampolineClose is to ensure messages that are posted
// from this thread prior to this function call are processed before the
// connection is closed.
void WebSocketImpl::TrampolineClose(const CloseInfo &close_info) {
  base::Closure no_op_closure(base::Bind(&base::DoNothing));

  base::Closure do_close_closure(
      base::Bind(&WebSocketImpl::DoClose, this, close_info));
  owner_task_runner_->PostTaskAndReply(FROM_HERE, no_op_closure,
                                       do_close_closure);
}

void WebSocketImpl::OnWebSocketConnected(
    const std::string &selected_subprotocol) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_) {
    delegate_->OnConnected(selected_subprotocol);
  }
}

void WebSocketImpl::OnWebSocketDisconnected(bool was_clean, uint16 code,
                                            const std::string &reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_) {
    delegate_->OnDisconnected(was_clean, code, reason);
  }
}

void WebSocketImpl::OnWebSocketSentData(int amount_sent) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_) {
    delegate_->OnSentData(amount_sent);
  }
}

void WebSocketImpl::OnWebSocketReceivedData(
    bool is_text_frame, scoped_refptr<net::IOBufferWithSize> data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_) {
    delegate_->OnReceivedData(is_text_frame, data);
  }
}

void WebSocketImpl::ProcessCompleteMessage(
    const WebSocketMessageContainer &message_container) {
  if (message_container.GetCurrentPayloadSizeBytes() >
      kMaxMessagePayloadInBytes) {
    // Receiving more than kMaxMessagePayloadInBytes is not supported.
    std::stringstream ss;

    ss << "Received a frame with payload more than "
       << kMaxMessagePayloadInBytes
       << " of data. This is above the supported size.  Closing connection.";
    std::string error_message = ss.str();
    std::string send_error_message;
    if (!SendClose(net::kWebSocketErrorMessageTooBig, error_message,
                   &send_error_message)) {
      DLOG(ERROR) << "Error while sending a Close message: "
                  << send_error_message;
    }

    return;
  }

  scoped_refptr<net::IOBufferWithSize> buf =
      message_container.GetMessageAsIOBuffer();

  bool is_text_message = message_container.IsTextMessage();
  if (is_text_message && buf && (buf->size() > 0)) {
    base::StringPiece payload_string_piece(buf->data(), buf->size());
    if (!IsStringUTF8(payload_string_piece)) {
      CloseInfo close_info(net::kWebSocketErrorProtocolError);
      TrampolineClose(close_info);
      return;
    }
  }

  owner_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnWebSocketReceivedData, this,
                            is_text_message, buf));
}

void WebSocketImpl::ProcessControlMessage(
    const WebSocketMessageContainer &message_container) {
  if (message_container.empty()) {
    CloseInfo close_info(net::kWebSocketErrorInternalServerError,
                         "Message has no frames.");
    DoClose(close_info);
    NOTREACHED();
    return;
  }

  if (message_container.GetCurrentPayloadSizeBytes() >
      kMaxControlPayloadSizeInBytes) {
    CloseInfo close_info(net::kWebSocketErrorProtocolError,
                         "Control Frame too large");
    DoClose(close_info);
    return;
  }

  const WebSocketMessageContainer::WebSocketFrames &frames =
      message_container.GetFrames();
  std::size_t number_of_frames = frames.size();

  if (number_of_frames > 0) {
    const net::WebSocketFrameHeader *header = frames.begin()->GetHeader();
    if ((number_of_frames > 1) || (header && !header->final)) {
      CloseInfo close_info(net::kWebSocketErrorProtocolError,
                           "Control messages must not be fragmented");
      DoClose(close_info);
      return;
    }
  }

  scoped_refptr<net::IOBufferWithSize> buf =
      message_container.GetMessageAsIOBuffer();
  const WebSocketFrameContainer &frame_container =
      *(message_container.GetFrames().begin());

  const net::WebSocketFrameHeader *const header_pointer =
      frame_container.GetHeader();

  switch (header_pointer->opcode) {
    case net::WebSocketFrameHeader::kOpCodeClose:
      HandleClose(*header_pointer, buf);
      break;
    case net::WebSocketFrameHeader::kOpCodePing:
      HandlePing(*header_pointer, buf);
      break;
    case net::WebSocketFrameHeader::kOpCodePong:
      HandlePong(*header_pointer, buf);
      break;
    case net::WebSocketFrameHeader::kOpCodeContinuation:
    case net::WebSocketFrameHeader::kOpCodeText:
    case net::WebSocketFrameHeader::kOpCodeBinary:
      NOTREACHED() << "Invalid case " << header_pointer->opcode;

      CloseInfo close_info(net::kWebSocketErrorInternalServerError,
                           "Invalid op code.");
      DoClose(close_info);
      break;
  }
}

void WebSocketImpl::HandlePing(
    const net::WebSocketFrameHeader &header,
    const scoped_refptr<net::IOBufferWithSize> &ping_data) {
  DCHECK_EQ(header.opcode, net::WebSocketFrameHeader::kOpCodePing);
  if (ping_data || (header.payload_length > 0)) {
    DCHECK_EQ(header.payload_length, ping_data->size());
  }

  base::Closure waste(base::Bind(&base::DoNothing));
  base::Closure do_pong_closure(
      base::Bind(&WebSocketImpl::DoPong, this, ping_data));
  owner_task_runner_->PostTaskAndReply(FROM_HERE, waste, do_pong_closure);
}

void WebSocketImpl::HandlePong(
    const net::WebSocketFrameHeader &header,
    const scoped_refptr<net::IOBufferWithSize> &pong_data) {
  DCHECK_EQ(header.opcode, net::WebSocketFrameHeader::kOpCodePong);
  if (pong_data || (header.payload_length > 0)) {
    DCHECK_EQ(header.payload_length, pong_data->size());
  }
}

void WebSocketImpl::HandleClose(
    const net::WebSocketFrameHeader &header,
    const scoped_refptr<net::IOBufferWithSize> &close_data) {
  net::WebSocketError outgoing_status_code = net::kWebSocketNormalClosure;
  std::size_t payload_length = 0;
  std::string close_reason;
  if (close_data) {
    DCHECK_EQ(header.payload_length, close_data->size());
    DCHECK(close_data->data());
    payload_length = close_data->size();
  }

  peer_close_info_ = CloseInfo(net::kWebSocketErrorNoStatusReceived);

  if (payload_length == 0) {
    // default status code of normal is OK.
  } else if (payload_length < 2) {
    outgoing_status_code = net::kWebSocketErrorProtocolError;
  } else {
    SerializedCloseStatusCodeType status_code_on_wire;

    const char *payload_pointer = close_data->data();
    if (payload_pointer && payload_length >= sizeof(status_code_on_wire)) {
      // https://tools.ietf.org/html/rfc6455#section-5.5.1 says
      // "When sending a Close frame in response, the endpoint
      // typically echos the status code it received.", but
      // Autobahn's test suite does not like this.
      // Also Chrome just sends kWebSocketNormalClosure, so, Cobalt
      // will mimic that behavior.
      net::ReadBigEndian(payload_pointer, &status_code_on_wire);
      payload_pointer += sizeof(status_code_on_wire);
      DCHECK_GE(payload_length, sizeof(status_code_on_wire));
      payload_length -= sizeof(status_code_on_wire);

      if (net::IsValidCloseStatusCode(status_code_on_wire)) {
        DLOG(INFO) << "Websocket received close status code: "
                   << status_code_on_wire;
        peer_close_info_->code = net::WebSocketError(status_code_on_wire);
      } else {
        DLOG(ERROR) << "Websocket received invalid close status code: "
                    << outgoing_status_code;
        outgoing_status_code = net::kWebSocketErrorProtocolError;
      }

      close_reason.assign(payload_pointer, payload_length);
      if (payload_length > 0) {
        if (IsStringUTF8(close_reason)) {
          DLOG(INFO) << "Websocket close reason [" << close_reason << "]";
          peer_close_info_->reason = close_reason;
        } else {
          outgoing_status_code = net::kWebSocketErrorProtocolError;
        }
      }
    }
  }

  CloseInfo outgoing_close_info(outgoing_status_code, close_reason);
  TrampolineClose(outgoing_close_info);
}

void WebSocketImpl::OnSentData(net::SocketStream *socket, int amount_sent) {
  UNREFERENCED_PARAMETER(socket);
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  std::size_t payload_sent = buffered_amount_tracker_.Pop(amount_sent);
  DCHECK_GE(payload_sent, 0ul);
  DCHECK_LE(payload_sent, static_cast<unsigned int>(kint32max));

  owner_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnWebSocketSentData, this,
                            static_cast<int>(payload_sent)));
}

void WebSocketImpl::OnClose(net::SocketStream *socket) {
  UNREFERENCED_PARAMETER(socket);
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  bool close_was_clean = false;
  std::uint16_t close_code =
      static_cast<std::uint16_t>(net::kWebSocketErrorAbnormalClosure);
  std::string close_reason;

  if (peer_close_info_) {
    close_was_clean = true;
    close_code = static_cast<std::uint16_t>(peer_close_info_->code);
    close_reason = peer_close_info_->reason;
  }

  DLOG(INFO) << "WebSocket is closing. was_clean[" << std::boolalpha
             << close_was_clean << "] code[" << close_code << "] reason["
             << close_reason << "]";

  owner_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnWebSocketDisconnected, this,
                            close_was_clean, close_code, close_reason));
}

// Currently only called in SocketStream::Finish(), so it is meant
// as an informative message.
// SocketStream code will soon call OnClose after this.
// Note: SocketStream will not call OnClose in some cases with SPDY, but that
// is legacy code, as SPDY is not used in Cobalt.
void WebSocketImpl::OnError(const net::SocketStream *socket, int error) {
  UNREFERENCED_PARAMETER(socket);
  UNREFERENCED_PARAMETER(error);
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
}

void WebSocketImpl::OnConnected(net::SocketStream *socket,
                                int max_pending_send_allowed) {
  UNREFERENCED_PARAMETER(socket);
  UNREFERENCED_PARAMETER(max_pending_send_allowed);
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  net::WebSocketJob::State current_state = GetCurrentState();
  DCHECK_EQ(current_state, net::WebSocketJob::CONNECTING);

  std::string header_string;
  handshake_helper_.GenerateHandshakeRequest(
      connect_url_, origin_, desired_sub_protocols_, &header_string);

  buffered_amount_tracker_.Add(false, header_string.size());

  job_->SendData(header_string.data(), static_cast<int>(header_string.size()));
}

void WebSocketImpl::OnHandshakeComplete(
    const std::string &selected_subprotocol) {
  owner_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebSocketImpl::OnWebSocketConnected, this,
                            selected_subprotocol));
}

// Note that |payload_length| in |header| will define the payload length.
bool WebSocketImpl::SendHelper(const net::WebSocketFrameHeader &header,
                               const char *data, std::string *error_message) {
  DCHECK(error_message);
  const uint64 payload_length = header.payload_length;
  int int_header_size = GetWebSocketFrameHeaderSize(header);
  DCHECK_GT(int_header_size, 0);
  const uint64 header_size = std::max(0, int_header_size);
  const uint64 frame_size = header_size + payload_length;

  if (frame_size > kMaxMessagePayloadInBytes) {
    *error_message = "Message size is too big.  Must be less than " +
                     base::IntToString(kMaxMessagePayloadInBytes) + " bytes.";
    return false;
  }

  scoped_array<char> data_ptr(new char[frame_size]);
  if (!data_ptr.get()) {
    *error_message = "Unable to allocate memory";
    return false;
  }

  net::WebSocketMaskingKey masking_key = net::GenerateWebSocketMaskingKey();
  int application_payload_offset = net::WriteWebSocketFrameHeader(
      header, &masking_key, data_ptr.get(), static_cast<int>(frame_size));

  DCHECK_EQ(application_payload_offset + payload_length, frame_size);
  if (payload_length != 0) {
    char *payload_offset = data_ptr.get() + application_payload_offset;
    COMPILE_ASSERT(
        kMaxFramePayloadInBytes < static_cast<std::size_t>(kint32max),
        frame_payload_size_too_big);
    SbMemoryCopy(payload_offset, data, static_cast<int>(payload_length));
    net::MaskWebSocketFramePayload(masking_key, 0, payload_offset,
                                   static_cast<int>(payload_length));
  }

  int overhead_bytes = static_cast<int>(frame_size);
  if ((header.opcode == net::WebSocketFrameHeader::kOpCodeText) ||
      (header.opcode == net::WebSocketFrameHeader::kOpCodeBinary)) {
    // Only consider text and binary frames as "payload".
    overhead_bytes = application_payload_offset;
  }
  if (delegate_task_runner_->BelongsToCurrentThread()) {
    // this behavior is not just an optimization, but required in case
    // we are closing the connection
    SendFrame(data_ptr.Pass(), static_cast<int>(frame_size), overhead_bytes);
  } else {
    base::Closure do_send_closure(base::Bind(
        &WebSocketImpl::SendFrame, this, base::Passed(data_ptr.Pass()),
        static_cast<int>(frame_size), overhead_bytes));
    delegate_task_runner_->PostTask(FROM_HERE, do_send_closure);
  }

  return true;
}

bool WebSocketImpl::ProcessHandshake(std::size_t *payload_offset) {
  if (handshake_completed_) {
    return true;
  }
  DCHECK(payload_offset);
  *payload_offset = 0;
  // OnReceivedData is only called after all of the response headers have been
  // received.
  net::WebSocketHandshakeResponseHandler *response_handler =
      job_->GetHandshakeResponse();
  if (!response_handler) {
    NOTREACHED() << "ResponseHandler was null in WebSocketImpl::OnReceivedData";
    return false;
  }
  // This call back should only be called if we have a full response.
  if (!response_handler->HasResponse()) {
    NOTREACHED() << "ResponseHandler says we do not have a full response.";
    return false;
  }

  *payload_offset = response_handler->GetRawResponseLength();

  std::string response_headers_string = response_handler->GetRawResponse();
  std::string response_headers_formatted = net::HttpUtil::AssembleRawHeaders(
      response_headers_string.data(),
      static_cast<int>(response_headers_string.size()));

  scoped_refptr<net::HttpResponseHeaders> response_headers = make_scoped_refptr(
      new net::HttpResponseHeaders(response_headers_formatted));

  std::string error_message;
  if (handshake_helper_.IsResponseValid(*response_headers, &error_message)) {
    const std::string &selected_subprotocol(
        handshake_helper_.GetSelectedSubProtocol());
    OnHandshakeComplete(selected_subprotocol);
    handshake_completed_ = true;
    DCHECK_EQ(GetCurrentState(), net::WebSocketJob::OPEN);
    DLOG(INFO) << "Websocket connected successfully";
    return true;
  } else {
    DLOG(ERROR) << "Handshake response is invalid: " << error_message;
    // Something is wrong, let's shutdown.
    CloseInfo close_info(net::kWebSocketErrorProtocolError);
    DoClose(close_info);
  }

  return false;
}

net::WebSocketJob::State WebSocketImpl::GetCurrentState() const {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(job_);
  if (job_) {
    return job_->state();
  } else {
    NOTREACHED() << "GetCurrentState should only be called after job_ has been "
                    "initialized.";
    return net::WebSocketJob::INITIALIZED;
  }
}

bool WebSocketImpl::SendText(const char *data, std::size_t length,
                             int *buffered_amount, std::string *error_message) {
  DCHECK(error_message);
  DCHECK(error_message->empty());
  error_message->clear();
  DCHECK(buffered_amount);

  net::WebSocketFrameHeader header;
  header.final = true;
  header.reserved1 = false;
  header.reserved2 = false;
  header.reserved3 = false;
  header.opcode = net::WebSocketFrameHeader::kOpCodeText;
  header.masked = true;
  header.payload_length = length;

  *buffered_amount += static_cast<int>(length);

  return SendHelper(header, data, error_message);
}

bool WebSocketImpl::SendBinary(const char *data, std::size_t length,
                               int *buffered_amount,
                               std::string *error_message) {
  DCHECK(error_message);
  DCHECK(error_message->empty());
  error_message->clear();
  DCHECK(buffered_amount);

  net::WebSocketFrameHeader header;
  header.final = true;
  header.reserved1 = false;
  header.reserved2 = false;
  header.reserved3 = false;
  header.opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  header.masked = true;
  header.payload_length = length;

  *buffered_amount += static_cast<int>(length);

  return SendHelper(header, data, error_message);
}

bool WebSocketImpl::SendClose(const net::WebSocketError status_code,
                              const std::string &reason,
                              std::string *error_message) {
  DCHECK(error_message);

  net::WebSocketFrameHeader header;
  header.final = true;
  header.reserved1 = false;
  header.reserved2 = false;
  header.reserved3 = false;
  header.opcode = net::WebSocketFrameHeader::kOpCodeClose;
  header.masked = true;
  header.payload_length = 0;

  if (reason.empty()) {
    SerializedCloseStatusCodeType payload;
    char *payload_pointer = reinterpret_cast<char *>(&payload);
    header.payload_length = sizeof(payload);
    DCHECK(net::IsValidCloseStatusCode(status_code));
    net::WriteBigEndian(
        payload_pointer,
        static_cast<SerializedCloseStatusCodeType>(status_code));
    return SendHelper(header, payload_pointer, error_message);
  }

  COMPILE_ASSERT(kMaxCloseReasonSize < kint32max, close_reason_size_too_big);
  COMPILE_ASSERT(kMaxCloseReasonSize > 0, close_reason_size_too_small);
  // The int cast is safe to do since kMaxCloseReasonSize is a small positive
  // integer.
  int reason_size = static_cast<int>(
      std::min(reason.size(), static_cast<std::size_t>(kMaxCloseReasonSize)));
  header.payload_length = sizeof(SerializedCloseStatusCodeType) + reason_size;
  std::string payload;
  // Safe due to the COMPILE_ASSERT few lines above.
  payload.reserve(static_cast<int>(header.payload_length));
  payload.resize(sizeof(SerializedCloseStatusCodeType));

  char *payload_pointer = &payload[0];
  net::WriteBigEndian(payload_pointer,
                      static_cast<SerializedCloseStatusCodeType>(status_code));
  payload.append(reason.data(), reason_size);
  return SendHelper(header, payload_pointer, error_message);
}

bool WebSocketImpl::SendPong(base::StringPiece payload,
                             std::string *error_message) {
  DCHECK(error_message);
  if (payload.size() > kMaxControlPayloadSizeInBytes) {
    // According to https://tools.ietf.org/html/rfc6455#section-5.5.2
    // "All control frames MUST have a payload length of 125 bytes or less
    // and MUST NOT be fragmented."
    std::stringstream ss;
    ss << "Pong payload size " << payload.size() << " is too big.";
    *error_message = ss.str();
    return false;
  }

  net::WebSocketFrameHeader header;
  header.final = true;
  header.reserved1 = false;
  header.reserved2 = false;
  header.reserved3 = false;
  header.opcode = net::WebSocketFrameHeader::kOpCodePong;
  header.masked = true;
  header.payload_length = payload.size();
  return SendHelper(header, payload.data(), error_message);
}

void WebSocketImpl::SendFrame(const scoped_array<char> data, const int length,
                              const int overhead_bytes) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  DCHECK(data);
  DCHECK_GE(length, 0);
  DCHECK_GE(length, overhead_bytes);

  buffered_amount_tracker_.Add(false, overhead_bytes);
  int user_payload_bytes = length - overhead_bytes;
  if (user_payload_bytes > 0) {
    buffered_amount_tracker_.Add(true, user_payload_bytes);
  }

  job_->SendData(data.get(), length);
}

}  // namespace websocket
}  // namespace cobalt
