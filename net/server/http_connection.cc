// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/http_connection.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/base/stream_listen_socket.h"
#include "net/server/http_server.h"
#include "net/server/web_socket.h"

namespace net {

int HttpConnection::last_id_ = 0;

void HttpConnection::Send(const std::string& data) {
  if (!socket_)
    return;
  socket_->Send(data);
}

void HttpConnection::Send(const char* bytes, int len) {
  if (!socket_)
    return;
  socket_->Send(bytes, len);
}

void HttpConnection::Send(HttpStatusCode status_code,
                          const std::string& data,
                          const std::string& content_type) {
  Send(status_code, data, content_type, std::vector<std::string>());
}

void HttpConnection::Send(HttpStatusCode status_code,
                          const std::string& data,
                          const std::string& content_type,
                          const std::vector<std::string>& headers) {
  if (!socket_)
    return;

  std::string status_message;
  switch (status_code) {
    case HTTP_OK:
      status_message = "OK";
      break;
    case HTTP_CREATED:
      status_message = "CREATED";
      break;
    case HTTP_NOT_FOUND:
      status_message = "Not Found";
      break;
    case HTTP_INTERNAL_SERVER_ERROR:
      status_message = "Internal Error";
      break;
    default:
      status_message = "";
      break;
  }

  socket_->Send(base::StringPrintf(
      "HTTP/1.1 %d %s\r\n"
      "Content-Type:%s\r\n"
      "Content-Length:%d\r\n",
      status_code, status_message.c_str(),
      content_type.c_str(),
      static_cast<int>(data.length())));

  // Custom headers. Since JoinString does not add a trailing delimiter,
  // add one manually.
  if (!headers.empty()) {
    socket_->Send(JoinString(headers, "\r\n"));
    socket_->Send("\r\n", 2);
  }

  // End linefeed and body.
  socket_->Send("\r\n", 2);
  socket_->Send(data);
}

void HttpConnection::SendRedirect(const std::string& location) {
  socket_->Send(base::StringPrintf(
      "HTTP/1.1 %d %s\r\n"
      "Location: %s\r\n"
      "Content-Length: 0\r\n"
      "\r\n",
      HTTP_FOUND, "Found", location.c_str()));
}

HttpConnection::HttpConnection(HttpServer* server, StreamListenSocket* sock)
    : server_(server),
      socket_(sock) {
  id_ = last_id_++;
}

HttpConnection::~HttpConnection() {
  DetachSocket();
  server_->delegate_->OnClose(id_);
}

void HttpConnection::DetachSocket() {
  socket_ = NULL;
}

void HttpConnection::Shift(int num_bytes) {
  recv_data_ = recv_data_.substr(num_bytes);
}

}  // namespace net
