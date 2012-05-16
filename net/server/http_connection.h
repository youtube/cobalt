// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_HTTP_CONNECTION_H_
#define NET_SERVER_HTTP_CONNECTION_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace net {

class HttpServer;
class StreamListenSocket;
class WebSocket;

class HttpConnection {
 public:
  void Send(const std::string& data);
  void Send(const char* bytes, int len);
  void Send200(const std::string& data, const std::string& content_type);
  void Send404();
  void Send500(const std::string& message);

  void Shift(int num_bytes);

  const std::string& recv_data() const { return recv_data_; }
  int id() const { return id_; }

 private:
  friend class HttpServer;
  static int last_id_;

  HttpConnection(HttpServer* server, StreamListenSocket* sock);
  ~HttpConnection();

  void DetachSocket();

  HttpServer* server_;
  scoped_refptr<StreamListenSocket> socket_;
  scoped_ptr<WebSocket> web_socket_;
  std::string recv_data_;
  int id_;
  DISALLOW_COPY_AND_ASSIGN(HttpConnection);
};

}  // namespace net

#endif  // NET_SERVER_HTTP_CONNECTION_H_
