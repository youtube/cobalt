// Copyright 2013 The Chromium Authors. All rights reserved.
/* Modifications: Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/websocket/web_socket_handshake_helper.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/websockets/websocket_extension.h"
#include "net/websockets/websocket_extension_parser.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_handshake_challenge.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "starboard/system.h"

namespace {
// Following enum and anonymous functions are adapted from Chromium net source,
// commit id: 7321c9e7ee80ef15b65c2f39646a5a2d22a9c950 in
// src/net/websockets/websocket_basic_handshake_stream.cc.

enum GetHeaderResult {
  GET_HEADER_OK,
  GET_HEADER_MISSING,
  GET_HEADER_MULTIPLE,
};

GetHeaderResult GetSingleHeaderValue(const net::HttpResponseHeaders* headers,
                                     const base::StringPiece& name,
                                     std::string* value) {
  size_t iter = 0;
  size_t num_values = 0;
  std::string temp_value;
  std::string name_string = name.as_string();
  while (headers->EnumerateHeader(&iter, name_string, &temp_value)) {
    if (++num_values > 1) return GET_HEADER_MULTIPLE;
    *value = temp_value;
  }
  return num_values > 0 ? GET_HEADER_OK : GET_HEADER_MISSING;
}

std::string MissingHeaderMessage(const std::string& header_name) {
  return std::string("'") + header_name + "' header is missing";
}

std::string MultipleHeaderValuesMessage(const std::string& header_name) {
  return std::string("'") + header_name +
         "' header must not appear more than once in a response";
}

bool ValidateHeaderHasSingleValue(GetHeaderResult result,
                                  const std::string& header_name,
                                  std::string* failure_message) {
  if (result == GET_HEADER_MISSING) {
    *failure_message = MissingHeaderMessage(header_name);
    return false;
  }
  if (result == GET_HEADER_MULTIPLE) {
    *failure_message = MultipleHeaderValuesMessage(header_name);
    return false;
  }
  DCHECK_EQ(result, GET_HEADER_OK);
  return true;
}

bool ValidateUpgrade(const net::HttpResponseHeaders* headers,
                     std::string* failure_message) {
  std::string value;
  GetHeaderResult result =
      GetSingleHeaderValue(headers, net::websockets::kUpgrade, &value);
  if (!ValidateHeaderHasSingleValue(result, net::websockets::kUpgrade,
                                    failure_message)) {
    return false;
  }

  if (!base::LowerCaseEqualsASCII(value,
                                  net::websockets::kWebSocketLowercase)) {
    *failure_message = "'Upgrade' header value is not 'WebSocket': " + value;
    return false;
  }
  return true;
}

bool ValidateSecWebSocketAccept(const net::HttpResponseHeaders* headers,
                                const std::string& expected,
                                std::string* failure_message) {
  std::string actual;
  GetHeaderResult result = GetSingleHeaderValue(
      headers, net::websockets::kSecWebSocketAccept, &actual);
  if (!ValidateHeaderHasSingleValue(
          result, net::websockets::kSecWebSocketAccept, failure_message)) {
    return false;
  }

  if (expected != actual) {
    *failure_message = "Incorrect 'Sec-WebSocket-Accept' header value";
    return false;
  }
  return true;
}

bool ValidateConnection(const net::HttpResponseHeaders* headers,
                        std::string* failure_message) {
  // Connection header is permitted to contain other tokens.
  if (!headers->HasHeader(net::HttpRequestHeaders::kConnection)) {
    *failure_message =
        MissingHeaderMessage(net::HttpRequestHeaders::kConnection);
    return false;
  }
  if (!headers->HasHeaderValue(net::HttpRequestHeaders::kConnection,
                               net::websockets::kUpgrade)) {
    *failure_message = "'Connection' header value must contain 'Upgrade'";
    return false;
  }
  return true;
}

bool ValidateSubProtocol(
    const net::HttpResponseHeaders* headers,
    const std::vector<std::string>& requested_sub_protocols,
    std::string* sub_protocol, std::string* failure_message) {
  size_t iter = 0;
  std::string value;
  std::set<std::string> requested_set(requested_sub_protocols.begin(),
                                      requested_sub_protocols.end());
  int count = 0;
  bool has_multiple_protocols = false;
  bool has_invalid_protocol = false;

  while (!has_invalid_protocol || !has_multiple_protocols) {
    std::string temp_value;
    if (!headers->EnumerateHeader(&iter, net::websockets::kSecWebSocketProtocol,
                                  &temp_value))
      break;
    value = temp_value;
    if (requested_set.count(value) == 0) has_invalid_protocol = true;
    if (++count > 1) has_multiple_protocols = true;
  }

  if (has_multiple_protocols) {
    *failure_message =
        MultipleHeaderValuesMessage(net::websockets::kSecWebSocketProtocol);
    return false;
  } else if (count > 0 && requested_sub_protocols.size() == 0) {
    *failure_message = std::string(
                           "Response must not include 'Sec-WebSocket-Protocol' "
                           "header if not present in request: ") +
                       value;
    return false;
  } else if (has_invalid_protocol) {
    *failure_message = "'Sec-WebSocket-Protocol' header value '" + value +
                       "' in response does not match any of sent values";
    return false;
  } else if (requested_sub_protocols.size() > 0 && count == 0) {
    *failure_message =
        "Sent non-empty 'Sec-WebSocket-Protocol' header "
        "but no response was received";
    return false;
  }
  *sub_protocol = value;
  return true;
}

bool ValidateExtensions(const net::HttpResponseHeaders* headers,
                        std::string* failure_message) {
  size_t iter = 0;
  std::string header_value;
  while (headers->EnumerateHeader(
      &iter, net::websockets::kSecWebSocketExtensions, &header_value)) {
    net::WebSocketExtensionParser parser;
    if (!parser.Parse(header_value)) {
      *failure_message =
          "'Sec-WebSocket-Extensions' header value is "
          "rejected by the parser: " +
          header_value;
      return false;
    }

    const std::vector<net::WebSocketExtension>& extensions =
        parser.extensions();
    if (extensions.empty() == false) {
      *failure_message = "Cobalt does not support any websocket extensions";
      return false;
    }
  }
  return true;
}

cobalt::websocket::SecWebSocketKey GenerateRandomSecWebSocketKey() {
  using cobalt::websocket::SecWebSocketKey;
  SecWebSocketKey::SecWebSocketKeyBytes random_data;
  SbSystemGetRandomData(&random_data,
                        sizeof(SecWebSocketKey::SecWebSocketKeyBytes));
  cobalt::websocket::SecWebSocketKey key(random_data);
  return key;
}

}  // namespace

namespace cobalt {
namespace websocket {

void WebSocketHandshakeHelper::GenerateHandshakeRequest(
    const GURL& connect_url, const std::string& origin,
    const std::vector<std::string>& desired_sub_protocols,
    std::string* handshake_request) {
  DCHECK(handshake_request);
  GenerateSecWebSocketKey();

  int effective_port = connect_url.IntPort();
  std::string host_header(connect_url.host());
  if (effective_port != url::PORT_UNSPECIFIED) {
    host_header += ":" + connect_url.port();
  }

  std::string& header_string(*handshake_request);
  header_string.clear();
  header_string.reserve(256);  // This avoids reallocations for most cases.

  // Note: Concatenating string literals and std::string objects are separated
  // to avoid creating unnecessary std::string objects.
  header_string += "GET ";
  header_string += connect_url.path();
  if (connect_url.has_query()) {
    header_string += "?";
    header_string += connect_url.query();
  }
  header_string += " HTTP/1.1\r\n";
  header_string += "Host:";
  header_string += host_header;
  header_string += "\r\n";
  header_string +=
      "Connection:Upgrade\r\n"
      "Pragma:no-cache\r\n"
      "Cache-Control:no-cache\r\n"
      "Upgrade:websocket\r\n"
      "Sec-WebSocket-Extensions:\r\n"
      "Sec-WebSocket-Version:13\r\n";
  header_string += "Origin:";
  header_string += origin;
  header_string += "\r\n";
  header_string += "Sec-WebSocket-Key:";
  header_string += sec_websocket_key_.GetKeyEncodedInBase64();
  header_string += "\r\n";
  header_string += "User-Agent:";
  header_string += user_agent_;
  header_string += "\r\n";

  if (!desired_sub_protocols.empty()) {
    header_string += "Sec-WebSocket-Protocol:";
    header_string += base::JoinString(desired_sub_protocols, ",");
    header_string += "\r\n";
  }

  header_string += "\r\n";

  requested_sub_protocols_ = desired_sub_protocols;

  const std::string& sec_websocket_key_base64(
      sec_websocket_key_.GetKeyEncodedInBase64());
  handshake_challenge_response_ =
      net::ComputeSecWebSocketAccept(sec_websocket_key_base64);
}

WebSocketHandshakeHelper::WebSocketHandshakeHelper(
    const base::StringPiece user_agent)
    : sec_websocket_key_generator_function_(&GenerateRandomSecWebSocketKey),
      user_agent_(user_agent.data(), user_agent.size()) {}

WebSocketHandshakeHelper::WebSocketHandshakeHelper(
    const base::StringPiece user_agent,
    SecWebSocketKeyGeneratorFunction sec_websocket_key_generator_function)
    : sec_websocket_key_generator_function_(
          sec_websocket_key_generator_function),
      user_agent_(user_agent.data(), user_agent.size()) {}

bool WebSocketHandshakeHelper::IsResponseValid(
    const net::HttpResponseHeaders& headers, std::string* failure_message) {
  DCHECK(failure_message);
  int response_code = headers.response_code();

  // Check response code first.
  if (response_code != net::HTTP_SWITCHING_PROTOCOLS) {
    *failure_message =
        "Invalid response code " + base::IntToString(response_code);
    return false;
  }

  if (!ValidateUpgrade(&headers, failure_message)) {
    return false;
  }
  if (!ValidateSecWebSocketAccept(&headers, handshake_challenge_response_,
                                  failure_message)) {
    return false;
  }
  if (!ValidateConnection(&headers, failure_message)) {
    return false;
  }
  if (!ValidateSubProtocol(&headers, requested_sub_protocols_,
                           &selected_subprotocol_, failure_message)) {
    return false;
  }
  // Cobalt does not support extensions, so we just make sure that none are
  // being selected.
  if (!ValidateExtensions(&headers, failure_message)) {
    return false;
  }

  failure_message->clear();
  return true;
}

void WebSocketHandshakeHelper::GenerateSecWebSocketKey() {
  sec_websocket_key_ = sec_websocket_key_generator_function_();
}

}  // namespace websocket
}  // namespace cobalt
