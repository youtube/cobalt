// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_HTTP_SERVER_H_
#define NET_SERVER_HTTP_SERVER_H_
#pragma once

#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "net/base/listen_socket.h"

class HttpServerRequestInfo;

class HttpServer : public ListenSocket::ListenSocketDelegate,
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
  virtual ~HttpServer();

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

private:
  friend class base::RefCountedThreadSafe<HttpServer>;
  class Connection {
   private:
    static int lastId_;
    friend class HttpServer;

    explicit Connection(HttpServer* server, ListenSocket* sock);
    ~Connection();

    void DetachSocket();

    void Shift(int num_bytes);

    HttpServer* server_;
    scoped_refptr<ListenSocket> socket_;
    bool is_web_socket_;
    std::string recv_data_;
    int id_;

    DISALLOW_COPY_AND_ASSIGN(Connection);
  };
  friend class Connection;


  // ListenSocketDelegate
  virtual void DidAccept(ListenSocket* server, ListenSocket* socket);
  virtual void DidRead(ListenSocket* socket, const char* data, int len);
  virtual void DidClose(ListenSocket* socket);

  // Expects the raw data to be stored in recv_data_. If parsing is successful,
  // will remove the data parsed from recv_data_, leaving only the unused
  // recv data.
  bool ParseHeaders(Connection* connection,
                    HttpServerRequestInfo* info,
                    int* ppos);

  Connection* FindConnection(int connection_id);
  Connection* FindConnection(ListenSocket* socket);

  HttpServer::Delegate* delegate_;
  scoped_refptr<ListenSocket> server_;
  typedef std::map<int, Connection*> IdToConnectionMap;
  IdToConnectionMap id_to_connection_;
  typedef std::map<ListenSocket*, Connection*> SocketToConnectionMap;
  SocketToConnectionMap socket_to_connection_;

  DISALLOW_COPY_AND_ASSIGN(HttpServer);
};

#endif // NET_SERVER_HTTP_SERVER_H_
