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
#ifndef COBALT_WEBSOCKET_WEB_SOCKET_H_
#define COBALT_WEBSOCKET_WEB_SOCKET_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/base/compiler.h"
#include "cobalt/base/tokens.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/blob.h"
#include "cobalt/web/csp_delegate.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_event.h"
#include "cobalt/websocket/web_socket_impl.h"

#undef close
#undef open

namespace cobalt {
namespace websocket {

// This class represents a WebSocket.  It abides by RFC 6455 "The WebSocket
// Protocol", and implements the The WebSocket API spec at
// https://www.w3.org/TR/websockets/ (as of Jan 2017).
class WebSocket : public web::EventTarget {
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
  ~WebSocket();

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
  void Send(const scoped_refptr<web::Blob>& data,
            script::ExceptionState* exception_state);
  void Send(const script::Handle<script::ArrayBuffer>& data,
            script::ExceptionState* exception_state);
  void Send(const script::Handle<script::ArrayBufferView>& data,
            script::ExceptionState* exception_state);

  // API from old cobalt WebSocketEventInterface. They are callbacks to handle
  // corresponding lower level websocket events.
  void OnConnected(const std::string& selected_subprotocol);

  void OnDisconnected(bool was_clean, uint16 code, const std::string& reason);

  void OnReceivedData(bool is_text_frame,
                      scoped_refptr<net::IOBufferWithSize> data);
  void OnWriteDone(uint64_t bytes_written);

  void OnError() { this->DispatchEvent(new web::Event(base::Tokens::error())); }

  // EventHandlers.
  const EventListenerScriptValue* onclose() const {
    return GetAttributeEventListener(base::Tokens::close());
  }

  void set_onclose(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::close(), event_listener);
    PotentiallyAllowGarbageCollection();
  }

  const EventListenerScriptValue* onerror() const {
    return GetAttributeEventListener(base::Tokens::error());
  }

  void set_onerror(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::error(), event_listener);
    PotentiallyAllowGarbageCollection();
  }

  const EventListenerScriptValue* onmessage() const {
    return GetAttributeEventListener(base::Tokens::message());
  }

  void set_onmessage(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::message(), event_listener);
    PotentiallyAllowGarbageCollection();
  }

  const EventListenerScriptValue* onopen() const {
    return GetAttributeEventListener(base::Tokens::open());
  }

  void set_onopen(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::open(), event_listener);
    PotentiallyAllowGarbageCollection();
  }

  std::string GetHost() const { return resolved_url_.host(); }
  std::string GetResourceName() const;
  bool IsSecure() const { return is_secure_; }
  int GetPort() const { return resolved_url_.EffectiveIntPort(); }
  std::string GetPortAsString() const;

  web::CspDelegate* csp_delegate() const;

  DEFINE_WRAPPABLE_TYPE(WebSocket)

 private:
  // The following constructors are used for testing only.
  WebSocket(script::EnvironmentSettings* settings, const std::string& url,
            script::ExceptionState* exception_state,
            const bool require_network_module);

  WebSocket(script::EnvironmentSettings* settings, const std::string& url,
            const std::string& sub_protocol_list,
            script::ExceptionState* exception_state,
            const bool require_network_module);

  WebSocket(script::EnvironmentSettings* settings, const std::string& url,
            const std::vector<std::string>& sub_protocols,
            script::ExceptionState* exception_state,
            const bool require_network_module);

  void Initialize(script::EnvironmentSettings* settings, const std::string& url,
                  const std::vector<std::string>& sub_protocols,
                  script::ExceptionState* exception_state);

  void Connect(const GURL& url, const std::vector<std::string>& sub_protocols);

  void SetReadyState(const uint16 ready_state) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    ready_state_ = ready_state;
    PotentiallyAllowGarbageCollection();
  }

  // TODO: Investigate if these are worth optimizing.
  bool HasOnOpenListener() const { return (onopen() != NULL); }
  bool HasOnMessageListener() const { return (onmessage() != NULL); }
  bool HasOnErrorListener() const { return (onerror() != NULL); }
  bool HasOnCloseListener() const { return (onclose() != NULL); }
  bool HasOutstandingData() const { return (buffered_amount_ > 0); }

  void PotentiallyAllowGarbageCollection();

  // Returns false if the check fails.
  bool CheckReadyState(script::ExceptionState* exception_state);

  // Per https://www.w3.org/TR/websockets/#garbage-collection, prevent garbage
  // collection of this object while connection is live and event listeners are
  // registered.
  void AllowGarbageCollection();
  void PreventGarbageCollection();

  // https://www.w3.org/TR/websockets/#dom-websocket-bufferedamount
  int32 buffered_amount_;
  // https://www.w3.org/TR/websockets/#dom-websocket-readystate
  uint16 ready_state_;
  // https://www.w3.org/TR/websockets/#dom-websocket-binarytype
  web::MessageEvent::ResponseType binary_type_;
  // https://www.w3.org/TR/websockets/#dom-websocket-extensions
  std::string extensions_;
  // https://www.w3.org/TR/websockets/#dom-websocket-protocol
  std::string protocol_;
  // https://www.w3.org/TR/websockets/#dom-websocket-url

  GURL resolved_url_;
  THREAD_CHECKER(thread_checker_);

  // Parsed fields that are populated in Initialize.
  bool is_secure_;
  int port_;
  std::string resource_name_;  // The path of the URL.
  std::string entry_script_origin_;

  bool require_network_module_;
  web::EnvironmentSettings* settings_;
  scoped_refptr<WebSocketImpl> impl_;

  std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
      prevent_gc_while_listening_;

  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, BadOrigin);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, GoodOrigin);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, TestInitialReadyState);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, SyntaxErrorWhenBadScheme);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, ParseWsAndWssCorrectly);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, CheckSecure);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, SyntaxErrorWhenRelativeUrl);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, URLHasFragments);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, URLHasPort);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, URLHasNoPort);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, ParseHost);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, ParseResourceName);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, ParseEmptyResourceName);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, ParseResourceNameWithQuery);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, FailInsecurePort);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, FailInvalidSubProtocols);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, SubProtocols);
  FRIEND_TEST_ALL_PREFIXES(WebSocketTest, DuplicatedSubProtocols);
  friend class WebSocketImplTest;

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};
}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_WEB_SOCKET_H_
