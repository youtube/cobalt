// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SERVER_SOCKET_LIBEVENT_H_
#define NET_SOCKET_TCP_SERVER_SOCKET_LIBEVENT_H_

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/socket/server_socket.h"

namespace net {

class IPEndPoint;

class TCPServerSocketLibevent : public ServerSocket,
                                public base::NonThreadSafe,
                                public MessageLoopForIO::Watcher {
 public:
  TCPServerSocketLibevent(net::NetLog* net_log,
                          const net::NetLog::Source& source);
  ~TCPServerSocketLibevent();

  // net::ServerSocket implementation.
  virtual int Listen(const net::IPEndPoint& address, int backlog);
  virtual int GetLocalAddress(IPEndPoint* address) const;
  virtual int Accept(scoped_ptr<ClientSocket>* socket,
                     CompletionCallback* callback);

  // MessageLoopForIO::Watcher implementation.
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd);

 private:
  int AcceptInternal(scoped_ptr<ClientSocket>* socket);
  void Close();

  int socket_;

  MessageLoopForIO::FileDescriptorWatcher accept_socket_watcher_;

  scoped_ptr<ClientSocket>* accept_socket_;
  CompletionCallback* accept_callback_;

  BoundNetLog net_log_;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SERVER_SOCKET_LIBEVENT_H_
