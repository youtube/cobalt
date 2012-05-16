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

void HttpConnection::Send200(const std::string& data,
                             const std::string& content_type) {
  if (!socket_)
    return;
  socket_->Send(base::StringPrintf(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type:%s\r\n"
      "Content-Length:%d\r\n"
      "\r\n",
      content_type.c_str(),
      static_cast<int>(data.length())));
  socket_->Send(data);
}

void HttpConnection::Send404() {
  if (!socket_)
    return;
  socket_->Send(
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Length: 0\r\n"
      "\r\n");
}

void HttpConnection::Send500(const std::string& message) {
  if (!socket_)
    return;
  socket_->Send(base::StringPrintf(
      "HTTP/1.1 500 Internal Error\r\n"
      "Content-Type:text/html\r\n"
      "Content-Length:%d\r\n"
      "\r\n"
      "%s",
      static_cast<int>(message.length()),
      message.c_str()));
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
