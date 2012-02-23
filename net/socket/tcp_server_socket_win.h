// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SERVER_SOCKET_WIN_H_
#define NET_SOCKET_TCP_SERVER_SOCKET_WIN_H_

#include <winsock2.h>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/win/object_watcher.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/socket/server_socket.h"

namespace net {

class IPEndPoint;

class NET_EXPORT_PRIVATE TCPServerSocketWin
    : public ServerSocket,
      NON_EXPORTED_BASE(public base::NonThreadSafe),
      public base::win::ObjectWatcher::Delegate  {
 public:
  TCPServerSocketWin(net::NetLog* net_log,
                     const net::NetLog::Source& source);
  ~TCPServerSocketWin();

  // net::ServerSocket implementation.
  virtual int Listen(const net::IPEndPoint& address, int backlog);
  virtual int GetLocalAddress(IPEndPoint* address) const;
  virtual int Accept(scoped_ptr<StreamSocket>* socket,
                     const CompletionCallback& callback);

  // base::ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object);

 private:
  int AcceptInternal(scoped_ptr<StreamSocket>* socket);
  void Close();

  SOCKET socket_;
  HANDLE socket_event_;

  base::win::ObjectWatcher accept_watcher_;

  scoped_ptr<StreamSocket>* accept_socket_;
  CompletionCallback accept_callback_;

  BoundNetLog net_log_;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SERVER_SOCKET_WIN_H_
