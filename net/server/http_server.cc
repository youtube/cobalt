// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"

int HttpServer::Connection::lastId_ = 0;
HttpServer::HttpServer(const std::string& host,
                       int port,
                       HttpServer::Delegate* del)
    : delegate_(del) {
  server_ = ListenSocket::Listen(host, port, this);
}

HttpServer::~HttpServer() {
  IdToConnectionMap copy = id_to_connection_;
  for (IdToConnectionMap::iterator it = copy.begin(); it != copy.end(); ++it)
    delete it->second;

  server_ = NULL;
}

std::string GetHeaderValue(
    const HttpServerRequestInfo& request,
    const std::string& header_name) {
  HttpServerRequestInfo::HeadersMap::iterator it =
      request.headers.find(header_name);
  if (it != request.headers.end())
    return it->second;
  return "";
}

uint32 WebSocketKeyFingerprint(const std::string& str) {
  std::string result;
  const char* pChar = str.c_str();
  int length = str.length();
  int spaces = 0;
  for (int i = 0; i < length; ++i) {
    if (pChar[i] >= '0' && pChar[i] <= '9')
      result.append(&pChar[i], 1);
    else if (pChar[i] == ' ')
      spaces++;
  }
  if (spaces == 0)
    return 0;
  int64 number = 0;
  if (!base::StringToInt64(result, &number))
    return 0;
  return htonl(static_cast<uint32>(number / spaces));
}

void HttpServer::AcceptWebSocket(
    int connection_id,
    const HttpServerRequestInfo& request) {
  Connection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  std::string key1 = GetHeaderValue(request, "Sec-WebSocket-Key1");
  std::string key2 = GetHeaderValue(request, "Sec-WebSocket-Key2");

  uint32 fp1 = WebSocketKeyFingerprint(key1);
  uint32 fp2 = WebSocketKeyFingerprint(key2);

  char data[16];
  memcpy(data, &fp1, 4);
  memcpy(data + 4, &fp2, 4);
  memcpy(data + 8, &request.data[0], 8);

  MD5Digest digest;
  MD5Sum(data, 16, &digest);

  std::string origin = GetHeaderValue(request, "Origin");
  std::string host = GetHeaderValue(request, "Host");
  std::string location = "ws://" + host + request.path;
  connection->is_web_socket_ = true;
  connection->socket_->Send(base::StringPrintf(
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: %s\r\n"
      "Sec-WebSocket-Location: %s\r\n"
      "\r\n",
      origin.c_str(),
      location.c_str()));
  connection->socket_->Send(reinterpret_cast<char*>(digest.a), 16);
}

void HttpServer::SendOverWebSocket(int connection_id,
                                   const std::string& data) {
  Connection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  DCHECK(connection->is_web_socket_);
  char message_start = 0;
  char message_end = -1;
  connection->socket_->Send(&message_start, 1);
  connection->socket_->Send(data);
  connection->socket_->Send(&message_end, 1);
}

void HttpServer::Send(int connection_id, const std::string& data) {
  Connection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  connection->socket_->Send(data);
}

void HttpServer::Send(int connection_id, const char* bytes, int len) {
  Connection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  connection->socket_->Send(bytes, len);
}

void HttpServer::Send200(int connection_id,
                         const std::string& data,
                         const std::string& content_type) {
  Connection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  connection->socket_->Send(base::StringPrintf(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type:%s\r\n"
      "Content-Length:%d\r\n"
      "\r\n",
      content_type.c_str(),
      static_cast<int>(data.length())));
  connection->socket_->Send(data);
}

void HttpServer::Send404(int connection_id) {
  Connection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  connection->socket_->Send(
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Length: 0\r\n"
      "\r\n");
}

void HttpServer::Send500(int connection_id, const std::string& message) {
  Connection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  connection->socket_->Send(base::StringPrintf(
      "HTTP/1.1 500 Internal Error\r\n"
      "Content-Type:text/html\r\n"
      "Content-Length:%d\r\n"
      "\r\n"
      "%s",
      static_cast<int>(message.length()),
      message.c_str()));
}

void HttpServer::Close(int connection_id)
{
  Connection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  connection->DetachSocket();
}

HttpServer::Connection::Connection(HttpServer* server, ListenSocket* sock)
    : server_(server),
      socket_(sock),
      is_web_socket_(false) {
  id_ = lastId_++;
}

HttpServer::Connection::~Connection() {
  DetachSocket();
  server_->delegate_->OnClose(id_);
}

void HttpServer::Connection::DetachSocket() {
  socket_ = NULL;
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
  INPUT_00,
  INPUT_FF,
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
  ST_WS_READY,   // Ready to receive web socket frame
  ST_WS_FRAME,   // Receiving WebSocket frame
  ST_WS_CLOSE,   // Closing the connection WebSocket connection
  ST_DONE,       // Parsing is complete and successful
  ST_ERR,        // Parsing encountered invalid syntax.
  MAX_STATES
};

// State transition table
int parser_state[MAX_STATES][MAX_INPUTS] = {
/* METHOD    */ { ST_URL,       ST_ERR,      ST_ERR,      ST_ERR,       ST_ERR,      ST_ERR,      ST_METHOD },
/* URL       */ { ST_PROTO,     ST_ERR,      ST_ERR,      ST_URL,       ST_ERR,      ST_ERR,      ST_URL },
/* PROTOCOL  */ { ST_ERR,       ST_HEADER,   ST_NAME,     ST_ERR,       ST_ERR,      ST_ERR,      ST_PROTO },
/* HEADER    */ { ST_ERR,       ST_ERR,      ST_NAME,     ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR },
/* NAME      */ { ST_SEPARATOR, ST_DONE,     ST_ERR,      ST_SEPARATOR, ST_ERR,      ST_ERR,      ST_NAME },
/* SEPARATOR */ { ST_SEPARATOR, ST_ERR,      ST_ERR,      ST_SEPARATOR, ST_ERR,      ST_ERR,      ST_VALUE },
/* VALUE     */ { ST_VALUE,     ST_HEADER,   ST_NAME,     ST_VALUE,     ST_ERR,      ST_ERR,      ST_VALUE },
/* WS_READY  */ { ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR,       ST_WS_FRAME, ST_WS_CLOSE, ST_ERR},
/* WS_FRAME  */ { ST_WS_FRAME,  ST_WS_FRAME, ST_WS_FRAME, ST_WS_FRAME,  ST_ERR,      ST_WS_READY, ST_WS_FRAME },
/* WS_CLOSE  */ { ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR,       ST_WS_CLOSE, ST_ERR,      ST_ERR },
/* DONE      */ { ST_DONE,      ST_DONE,     ST_DONE,     ST_DONE,      ST_DONE,     ST_DONE,     ST_DONE },
/* ERR       */ { ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR }
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
    case 0x0:
      return INPUT_00;
    case static_cast<char>(-1):
      return INPUT_FF;
  }
  return INPUT_DEFAULT;
}

bool HttpServer::ParseHeaders(Connection* connection,
                              HttpServerRequestInfo* info) {
  int pos = 0;
  int data_len = connection->recv_data_.length();
  int state = connection->is_web_socket_ ? ST_WS_READY : ST_METHOD;
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
        case ST_WS_FRAME:
          connection->recv_data_ = connection->recv_data_.substr(pos);
          info->data = buffer;
          buffer.clear();
          return true;
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
        case ST_WS_FRAME:
          buffer.append(&ch, 1);
          break;
        case ST_DONE:
          connection->recv_data_ = connection->recv_data_.substr(pos);
          info->data = connection->recv_data_;
          connection->recv_data_.clear();
          return true;
        case ST_WS_CLOSE:
          connection->is_web_socket_ = false;
          return false;
        case ST_ERR:
          return false;
      }
    }
  }
  // No more characters, but we haven't finished parsing yet.
  return false;
}

void HttpServer::DidAccept(ListenSocket* server,
                           ListenSocket* socket) {
  Connection* connection = new Connection(this, socket);
  id_to_connection_[connection->id_] = connection;
  socket_to_connection_[socket] = connection;
}

void HttpServer::DidRead(ListenSocket* socket,
                         const char* data,
                         int len) {
  Connection* connection = FindConnection(socket);
  DCHECK(connection != NULL);
  if (connection == NULL)
    return;

  connection->recv_data_.append(data, len);
  while (connection->recv_data_.length()) {
    HttpServerRequestInfo request;
    if (!ParseHeaders(connection, &request))
      break;

    if (connection->is_web_socket_) {
      delegate_->OnWebSocketMessage(connection->id_, request.data);
      continue;
    }

    std::string connection_header = GetHeaderValue(request, "Connection");
    if (connection_header == "Upgrade") {
      // Is this WebSocket and if yes, upgrade the connection.
      std::string key1 = GetHeaderValue(request, "Sec-WebSocket-Key1");
      std::string key2 = GetHeaderValue(request, "Sec-WebSocket-Key2");
      if (!key1.empty() && !key2.empty()) {
        delegate_->OnWebSocketRequest(connection->id_, request);
        continue;
      }
    }
    delegate_->OnHttpRequest(connection->id_, request);
  }

}

void HttpServer::DidClose(ListenSocket* socket) {
  Connection* connection = FindConnection(socket);
  DCHECK(connection != NULL);
  id_to_connection_.erase(connection->id_);
  socket_to_connection_.erase(connection->socket_);
  delete connection;
}

HttpServer::Connection* HttpServer::FindConnection(int connection_id) {
  IdToConnectionMap::iterator it = id_to_connection_.find(connection_id);
  if (it == id_to_connection_.end())
    return NULL;
  return it->second;
}

HttpServer::Connection* HttpServer::FindConnection(ListenSocket* socket) {
  SocketToConnectionMap::iterator it = socket_to_connection_.find(socket);
  if (it == socket_to_connection_.end())
    return NULL;
  return it->second;
}
