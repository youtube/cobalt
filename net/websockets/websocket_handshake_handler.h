// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
//
// The classes below support two styles of handshake: handshake based
// on hixie-76 draft and one based on hybi-04 draft. The critical difference
// between these two is how they pass challenge and response values. Hixie-76
// based handshake appends a few bytes of binary data after header fields of
// handshake request and response. These data are called "key3" (for request)
// or "response key" (for response). On the other hand, handshake based on
// hybi-04 and later drafts put challenge and response values into handshake
// header fields, thus we do not need to send or receive extra bytes after
// handshake headers.
//
// While we are working on updating WebSocket implementation in WebKit to
// conform to the latest procotol draft, we need to accept both styles of
// handshake. After we land the protocol changes in WebKit, we will be able to
// drop codes handling old-style handshake.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_HANDLER_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_header_block.h"

namespace net {

class NET_EXPORT_PRIVATE WebSocketHandshakeRequestHandler {
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
  // Fills challange data (concatenation of key1, 2 and 3 for hybi-03 and
  // earlier, or Sec-WebSocket-Key header value for hybi-04 and later)
  // in |challenge|.
  HttpRequestInfo GetRequestInfo(const GURL& url, std::string* challenge);
  // Gets request as SpdyHeaderBlock.
  // Also, fills challenge data in |challenge|.
  bool GetRequestHeaderBlock(const GURL& url,
                             SpdyHeaderBlock* headers,
                             std::string* challenge,
                             int spdy_protocol_version);
  // Gets WebSocket handshake raw request message to open WebSocket
  // connection.
  std::string GetRawRequest();
  // Calling raw_length is valid only after GetRawRquest() call.
  size_t raw_length() const;

  // Returns the value of Sec-WebSocket-Version or Sec-WebSocket-Draft header
  // (the latter is an old name of the former). Returns 0 if both headers were
  // absent, which means the handshake was based on hybi-00 (= hixie-76).
  // Should only be called after the handshake has been parsed.
  int protocol_version() const;

 private:
  std::string status_line_;
  std::string headers_;
  std::string key3_;
  int original_length_;
  int raw_length_;
  int protocol_version_;  // "-1" means we haven't parsed the handshake yet.

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeRequestHandler);
};

class NET_EXPORT_PRIVATE WebSocketHandshakeResponseHandler {
 public:
  WebSocketHandshakeResponseHandler();
  ~WebSocketHandshakeResponseHandler();

  // Set WebSocket protocol version before parsing the response.
  // Default is 0 (hybi-00, which is same as hixie-76).
  int protocol_version() const;
  void set_protocol_version(int protocol_version);

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
  bool ParseResponseHeaderBlock(const SpdyHeaderBlock& headers,
                                const std::string& challenge,
                                int spdy_protocol_version);

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
  // Returns the length of response key. This function will return 0
  // if the specified WebSocket protocol version does not require
  // sending response key.
  size_t GetResponseKeySize() const;

  std::string original_;
  int original_header_length_;
  std::string status_line_;
  std::string headers_;
  std::string header_separator_;
  std::string key_;
  int protocol_version_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeResponseHandler);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_HANDLER_H_
