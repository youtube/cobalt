/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef COBALT_WEBSOCKET_WEB_SOCKET_H_
#define COBALT_WEBSOCKET_WEB_SOCKET_H_

#include <string>

#include "base/optional.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/array_buffer_view.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace websocket {

// This class represents a WebSocket.  It will abide by RFC 6455 "The WebSocket
// Protocol", and implements the The WebSocket API spec at
// https://www.w3.org/TR/websockets/ (as of Jan 2017).
class WebSocket : public dom::EventTarget {
 public:
  // Constants.
  static const uint16 kConnecting = 0;
  static const uint16 kOpen = 1;
  static const uint16 kClosing = 2;
  static const uint16 kClosed = 3;

  // Constructors.
  explicit WebSocket(const std::string& url,
                     script::ExceptionState* exception_state);
  WebSocket(const std::string& url, const std::string& protocols,
            script::ExceptionState* exception_state);

  // Readonly Attributes.
  uint32 buffered_amount() const { return buffered_amount_; }
  uint16 ready_state() const { return ready_state_; }

  std::string extensions() const { return extensions_; }
  std::string protocol() const { return protocol_; }
  std::string url() const { return url_; }

  // Read+Write Attributes.
  std::string binary_type(script::ExceptionState*) { return binary_type_; }
  void set_binary_type(const std::string& binary_type,
                       script::ExceptionState* exception_state);

  // Functions.
  void Close(script::ExceptionState* exception_state);
  void Close(const uint16 code, script::ExceptionState* exception_state);
  void Close(const uint16 code, const std::string& reason,
             script::ExceptionState* exception_state);

  void Send(const std::string& data, script::ExceptionState* exception_state);
  void Send(const scoped_refptr<dom::Blob>& data,
            script::ExceptionState* exception_state);
  void Send(const scoped_refptr<dom::ArrayBuffer>& data,
            script::ExceptionState* exception_state);
  void Send(const scoped_refptr<dom::ArrayBufferView>& data,
            script::ExceptionState* exception_state);

  // EventHandlers.
  const EventListenerScriptObject* onclose() const {
    return GetAttributeEventListener(base::Tokens::close());
  }

  void set_onclose(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::close(), event_listener);
  }

  const EventListenerScriptObject* onerror() const {
    return GetAttributeEventListener(base::Tokens::error());
  }

  void set_onerror(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::error(), event_listener);
  }

  const EventListenerScriptObject* onmessage() const {
    return GetAttributeEventListener(base::Tokens::message());
  }

  void set_onmessage(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::message(), event_listener);
  }

  const EventListenerScriptObject* onopen() const {
    return GetAttributeEventListener(base::Tokens::open());
  }

  void set_onopen(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::open(), event_listener);
  }

  DEFINE_WRAPPABLE_TYPE(WebSocket);

 private:
  // https://www.w3.org/TR/websockets/#dom-websocket-bufferedamount
  uint32 buffered_amount_;

  // https://www.w3.org/TR/websockets/#dom-websocket-readystate
  uint16 ready_state_;

  // https://www.w3.org/TR/websockets/#dom-websocket-binarytype
  std::string binary_type_;
  // https://www.w3.org/TR/websockets/#dom-websocket-extensions
  std::string extensions_;
  // https://www.w3.org/TR/websockets/#dom-websocket-protocol
  std::string protocol_;
  // https://www.w3.org/TR/websockets/#dom-websocket-url
  std::string url_;

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};
}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_WEB_SOCKET_H_
