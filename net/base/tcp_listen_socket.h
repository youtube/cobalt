// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TCP/IP server that handles IO asynchronously in the specified MessageLoop.
// These objects are NOT thread safe.  They use WSAEVENT handles to monitor
// activity in a given MessageLoop.  This means that callbacks will
// happen in that loop's thread always and that all other methods (including
// constructors and destructors) should also be called from the same thread.

#ifndef NET_BASE_TCP_LISTEN_SOCKET_H_
#define NET_BASE_TCP_LISTEN_SOCKET_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#endif
#include <string>
#if defined(OS_WIN)
#include "base/win/object_watcher.h"
#elif defined(OS_POSIX)
#include "base/message_loop.h"
#endif

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/listen_socket.h"
#include "net/base/net_export.h"

#if defined(OS_POSIX)
typedef int SOCKET;
#endif

namespace net {

// Implements a TCP socket interface. Note that this is ref counted.
class NET_EXPORT TCPListenSocket : public ListenSocket,
#if defined(OS_WIN)
                                   public base::win::ObjectWatcher::Delegate {
#elif defined(OS_POSIX)
                                   public MessageLoopForIO::Watcher {
#endif
 public:
  // Listen on port for the specified IP address.  Use 127.0.0.1 to only
  // accept local connections.
  static TCPListenSocket* CreateAndListen(const std::string& ip, int port,
                                          ListenSocketDelegate* del);

  // NOTE: This is for unit test use only!
  // Pause/Resume calling Read().  Note that ResumeReads() will also call
  // Read() if there is anything to read.
  void PauseReads();
  void ResumeReads();

 protected:
  enum WaitState {
    NOT_WAITING      = 0,
    WAITING_ACCEPT   = 1,
    WAITING_READ     = 2
  };

  static const SOCKET kInvalidSocket;
  static const int kSocketError;

  TCPListenSocket(SOCKET s, ListenSocketDelegate* del);
  virtual ~TCPListenSocket();
  static SOCKET CreateAndBind(const std::string& ip, int port);
  // if valid, returned SOCKET is non-blocking
  static SOCKET Accept(SOCKET s);

  // Implements ListenSocket::SendInternal.
  virtual void SendInternal(const char* bytes, int len) OVERRIDE;

  virtual void Listen();
  virtual void Accept();
  virtual void Read();
  virtual void Close();
  virtual void CloseSocket(SOCKET s);

  // Pass any value in case of Windows, because in Windows
  // we are not using state.
  void WatchSocket(WaitState state);
  void UnwatchSocket();

#if defined(OS_WIN)
  // ObjectWatcher delegate
  virtual void OnObjectSignaled(HANDLE object);
  base::win::ObjectWatcher watcher_;
  HANDLE socket_event_;
#elif defined(OS_POSIX)
  // Called by MessagePumpLibevent when the socket is ready to do I/O
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;
  WaitState wait_state_;
  // The socket's libevent wrapper
  MessageLoopForIO::FileDescriptorWatcher watcher_;
#endif

  SOCKET socket_;

 private:
  bool reads_paused_;
  bool has_pending_reads_;

  DISALLOW_COPY_AND_ASSIGN(TCPListenSocket);
};

}  // namespace net

#endif  // NET_BASE_TCP_LISTEN_SOCKET_H_
