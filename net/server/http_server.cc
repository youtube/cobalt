// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/http_server.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_listen_socket.h"
#include "net/server/http_connection.h"
#include "net/server/http_server_request_info.h"
#include "net/server/web_socket.h"

namespace net {

HttpServer::HttpServer(const StreamListenSocketFactory& factory,
                       HttpServer::Delegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(server_(factory.CreateAndListen(this))) {
  if (!server_) {
    DLOG(WARNING) << "Could not start HTTP server.";
  }
}

void HttpServer::AcceptWebSocket(
    int connection_id,
    const HttpServerRequestInfo& request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  DCHECK(connection->web_socket_.get());
  connection->web_socket_->Accept(request);
}

void HttpServer::SendOverWebSocket(int connection_id,
                                   const std::string& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  DCHECK(connection->web_socket_.get());
  connection->web_socket_->Send(data);
}

void HttpServer::Send(int connection_id,
                      HttpStatusCode status_code,
                      const std::string& data,
                      const std::string& content_type,
                      const std::vector<std::string>& headers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  connection->Send(status_code, data, content_type, headers);
}

void HttpServer::Send(int connection_id,
                      HttpStatusCode status_code,
                      const std::string& data,
                      const std::string& content_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Send(connection_id, status_code, data, content_type,
       std::vector<std::string>());
}

void HttpServer::Send200(int connection_id,
                         const std::string& data,
                         const std::string& content_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Send(connection_id, HTTP_OK, data, content_type);
}

void HttpServer::Send302(int connection_id,
                         const std::string& location) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  connection->SendRedirect(location);
}

void HttpServer::Send404(int connection_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Send(connection_id, HTTP_NOT_FOUND, "", "text/html");
}

void HttpServer::Send500(int connection_id, const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Send(connection_id, HTTP_INTERNAL_SERVER_ERROR, message, "text/html");
}

void HttpServer::Close(int connection_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  // Initiating close from server-side does not lead to the DidClose call.
  // Do it manually here.
  DidClose(connection->socket_);
}

int HttpServer::GetLocalAddress(IPEndPoint* address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (server_) {
    return server_->GetLocalAddress(address);
  } else {
    return net::ERR_FAILED;
  }
}

void HttpServer::DidAccept(StreamListenSocket* server,
                           StreamListenSocket* socket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HttpConnection* connection = new HttpConnection(this, socket);
  id_to_connection_[connection->id()] = connection;
  socket_to_connection_[socket] = connection;
}

void HttpServer::DidRead(StreamListenSocket* socket,
                         const char* data,
                         int len) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
    if (!ParseHeaders(connection, &request, &pos)) {
#if defined(COBALT)
      Send500(connection->id(), "Error parsing HTTP headers.");
#endif
      break;
    }

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

    if (request.method == "POST" || request.method == "PUT") {
      // Get the remaining data.
      size_t content_length = 0;
      base::StringToSizeT(
          base::StringPiece(request.GetHeaderValue("Content-Length")),
          &content_length);

      if (pos > connection->recv_data_.size()) {
        NOTREACHED() << "pos should not be outside of recv_data_";
        break;
      }

      size_t payload_size = connection->recv_data_.size() - pos;
      if ((content_length > 0) && (content_length > payload_size)) {
        break;
      }

      if (content_length > 0 && pos < (connection->recv_data_.size())) {
        request.data = connection->recv_data_.substr(pos);
      }
      pos += request.data.length();
    }

    delegate_->OnHttpRequest(connection->id(), request);
    connection->Shift(pos);
  }
}

void HttpServer::DidClose(StreamListenSocket* socket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HttpConnection* connection = FindConnection(socket);
  DCHECK(connection != NULL);
  id_to_connection_.erase(connection->id());
  socket_to_connection_.erase(connection->socket_);
  delete connection;
}

HttpServer::~HttpServer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  STLDeleteContainerPairSecondPointers(
      id_to_connection_.begin(), id_to_connection_.end());
  server_ = NULL;
}

namespace { // anonymous

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
enum HeaderParseInputs {
  INPUT_SPACE,
  INPUT_CR,
  INPUT_LF,
  INPUT_COLON,
  INPUT_DEFAULT,
  MAX_INPUTS,
};

// Parser states.
enum HeaderParseStates {
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
HeaderParseStates parser_state[MAX_STATES][MAX_INPUTS] = {
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
HeaderParseInputs charToInput(char ch) {
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

bool ParseHeadersInternal(const std::string& received_data,
                          HttpServerRequestInfo* info,
                          size_t* ppos) {
  size_t& pos = *ppos;
  size_t data_len = received_data.length();
  HeaderParseStates state = ST_METHOD;
  std::string buffer;
  std::string header_name;
  while (pos < data_len) {
    char ch = received_data[pos++];
    HeaderParseInputs input = charToInput(ch);
    HeaderParseStates next_state = parser_state[state][input];

    bool transition = (next_state != state);
    if (transition) {
      // Do any actions based on state transitions.
      switch (state) {
        case ST_METHOD: {
          info->method = buffer;
          buffer.clear();
          break;
        }
        case ST_URL: {
          info->path = buffer;
          buffer.clear();
          break;
        }
        case ST_PROTO: {
          // TODO(mbelshe): Deal better with parsing protocol.
          bool is_protocol_supported =
              buffer == "HTTP/1.0" || buffer == "HTTP/1.1";
#if defined(COBALT)
          if (!is_protocol_supported) {
            next_state = ST_ERR;
          }
#else
          DCHECK(is_protocol_supported);
#endif
          buffer.clear();
          break;
        }
        case ST_NAME: {
          header_name = buffer;
          buffer.clear();
          break;
        }
        case ST_VALUE: {
          HttpServerRequestInfo::HeadersMap::iterator
            header_entry = info->headers.find(header_name);
          if (header_entry != info->headers.end()) {
            // Combine duplicate headers, as allowed according to:
            // http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
            header_entry->second += "," + buffer;
          } else {
            info->headers[header_name] = buffer;
          }
          buffer.clear();
          break;
        }
        case ST_SEPARATOR: {
          buffer.append(&ch, 1);
          break;
        }
        default: {
          break;
        }
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
        default:
          break;
      }
    }
  }
  // No more characters, but we haven't finished parsing yet.
  return false;
}
} // anonymous namespace

bool HttpServer::ParseHeaders(HttpConnection* connection,
                              HttpServerRequestInfo* info,
                              size_t* ppos) {
  return ParseHeadersInternal(connection->recv_data_, info, ppos);
}

// static
bool HttpServer::ParseHeaders(const std::string& received_data,
                              HttpServerRequestInfo* info) {
  size_t pos = 0;
  return ParseHeadersInternal(received_data, info, &pos);
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
