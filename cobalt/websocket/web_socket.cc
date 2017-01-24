/* Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/websocket/web_socket.h"

#include <string>

namespace {
const char* kBinaryTypeArrayBuffer = "arraybuffer";
const char* kBinaryTypeBlob = "blob";
}

namespace cobalt {
namespace websocket {

const uint16 WebSocket::kConnecting;
const uint16 WebSocket::kOpen;
const uint16 WebSocket::kClosing;
const uint16 WebSocket::kClosed;

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket.
WebSocket::WebSocket(const std::string& url,
                     script::ExceptionState* exception_state)
    : buffered_amount_(0),
      ready_state_(kConnecting),
      binary_type_(kBinaryTypeBlob) {
  UNREFERENCED_PARAMETER(url);
  UNREFERENCED_PARAMETER(exception_state);
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket.
WebSocket::WebSocket(const std::string& url, const std::string& protocols,
                     script::ExceptionState* exception_state)
    : buffered_amount_(0),
      ready_state_(kConnecting),
      binary_type_(kBinaryTypeBlob) {
  UNREFERENCED_PARAMETER(url);
  UNREFERENCED_PARAMETER(protocols);
  UNREFERENCED_PARAMETER(exception_state);
}

void WebSocket::set_binary_type(const std::string& binary_type,
                                script::ExceptionState* exception_state) {
  // Per spec:
  // "On setting, if the new value is either the string "blob" or the string
  // "arraybuffer", then set the IDL attribute to this new value.
  // Otherwise, throw a SyntaxError exception."
  if ((binary_type.compare(kBinaryTypeArrayBuffer) == 0) ||
      (binary_type.compare((kBinaryTypeBlob)) == 0)) {
    binary_type_ = binary_type;
  } else {
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, exception_state);
  }
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-close.
void WebSocket::Close(script::ExceptionState* exception_state) {
  UNREFERENCED_PARAMETER(exception_state);
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-close.
void WebSocket::Close(const uint16 code,
                      script::ExceptionState* exception_state) {
  UNREFERENCED_PARAMETER(code);
  UNREFERENCED_PARAMETER(exception_state);
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-close.
void WebSocket::Close(const uint16 code, const std::string& reason,
                      script::ExceptionState* exception_state) {
  UNREFERENCED_PARAMETER(code);
  UNREFERENCED_PARAMETER(reason);
  UNREFERENCED_PARAMETER(exception_state);
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-send.
void WebSocket::Send(const std::string& data,
                     script::ExceptionState* exception_state) {
  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(exception_state);
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-send.
void WebSocket::Send(const scoped_refptr<dom::Blob>& data,
                     script::ExceptionState* exception_state) {
  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(exception_state);
}

// Implements spec at https://www.w3.org/TR/websockets/#dom-websocket-send.
void WebSocket::Send(const scoped_refptr<dom::ArrayBuffer>& data,
                     script::ExceptionState* exception_state) {
  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(exception_state);
}

// Implements spect at https://www.w3.org/TR/websockets/#dom-websocket-send.
void WebSocket::Send(const scoped_refptr<dom::ArrayBufferView>& data,
                     script::ExceptionState* exception_state) {
  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(exception_state);
}

}  // namespace websocket
}  // namespace cobalt
