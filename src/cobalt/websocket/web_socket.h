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
#ifndef COBALT_WEBSOCKET_WEB_SOCKET_H_
#define COBALT_WEBSOCKET_WEB_SOCKET_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/base/compiler.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/array_buffer_view.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/message_event.h"
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
  WebSocket(script::EnvironmentSettings* settings, const std::string& url,
            script::ExceptionState* exception_state);
  WebSocket(script::EnvironmentSettings* settings, const std::string& url,
            const std::string& protocol,
            script::ExceptionState* exception_state);
  WebSocket(script::EnvironmentSettings* settings, const std::string& url,
            const std::vector<std::string>& sub_protocols,
            script::ExceptionState* exception_state);

  // Readonly Attributes.
  uint32 buffered_amount() const { return buffered_amount_; }
  uint16 ready_state() const { return ready_state_; }

  const std::string& extensions() const { return extensions_; }
  const std::string& protocol() const { return protocol_; }
  const std::string& url() const {
    return resolved_url_.possibly_invalid_spec();
  }

  // Read+Write Attributes.
  std::string binary_type(script::ExceptionState*);
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
  const EventListenerScriptValue* onclose() const {
    return GetAttributeEventListener(base::Tokens::close());
  }

  void set_onclose(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::close(), event_listener);
  }

  const EventListenerScriptValue* onerror() const {
    return GetAttributeEventListener(base::Tokens::error());
  }

  void set_onerror(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::error(), event_listener);
  }

  const EventListenerScriptValue* onmessage() const {
    return GetAttributeEventListener(base::Tokens::message());
  }

  void set_onmessage(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::message(), event_listener);
  }

  const EventListenerScriptValue* onopen() const {
    return GetAttributeEventListener(base::Tokens::open());
  }

  void set_onopen(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::open(), event_listener);
  }

  std::string GetHost() const { return resolved_url_.host(); }
  std::string GetResourceName() const;
  bool IsSecure() const { return is_secure_; }
  int GetPort() const { return resolved_url_.EffectiveIntPort(); }
  std::string GetPortAsString() const;

  DEFINE_WRAPPABLE_TYPE(WebSocket)

 private:
  void Initialize(dom::DOMSettings* dom_settings, const std::string& url,
                  const std::vector<std::string>& sub_protocols,
                  script::ExceptionState* exception_state);

  void Connect(const GURL& url, const std::vector<std::string>& sub_protocols);

  // Returns false if the check fails.
  bool CheckReadyState(script::ExceptionState* exception_state);

  // https://www.w3.org/TR/websockets/#dom-websocket-bufferedamount
  int32 buffered_amount_;
  // https://www.w3.org/TR/websockets/#dom-websocket-readystate
  uint16 ready_state_;
  // https://www.w3.org/TR/websockets/#dom-websocket-binarytype
  dom::MessageEvent::ResponseTypeCode binary_type_;
  // https://www.w3.org/TR/websockets/#dom-websocket-extensions
  std::string extensions_;
  // https://www.w3.org/TR/websockets/#dom-websocket-protocol
  std::string protocol_;
  // https://www.w3.org/TR/websockets/#dom-websocket-url

  GURL resolved_url_;
  base::ThreadChecker thread_checker_;

  // Parsed fields that are populated in Initialize.
  bool is_secure_;
  int port_;
  std::string resource_name_;  // The path of the URL.
  std::string entry_script_origin_;

  dom::DOMSettings* settings_;

  FRIEND_TEST(WebSocketTest, GoodOrigin);

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};
}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_WEB_SOCKET_H_
