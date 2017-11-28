/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Datagram-based listen socket implementation that handles reading and writing
// to the socket, but does not handle creating the socket, which is handled
// by a UdpSocketFactory class.


#ifndef NET_BASE_DATAGRAM_LISTEN_SOCKET_H_
#define NET_BASE_DATAGRAM_LISTEN_SOCKET_H_

#include "build/build_config.h"

#include <list>
#include <string>

#if defined(__LB_SHELL__) && !defined(__LB_ANDROID__)
#include "base/object_watcher_shell.h"
#endif

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "net/base/net_export.h"
#include "net/base/ip_endpoint.h"

#if defined(OS_POSIX)
typedef int SocketDescriptor;
#elif defined(OS_STARBOARD)
typedef SbSocket SocketDescriptor;
#endif

namespace net {

class NET_EXPORT UDPListenSocket
    : public base::RefCountedThreadSafe<UDPListenSocket>
#if defined(__LB_SHELL__) && !defined(__LB_ANDROID__)
      ,
      public base::ObjectWatcher::Delegate
#elif defined(OS_STARBOARD)
      ,
      public MessageLoopForIO::Watcher
#endif
{
 public:
  class NET_EXPORT Delegate {
   public:
    virtual void DidRead(UDPListenSocket* server,
                         const char* data,
                         int len,
                         const IPEndPoint* address) = 0;
    virtual void DidClose(UDPListenSocket* sock) = 0;

   protected:
    virtual ~Delegate() {}
  };

  UDPListenSocket(SocketDescriptor s, Delegate* del);

  void SendTo(const IPEndPoint& address, const char* bytes, int len);
  void SendTo(const IPEndPoint& address, const std::string& str);

  static const SocketDescriptor kInvalidSocket;
 protected:
  static const int kSocketError;
  Delegate* const socket_delegate_;

  void Close();
  void CloseSocket(SocketDescriptor s);
  void Read();
  void WatchSocket();
  void UnwatchSocket();

 private:
  friend class base::RefCountedThreadSafe<UDPListenSocket>;
  virtual ~UDPListenSocket();

  const SocketDescriptor socket_;
  bool send_error_;
  scoped_array<char> buffer_;

#if defined(__LB_SHELL__) && !defined(__LB_ANDROID__)
  virtual void OnObjectSignaled(int object) override;
  base::ObjectWatcher watcher_;
#elif defined(OS_STARBOARD)
  void OnSocketReadyToRead(SbSocket /*socket*/) override;
  void OnSocketReadyToWrite(SbSocket /*socket*/) override {}
  MessageLoopForIO::SocketWatcher watcher_;
#endif

  DISALLOW_COPY_AND_ASSIGN(UDPListenSocket);
};

}
#endif // NET_BASE_DATAGRAM_LISTEN_SOCKET_H_
