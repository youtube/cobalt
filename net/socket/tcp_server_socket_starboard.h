// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NET_SOCKET_TCP_SERVER_SOCKET_STARBOARD_H_
#define NET_SOCKET_TCP_SERVER_SOCKET_STARBOARD_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/socket/server_socket.h"

namespace net {

class IPEndPoint;

class NET_EXPORT_PRIVATE TCPServerSocketStarboard
    : public ServerSocket,
      public base::NonThreadSafe,
      public MessageLoopForIO::Watcher {
 public:
  TCPServerSocketStarboard(net::NetLog* net_log,
                           const net::NetLog::Source& source);
  virtual ~TCPServerSocketStarboard();

  // net::ServerSocket implementation.
  virtual void AllowAddressReuse() override;
  virtual int Listen(const net::IPEndPoint& address, int backlog) override;
  virtual int GetLocalAddress(IPEndPoint* address) const override;
  virtual int Accept(scoped_ptr<StreamSocket>* socket,
                     const CompletionCallback& callback) override;

  // MessageLoopForIO::Watcher implementation.
  virtual void OnSocketReadyToRead(SbSocket socket) override;
  virtual void OnSocketReadyToWrite(SbSocket socket) override;

 private:
  int SetSocketOptions();
  int AcceptInternal(scoped_ptr<StreamSocket>* socket);
  void Close();

  SbSocket socket_;

  MessageLoopForIO::SocketWatcher accept_socket_watcher_;

  scoped_ptr<StreamSocket>* accept_socket_;
  CompletionCallback accept_callback_;

  bool reuse_address_;

  BoundNetLog net_log_;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SERVER_SOCKET_STARBOARD_H_
