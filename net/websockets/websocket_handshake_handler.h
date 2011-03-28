// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WebSocketHandshake*Handler handles WebSocket handshake request message
// from WebKit renderer process, and WebSocket handshake response message
// from WebSocket server.
// It modifies messages for the following reason:
// - We don't trust WebKit renderer process, so we'll not expose HttpOnly
//   cookies to the renderer process, so handles HttpOnly cookies in
//   browser process.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_HANDLER_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_HANDLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/spdy/spdy_framer.h"

namespace net {

class WebSocketHandshakeRequestHandler {
 public:
  WebSocketHandshakeRequestHandler();
  ~WebSocketHandshakeRequestHandler() {}

  // Parses WebSocket handshake request from renderer process.
  // It assumes a WebSocket handshake request message is given at once, and
  // no other data is added to the request message.
  bool ParseRequest(const char* data, int length);

  size_t original_length() const;

  // Appends the header value pair for |name| and |value|, if |name| doesn't
  // exist.
  void AppendHeaderIfMissing(const std::string& name,
                             const std::string& value);
  // Removes the headers that matches (case insensitive).
  void RemoveHeaders(const char* const headers_to_remove[],
                     size_t headers_to_remove_len);

  // Gets request info to open WebSocket connection.
  // Also, fill challange data in |challenge|.
  HttpRequestInfo GetRequestInfo(const GURL& url, std::string* challenge);
  // Gets request as SpdyHeaderBlock.
  // Also, fill challenge data in |challenge|.
  bool GetRequestHeaderBlock(const GURL& url,
                             spdy::SpdyHeaderBlock* headers,
                             std::string* challenge);
  // Gets WebSocket handshake raw request message to open WebSocket
  // connection.
  std::string GetRawRequest();
  // Calling raw_length is valid only after GetRawRquest() call.
  size_t raw_length() const;

 private:
  std::string status_line_;
  std::string headers_;
  std::string key3_;
  int original_length_;
  int raw_length_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeRequestHandler);
};

class WebSocketHandshakeResponseHandler {
 public:
  WebSocketHandshakeResponseHandler();
  ~WebSocketHandshakeResponseHandler();

  // Parses WebSocket handshake response from WebSocket server.
  // Returns number of bytes in |data| used for WebSocket handshake response
  // message, including response key.  If it already got whole WebSocket
  // handshake response message, returns zero.  In other words,
  // [data + returned value, data + length) will be WebSocket frame data
  // after handshake response message.
  // TODO(ukai): fail fast when response gives wrong status code.
  size_t ParseRawResponse(const char* data, int length);
  // Returns true if it already parses full handshake response message.
  bool HasResponse() const;
  // Parses WebSocket handshake response info given as HttpResponseInfo.
  bool ParseResponseInfo(const HttpResponseInfo& response_info,
                         const std::string& challenge);
  // Parses WebSocket handshake response as SpdyHeaderBlock.
  bool ParseResponseHeaderBlock(const spdy::SpdyHeaderBlock& headers,
                                const std::string& challenge);

  // Gets the headers value.
  void GetHeaders(const char* const headers_to_get[],
                  size_t headers_to_get_len,
                  std::vector<std::string>* values);
  // Removes the headers that matches (case insensitive).
  void RemoveHeaders(const char* const headers_to_remove[],
                     size_t headers_to_remove_len);

  // Gets raw WebSocket handshake response received from WebSocket server.
  std::string GetRawResponse() const;

  // Gets WebSocket handshake response message sent to renderer process.
  std::string GetResponse();

 private:
  std::string original_;
  int original_header_length_;
  std::string status_line_;
  std::string headers_;
  std::string header_separator_;
  std::string key_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeResponseHandler);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_HANDLER_H_
