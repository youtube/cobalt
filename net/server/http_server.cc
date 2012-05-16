// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/http_server.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "net/base/tcp_listen_socket.h"
#include "net/server/http_connection.h"
#include "net/server/http_server_request_info.h"
#include "net/server/web_socket.h"

namespace net {

HttpServer::HttpServer(const std::string& host,
                       int port,
                       HttpServer::Delegate* del)
    : delegate_(del) {
  server_ = TCPListenSocket::CreateAndListen(host, port, this);
}

void HttpServer::AcceptWebSocket(
    int connection_id,
    const HttpServerRequestInfo& request) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  DCHECK(connection->web_socket_.get());
  connection->web_socket_->Accept(request);
}

void HttpServer::SendOverWebSocket(int connection_id,
                                   const std::string& data) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  DCHECK(connection->web_socket_.get());
  connection->web_socket_->Send(data);
}

void HttpServer::Send(int connection_id, const std::string& data) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  connection->Send(data);
}

void HttpServer::Send(int connection_id, const char* bytes, int len) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  connection->Send(bytes, len);
}

void HttpServer::Send200(int connection_id,
                         const std::string& data,
                         const std::string& content_type) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  connection->Send200(data, content_type);
}

void HttpServer::Send404(int connection_id) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  connection->Send404();
}

void HttpServer::Send500(int connection_id, const std::string& message) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  connection->Send500(message);
}

void HttpServer::Close(int connection_id)
{
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  // Initiating close from server-side does not lead to the DidClose call.
  // Do it manually here.
  DidClose(connection->socket_);
}

void HttpServer::DidAccept(StreamListenSocket* server,
                           StreamListenSocket* socket) {
  HttpConnection* connection = new HttpConnection(this, socket);
  id_to_connection_[connection->id()] = connection;
  socket_to_connection_[socket] = connection;
}

void HttpServer::DidRead(StreamListenSocket* socket,
                         const char* data,
                         int len) {
  HttpConnection* connection = FindConnection(socket);
  DCHECK(connection != NULL);
  if (connection == NULL)
    return;

  connection->recv_data_.append(data, len);
  while (connection->recv_data_.length()) {
    if (connection->web_socket_.get()) {
      std::string message;
      WebSocket::ParseResult result = connection->web_socket_->Read(&message);
      if (result == WebSocket::FRAME_INCOMPLETE)
        break;

      if (result == WebSocket::FRAME_CLOSE ||
          result == WebSocket::FRAME_ERROR) {
        Close(connection->id());
        break;
      }
      delegate_->OnWebSocketMessage(connection->id(), message);
      continue;
    }

    HttpServerRequestInfo request;
    size_t pos = 0;
    if (!ParseHeaders(connection, &request, &pos))
      break;

    std::string connection_header = request.GetHeaderValue("Connection");
    if (connection_header == "Upgrade") {
      connection->web_socket_.reset(WebSocket::CreateWebSocket(connection,
                                                               request,
                                                               &pos));

      if (!connection->web_socket_.get())  // Not enought data was received.
        break;
      delegate_->OnWebSocketRequest(connection->id(), request);
      connection->Shift(pos);
      continue;
    }
    // Request body is not supported. It is always empty.
    delegate_->OnHttpRequest(connection->id(), request);
    connection->Shift(pos);
  }
}

void HttpServer::DidClose(StreamListenSocket* socket) {
  HttpConnection* connection = FindConnection(socket);
  DCHECK(connection != NULL);
  id_to_connection_.erase(connection->id());
  socket_to_connection_.erase(connection->socket_);
  delete connection;
}

HttpServer::~HttpServer() {
  IdToConnectionMap copy = id_to_connection_;
  for (IdToConnectionMap::iterator it = copy.begin(); it != copy.end(); ++it)
    delete it->second;

  server_ = NULL;
}

//
// HTTP Request Parser
// This HTTP request parser uses a simple state machine to quickly parse
// through the headers.  The parser is not 100% complete, as it is designed
// for use in this simple test driver.
//
// Known issues:
//   - does not handle whitespace on first HTTP line correctly.  Expects
//     a single space between the method/url and url/protocol.

// Input character types.
enum header_parse_inputs {
  INPUT_SPACE,
  INPUT_CR,
  INPUT_LF,
  INPUT_COLON,
  INPUT_DEFAULT,
  MAX_INPUTS,
};

// Parser states.
enum header_parse_states {
  ST_METHOD,     // Receiving the method
  ST_URL,        // Receiving the URL
  ST_PROTO,      // Receiving the protocol
  ST_HEADER,     // Starting a Request Header
  ST_NAME,       // Receiving a request header name
  ST_SEPARATOR,  // Receiving the separator between header name and value
  ST_VALUE,      // Receiving a request header value
  ST_DONE,       // Parsing is complete and successful
  ST_ERR,        // Parsing encountered invalid syntax.
  MAX_STATES
};

// State transition table
int parser_state[MAX_STATES][MAX_INPUTS] = {
/* METHOD    */ { ST_URL,       ST_ERR,     ST_ERR,   ST_ERR,       ST_METHOD },
/* URL       */ { ST_PROTO,     ST_ERR,     ST_ERR,   ST_URL,       ST_URL },
/* PROTOCOL  */ { ST_ERR,       ST_HEADER,  ST_NAME,  ST_ERR,       ST_PROTO },
/* HEADER    */ { ST_ERR,       ST_ERR,     ST_NAME,  ST_ERR,       ST_ERR },
/* NAME      */ { ST_SEPARATOR, ST_DONE,    ST_ERR,   ST_SEPARATOR, ST_NAME },
/* SEPARATOR */ { ST_SEPARATOR, ST_ERR,     ST_ERR,   ST_SEPARATOR, ST_VALUE },
/* VALUE     */ { ST_VALUE,     ST_HEADER,  ST_NAME,  ST_VALUE,     ST_VALUE },
/* DONE      */ { ST_DONE,      ST_DONE,    ST_DONE,  ST_DONE,      ST_DONE },
/* ERR       */ { ST_ERR,       ST_ERR,     ST_ERR,   ST_ERR,       ST_ERR }
};

// Convert an input character to the parser's input token.
int charToInput(char ch) {
  switch(ch) {
    case ' ':
      return INPUT_SPACE;
    case '\r':
      return INPUT_CR;
    case '\n':
      return INPUT_LF;
    case ':':
      return INPUT_COLON;
  }
  return INPUT_DEFAULT;
}

bool HttpServer::ParseHeaders(HttpConnection* connection,
                              HttpServerRequestInfo* info,
                              size_t* ppos) {
  size_t& pos = *ppos;
  size_t data_len = connection->recv_data_.length();
  int state = ST_METHOD;
  std::string buffer;
  std::string header_name;
  std::string header_value;
  while (pos < data_len) {
    char ch = connection->recv_data_[pos++];
    int input = charToInput(ch);
    int next_state = parser_state[state][input];

    bool transition = (next_state != state);
    if (transition) {
      // Do any actions based on state transitions.
      switch (state) {
        case ST_METHOD:
          info->method = buffer;
          buffer.clear();
          break;
        case ST_URL:
          info->path = buffer;
          buffer.clear();
          break;
        case ST_PROTO:
          // TODO(mbelshe): Deal better with parsing protocol.
          DCHECK(buffer == "HTTP/1.1");
          buffer.clear();
          break;
        case ST_NAME:
          header_name = buffer;
          buffer.clear();
          break;
        case ST_VALUE:
          header_value = buffer;
          // TODO(mbelshe): Deal better with duplicate headers
          DCHECK(info->headers.find(header_name) == info->headers.end());
          info->headers[header_name] = header_value;
          buffer.clear();
          break;
        case ST_SEPARATOR:
          buffer.append(&ch, 1);
          break;
      }
      state = next_state;
    } else {
      // Do any actions based on current state
      switch (state) {
        case ST_METHOD:
        case ST_URL:
        case ST_PROTO:
        case ST_VALUE:
        case ST_NAME:
          buffer.append(&ch, 1);
          break;
        case ST_DONE:
          DCHECK(input == INPUT_LF);
          return true;
        case ST_ERR:
          return false;
      }
    }
  }
  // No more characters, but we haven't finished parsing yet.
  return false;
}

HttpConnection* HttpServer::FindConnection(int connection_id) {
  IdToConnectionMap::iterator it = id_to_connection_.find(connection_id);
  if (it == id_to_connection_.end())
    return NULL;
  return it->second;
}

HttpConnection* HttpServer::FindConnection(StreamListenSocket* socket) {
  SocketToConnectionMap::iterator it = socket_to_connection_.find(socket);
  if (it == socket_to_connection_.end())
    return NULL;
  return it->second;
}

}  // namespace net
