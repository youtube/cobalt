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

#include "cobalt/websocket/web_socket.h"

#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/websocket/close_event.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_constants.h"
#include "net/base/net_util.h"
#include "net/websockets/websocket_errors.h"

namespace {

typedef uint16 SerializedCloseStatusCodeType;

static const std::string kComma = ",";

bool IsURLAbsolute(cobalt::dom::DOMSettings* dom_settings,
                   const std::string& url) {
  // This is a requirement for calling spec()
  DCHECK(dom_settings->base_url().is_valid());

  url_canon::RawCanonOutputT<char> whitespace_buffer;
  int relative_length;
  const char* relative =
      RemoveURLWhitespace(url.c_str(), static_cast<int>(url.size()),
                          &whitespace_buffer, &relative_length);

  url_parse::Component relative_component;

  const std::string& base_url(dom_settings->base_url().spec());
  url_parse::Parsed parsed;

  url_parse::ParseStandardURL(base_url.c_str(),
                              static_cast<int>(base_url.length()), &parsed);

  bool is_relative;
  url_canon::IsRelativeURL(base_url.c_str(), parsed, relative, relative_length,
                           true, &is_relative, &relative_component);

  return !is_relative;
}

bool IsValidSubprotocolCharacter(const char subprotocol_char) {
  // From RFC 2616 (https://tools.ietf.org/html/rfc2616),

  // CHAR: "any US-ASCII character (octets 0 - 127)"
  // CTL: "any US-ASCII control character (octets 0 - 31) and DEL (127)>"
  bool is_CHAR_except_CTL =
      ((subprotocol_char > 31) && (subprotocol_char < 127));
  bool is_separator = false;

  // Per spec, the separators are defined as following where (SP and HT are
  // space and horizontal tab, respectively):
  // separators    = "(" | ")" | "<" | ">" | "@"
  //               | "," | ";" | ":" | "\" | <">
  //               | "/" | "[" | "]" | "?" | "="
  //               | "{" | "}" | SP | HT

  switch (subprotocol_char) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case ' ':
    case '\t':
      is_separator = true;
      break;
    default:
      break;
  }

  return (is_CHAR_except_CTL && !is_separator);
}

bool IsSubProtocolValid(const std::string& sub_protocol) {
  if (sub_protocol.empty()) {
    return false;
  }

  for (std::string::const_iterator it(sub_protocol.begin());
       it != sub_protocol.end(); ++it) {
    if (!IsValidSubprotocolCharacter(*it)) {
      return false;
    }
  }

  return true;
}

bool AreSubProtocolsValid(const std::vector<std::string>& sub_protocols,
                          std::string* invalid_subprotocol) {
  DCHECK(invalid_subprotocol);
  for (std::vector<std::string>::const_iterator it(sub_protocols.begin());
       it != sub_protocols.end(); ++it) {
    if (!IsSubProtocolValid(*it)) {
      *invalid_subprotocol = *it;
      return false;
    }
  }

  return true;
}

bool AreSubProtocolsUnique(const std::vector<std::string>& sub_protocols) {
  if (sub_protocols.size() <= 1) return true;

  std::set<std::string> all_protocols;
  all_protocols.insert(sub_protocols.begin(), sub_protocols.end());
  return (all_protocols.size() == sub_protocols.size());
}

bool IsValidBinaryType(cobalt::dom::MessageEvent::ResponseTypeCode code) {
  return (code == cobalt::dom::MessageEvent::kBlob) ||
         (code == cobalt::dom::MessageEvent::kArrayBuffer);
}

}  // namespace

namespace cobalt {
namespace websocket {

const uint16 WebSocket::kConnecting;
const uint16 WebSocket::kOpen;
const uint16 WebSocket::kClosing;
const uint16 WebSocket::kClosed;

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket.
WebSocket::WebSocket(script::EnvironmentSettings* settings,
                     const std::string& url,
                     script::ExceptionState* exception_state)
    : require_network_module_(true) {
  const std::vector<std::string> empty;
  Initialize(settings, url, empty, exception_state);
}

WebSocket::WebSocket(script::EnvironmentSettings* settings,
                     const std::string& url,
                     const std::vector<std::string>& sub_protocols,
                     script::ExceptionState* exception_state)
    : require_network_module_(true) {
  Initialize(settings, url, sub_protocols, exception_state);
}

std::string WebSocket::binary_type(script::ExceptionState* exception_state) {
  if (!IsValidBinaryType(binary_type_)) {
    NOTREACHED() << "Invalid binary_type_";
    dom::DOMException::Raise(dom::DOMException::kNone, exception_state);
    return std::string();
  }
  return dom::MessageEvent::GetResponseTypeAsString(binary_type_);
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket.
WebSocket::WebSocket(script::EnvironmentSettings* settings,
                     const std::string& url,
                     const std::string& sub_protocol_list,
                     script::ExceptionState* exception_state)
    : require_network_module_(true) {
  std::vector<std::string> sub_protocols;
  Tokenize(sub_protocol_list, kComma, &sub_protocols);
  Initialize(settings, url, sub_protocols, exception_state);
}

void WebSocket::set_binary_type(const std::string& binary_type,
                                script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Per spec:
  // "On setting, if the new value is either the string "blob" or the string
  // "arraybuffer", then set the IDL attribute to this new value.
  // Otherwise, throw a SyntaxError exception."
  base::StringPiece binary_type_string_piece(binary_type);
  dom::MessageEvent::ResponseTypeCode response_code =
      dom::MessageEvent::GetResponseTypeCode(binary_type_string_piece);
  if (!IsValidBinaryType(response_code)) {
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, exception_state);
  } else {
    binary_type_ = response_code;
  }
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-close.
void WebSocket::Close(script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const std::string empty_reason;
  Close(net::kWebSocketNormalClosure, empty_reason, exception_state);
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-close.
void WebSocket::Close(const uint16 code,
                      script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const std::string empty_reason;
  Close(code, empty_reason, exception_state);
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-close.
void WebSocket::Close(const uint16 code, const std::string& reason,
                      script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Per spec @ https://www.w3.org/TR/websockets/#dom-websocket-close
  // "If reason is longer than 123 bytes, then throw a SyntaxError exception and
  // abort these steps."
  if (reason.size() > kMaxCloseReasonSize) {
    DLOG(ERROR) << "Reason specified in WebSocket::Close must be less than "
                << kMaxControlPayloadSizeInBytes << " bytes.";

    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, exception_state);
    return;
  }

  if (!net::IsValidCloseStatusCode(code)) {
    dom::DOMException::Raise(dom::DOMException::kInvalidAccessErr,
                             exception_state);
    return;
  }

  DLOG(INFO) << "Websocket close code " << code;

  switch (ready_state()) {
    case kOpen:
    case kConnecting:
      DCHECK(impl_);
      impl_->Close(net::WebSocketError(code), reason);
      SetReadyState(kClosing);
      break;
    case kClosing:
    case kClosed:
      return;
    default:
      NOTREACHED() << "Invalid ready_state_ " << ready_state()
                   << " in WebSocket::Close.";
  }
}

bool WebSocket::CheckReadyState(script::ExceptionState* exception_state) {
  DCHECK(exception_state);

  // Per Websockets API spec:
  // "If the readyState attribute is CONNECTING, it must throw an
  // InvalidStateError exception"
  if (ready_state() == kConnecting) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             "readyState is not in CONNECTING state",
                             exception_state);
    return false;
  }

  return true;
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-send.
void WebSocket::Send(const std::string& data,
                     script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(impl_);
  if (!CheckReadyState(exception_state)) {
    return;
  }
  std::string error_message;
  bool success = impl_->SendText(data.data(), data.size(), &buffered_amount_,
                                 &error_message);
  if (!success) {
    DLOG(ERROR) << "Unable to send message: [" << error_message << "]";
  }
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-send.
void WebSocket::Send(const scoped_refptr<dom::Blob>& data,
                     script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(impl_);
  if (!CheckReadyState(exception_state)) {
    return;
  }
  dom::Blob* blob(data.get());
  if (!blob) {
    return;
  }
  std::string error_message;
  bool success = impl_->SendBinary(reinterpret_cast<const char*>(blob->data()),
                                   static_cast<std::size_t>(blob->size()),
                                   &buffered_amount_, &error_message);
  if (!success) {
    DLOG(ERROR) << "Unable to send message: [" << error_message << "]";
  }
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-send.
void WebSocket::Send(const scoped_refptr<dom::ArrayBuffer>& data,
                     script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(impl_);
  if (!CheckReadyState(exception_state)) {
    return;
  }
  std::string error_message;
  dom::ArrayBuffer* array_buffer(data.get());
  if (!array_buffer) {
    return;
  }
  bool success =
      impl_->SendBinary(reinterpret_cast<const char*>(array_buffer->data()),
                        static_cast<std::size_t>(array_buffer->byte_length()),
                        &buffered_amount_, &error_message);
  if (!success) {
    DLOG(ERROR) << "Unable to send message: [" << error_message << "]";
  }
}

// Implements spect at https://www.w3.org/TR/websockets/#dom-websocket-send.
void WebSocket::Send(const scoped_refptr<dom::ArrayBufferView>& data,
                     script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(impl_);
  if (!CheckReadyState(exception_state)) {
    return;
  }
  std::string error_message;
  dom::ArrayBufferView* array_buffer_view(data.get());
  if (!array_buffer_view) {
    return;
  }
  bool success = impl_->SendBinary(
      reinterpret_cast<const char*>(array_buffer_view->base_address()),
      static_cast<std::size_t>(array_buffer_view->byte_length()),
      &buffered_amount_, &error_message);
  if (!success) {
    DLOG(ERROR) << "Unable to send message: [" << error_message << "]";
  }
}

std::string WebSocket::GetResourceName() const {
  if (resolved_url_.has_query()) {
    return resolved_url_.path() + "?" + resolved_url_.query();
  } else {
    return resolved_url_.path();
  }
}

std::string WebSocket::GetPortAsString() const {
  if (resolved_url_.has_port()) {
    return resolved_url_.port();
  }

  return base::IntToString(GetPort());
}

void WebSocket::OnConnected(const std::string& selected_subprotocol) {
  DLOG(INFO) << "Websockets selected subprotocol: [" << selected_subprotocol
             << "]";
  protocol_ = selected_subprotocol;
  SetReadyState(kOpen);
  this->DispatchEvent(new dom::Event(base::Tokens::open()));
}

void WebSocket::OnDisconnected(bool was_clean, uint16 code,
                               const std::string& reason) {
  SetReadyState(kClosed);
  CloseEventInit close_event_init;
  close_event_init.set_was_clean(was_clean);
  close_event_init.set_code(code);
  close_event_init.set_reason(reason);
  this->DispatchEvent(new CloseEvent(base::Tokens::close(), close_event_init));
}

void WebSocket::OnReceivedData(bool is_text_frame,
                               scoped_refptr<net::IOBufferWithSize> data) {
  dom::MessageEvent::ResponseTypeCode response_type_code = binary_type_;
  if (is_text_frame) {
    response_type_code = dom::MessageEvent::kText;
  }
  this->DispatchEvent(new dom::MessageEvent(base::Tokens::message(), settings_,
                                            response_type_code, data));
}

void WebSocket::Initialize(script::EnvironmentSettings* settings,
                           const std::string& url,
                           const std::vector<std::string>& sub_protocols,
                           script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  buffered_amount_ = 0;
  binary_type_ = dom::MessageEvent::kBlob;
  is_secure_ = false;
  port_ = -1;
  preventing_gc_ = false;
  SetReadyState(kConnecting);

  settings_ = base::polymorphic_downcast<dom::DOMSettings*>(settings);
  if (!settings_) {
    dom::DOMException::Raise(dom::DOMException::kNone,
                             "Internal error: Unable to get DOM settings.",
                             exception_state);
    NOTREACHED() << "Unable to get DOM settings.";
    return;
  }

  if (require_network_module_ && !settings_->network_module()) {
    dom::DOMException::Raise(dom::DOMException::kNone,
                             "Internal error: Unable to get network module.",
                             exception_state);
    NOTREACHED() << "Unable to get network module.";
    return;
  }

  if (!settings_->base_url().is_valid()) {
    dom::DOMException::Raise(
        dom::DOMException::kNone,
        "Internal error: base_url (the url of the entry script) must be valid.",
        exception_state);
    return;
  } else {
    // GetOrigin() can only be called on valid urls.
    // Since origin does not contain fragments, spec() is guaranteed
    // to return an ASCII encoded string.
    entry_script_origin_ = settings_->base_url().GetOrigin().spec();
  }

  // Per spec:
  // Parse a WebSocket URL's components from the url argument, to obtain host,
  // port, resource name, and secure. If this fails, throw a SyntaxError
  // exception and abort these steps. [WSP]"

  resolved_url_ = settings_->base_url().Resolve(url);
  if (resolved_url_.is_empty()) {
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, "url is empty",
                             exception_state);
    return;
  }

  if (!resolved_url_.is_valid()) {
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, "url is invalid",
                             exception_state);
    return;
  }

  bool is_absolute = IsURLAbsolute(settings_, url);

  if (!is_absolute) {
    std::string error_message = "Only relative URLs are supported.  [" + url +
                                "] is not an absolute URL.";
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, error_message,
                             exception_state);
    return;
  }
  // Per spec @ https://www.w3.org/TR/websockets/#parsing-websocket-urls:
  // If url does not have a <scheme> component whose value, when converted to
  // ASCII lowercase, is either "ws" or "wss", then fail this algorithm.
  if (resolved_url_.SchemeIs(url::kWsScheme)) {
    is_secure_ = false;
  } else if (resolved_url_.SchemeIs(url::kWssScheme)) {
    is_secure_ = true;
  } else {
    std::string error_message = "Invalid scheme [" + resolved_url_.scheme() +
                                "].  Only " + std::string(url::kWsScheme) +
                                ", and " + std::string(url::kWssScheme) +
                                " schemes are supported.";
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, error_message,
                             exception_state);
    return;
  }

  // If url has a <fragment> component, then fail this algorithm.
  std::string fragment(resolved_url_.ref());
  if (!fragment.empty()) {
    std::string error_message =
        "URL has a fragment '" + fragment +
        "'.  Fragments are not are supported in websocket URLs.";
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, error_message,
                             exception_state);
    return;
  }

  if (!net::IsPortAllowedByDefault(GetPort())) {
    std::string error_message = "Connecting to port " + GetPortAsString() +
                                " using websockets is not allowed.";
    dom::DOMException::Raise(dom::DOMException::kSecurityErr, error_message,
                             exception_state);
    return;
  }

  std::string invalid_subprotocol;
  if (!AreSubProtocolsValid(sub_protocols, &invalid_subprotocol)) {
    std::string error_message = "Invalid subprotocol [" + invalid_subprotocol +
                                "].  Subprotocols' characters must be in valid "
                                "range and not have separating characters.  "
                                "See RFC 2616 for details.";
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, error_message,
                             exception_state);
    return;
  }

  if (!AreSubProtocolsUnique(sub_protocols)) {
    std::string error_message = "Subprotocol values must be unique.";
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, error_message,
                             exception_state);
    return;
  }

  Connect(resolved_url_, sub_protocols);
}

void WebSocket::Connect(const GURL& url,
                        const std::vector<std::string>& sub_protocols) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(settings_);

  GURL origin_gurl = settings_->base_url().GetOrigin();
  const std::string& origin = origin_gurl.possibly_invalid_spec();

  impl_ = make_scoped_refptr<WebSocketImpl>(
      new WebSocketImpl(settings_->network_module(), this));

  impl_->Connect(origin, url, sub_protocols);
}

WebSocket::WebSocket(script::EnvironmentSettings* settings,
                     const std::string& url,
                     script::ExceptionState* exception_state,
                     const bool require_network_module)
    : require_network_module_(require_network_module) {
  const std::vector<std::string> empty;
  Initialize(settings, url, empty, exception_state);
}

WebSocket::WebSocket(script::EnvironmentSettings* settings,
                     const std::string& url, const std::string& sub_protocol,
                     script::ExceptionState* exception_state,
                     const bool require_network_module)
    : require_network_module_(require_network_module) {
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back(sub_protocol);
  Initialize(settings, url, sub_protocols, exception_state);
}

WebSocket::WebSocket(script::EnvironmentSettings* settings,
                     const std::string& url,
                     const std::vector<std::string>& sub_protocols,
                     script::ExceptionState* exception_state,
                     const bool require_network_module)
    : require_network_module_(require_network_module) {
  Initialize(settings, url, sub_protocols, exception_state);
}

void WebSocket::PotentiallyAllowGarbageCollection() {
  bool prevent_gc = false;
  switch (ready_state()) {
    case kOpen:
      //  Per spec, "A WebSocket object whose readyState attribute's value was
      //  set to OPEN (1) as of the last time the event loop started executing a
      //  task must not be garbage collected if there are any event listeners
      //  registered for message events, error, or close events..
      //  A WebSocket object with an established connection that has data queued
      //  to be transmitted to the network must not be garbage collected."
      prevent_gc = HasOnMessageListener() || HasOnErrorListener() ||
                   HasOnCloseListener() || HasOutstandingData();
      break;
    case kConnecting:
      //  Per spec, "WebSocket object whose readyState attribute's value was set
      //  to CONNECTING (0) as of the last time the event loop started executing
      //  a task must not be garbage collected if there are any event listeners
      //  registered for open events, message events, error events, or close
      //  events."
      prevent_gc = HasOnOpenListener() || HasOnMessageListener() ||
                   HasOnErrorListener() || HasOnCloseListener() ||
                   HasOutstandingData();
      break;
    case kClosing:
      //  Per spec, "A WebSocket object whose readyState attribute's value was
      //  set to CLOSING (2) as of the last time the event loop started
      //  executing a task must not be garbage collected if there are any event
      //  listeners registered for error or close events."
      prevent_gc =
          HasOnErrorListener() || HasOnCloseListener() || HasOutstandingData();
      break;
    case kClosed:
      prevent_gc = false;
      break;
    default:
      NOTREACHED() << "Invalid ready_state: " << ready_state();
  }

  if (prevent_gc != preventing_gc_) {
    if (prevent_gc) {
      PreventGarbageCollection();
    } else {
      AllowGarbageCollection();
    }

    // The above function calls should change |preventing_gc_|.
    DCHECK_EQ(prevent_gc, preventing_gc_);
  }
}

void WebSocket::PreventGarbageCollection() {
  settings_->global_environment()->PreventGarbageCollection(
      make_scoped_refptr(this));
  DCHECK(!preventing_gc_);
  preventing_gc_ = true;
}

void WebSocket::AllowGarbageCollection() {
  DCHECK(preventing_gc_);

  // Note: the fall through in this switch statement is on purpose.
  switch (ready_state_) {
    case kConnecting:
      DCHECK(!HasOnOpenListener());
    case kOpen:
      DCHECK(!HasOnMessageListener());
    case kClosing:
      DCHECK(!HasOnErrorListener());
      DCHECK(!HasOnCloseListener());
    default:
      break;
  }

  preventing_gc_ = false;
  settings_->global_environment()->AllowGarbageCollection(
      make_scoped_refptr(this));
}

}  // namespace websocket
}  // namespace cobalt
