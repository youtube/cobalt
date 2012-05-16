// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_HTTP_SERVER_H_
#define NET_SERVER_HTTP_SERVER_H_
#pragma once

#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "net/base/stream_listen_socket.h"

namespace net {

class HttpConnection;
class HttpServerRequestInfo;
class WebSocket;

class HttpServer : public StreamListenSocket::Delegate,
                   public base::RefCountedThreadSafe<HttpServer> {
 public:
  class Delegate {
   public:
    virtual void OnHttpRequest(int connection_id,
                               const HttpServerRequestInfo& info) = 0;

    virtual void OnWebSocketRequest(int connection_id,
                                    const HttpServerRequestInfo& info) = 0;

    virtual void OnWebSocketMessage(int connection_id,
                                    const std::string& data) = 0;

    virtual void OnClose(int connection_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  HttpServer(const std::string& host, int port, HttpServer::Delegate* del);

  void AcceptWebSocket(int connection_id,
                       const HttpServerRequestInfo& request);
  void SendOverWebSocket(int connection_id, const std::string& data);
  void Send(int connection_id, const std::string& data);
  void Send(int connection_id, const char* bytes, int len);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type);
  void Send404(int connection_id);
  void Send500(int connection_id, const std::string& message);
  void Close(int connection_id);

  // ListenSocketDelegate
  virtual void DidAccept(StreamListenSocket* server,
                         StreamListenSocket* socket) OVERRIDE;
  virtual void DidRead(StreamListenSocket* socket,
                       const char* data,
                       int len) OVERRIDE;
  virtual void DidClose(StreamListenSocket* socket) OVERRIDE;

 protected:
  virtual ~HttpServer();

 private:
  friend class base::RefCountedThreadSafe<HttpServer>;
  friend class HttpConnection;

  // Expects the raw data to be stored in recv_data_. If parsing is successful,
  // will remove the data parsed from recv_data_, leaving only the unused
  // recv data.
  bool ParseHeaders(HttpConnection* connection,
                    HttpServerRequestInfo* info,
                    size_t* pos);

  HttpConnection* FindConnection(int connection_id);
  HttpConnection* FindConnection(StreamListenSocket* socket);

  HttpServer::Delegate* delegate_;
  scoped_refptr<StreamListenSocket> server_;
  typedef std::map<int, HttpConnection*> IdToConnectionMap;
  IdToConnectionMap id_to_connection_;
  typedef std::map<StreamListenSocket*, HttpConnection*> SocketToConnectionMap;
  SocketToConnectionMap socket_to_connection_;

  DISALLOW_COPY_AND_ASSIGN(HttpServer);
};

}  // namespace net

#endif // NET_SERVER_HTTP_SERVER_H_
