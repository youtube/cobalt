// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SERVER_SOCKET_LIBEVENT_H_
#define NET_SOCKET_TCP_SERVER_SOCKET_LIBEVENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/socket/server_socket.h"

namespace net {

class IPEndPoint;

class NET_EXPORT_PRIVATE TCPServerSocketLibevent
    : public ServerSocket,
      public base::NonThreadSafe,
      public MessageLoopForIO::Watcher {
 public:
  TCPServerSocketLibevent(net::NetLog* net_log,
                          const net::NetLog::Source& source);
  virtual ~TCPServerSocketLibevent();

  // net::ServerSocket implementation.
  virtual int Listen(const net::IPEndPoint& address, int backlog) OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual int Accept(scoped_ptr<StreamSocket>* socket,
                     const CompletionCallback& callback) OVERRIDE;

  // MessageLoopForIO::Watcher implementation.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  int AcceptInternal(scoped_ptr<StreamSocket>* socket);
  void Close();

  int socket_;

  MessageLoopForIO::FileDescriptorWatcher accept_socket_watcher_;

  scoped_ptr<StreamSocket>* accept_socket_;
  CompletionCallback accept_callback_;

  BoundNetLog net_log_;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SERVER_SOCKET_LIBEVENT_H_
