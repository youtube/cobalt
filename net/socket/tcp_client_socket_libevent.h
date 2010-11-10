// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CLIENT_SOCKET_LIBEVENT_H_
#define NET_SOCKET_TCP_CLIENT_SOCKET_LIBEVENT_H_
#pragma once

#include "base/message_loop.h"
#include "base/non_thread_safe.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/socket/client_socket.h"

struct event;  // From libevent

namespace net {

class BoundNetLog;

// A client socket that uses TCP as the transport layer.
class TCPClientSocketLibevent : public ClientSocket, NonThreadSafe {
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  TCPClientSocketLibevent(const AddressList& addresses,
                          net::NetLog* net_log,
                          const net::NetLog::Source& source);

  virtual ~TCPClientSocketLibevent();

  // AdoptSocket causes the given, connected socket to be adopted as a TCP
  // socket. This object must not be connected. This object takes ownership of
  // the given socket and then acts as if Connect() had been called. This
  // function is intended for testing only.
  void AdoptSocket(int socket);

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual int GetPeerAddress(AddressList* address) const;
  virtual const BoundNetLog& NetLog() const { return net_log_; }
  virtual void SetSubresourceSpeculation();
  virtual void SetOmniboxSpeculation();
  virtual bool WasEverUsed() const;
  virtual bool UsingTCPFastOpen() const;

  // Socket methods:
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  // State machine for connecting the socket.
  enum ConnectState {
    CONNECT_STATE_CONNECT,
    CONNECT_STATE_CONNECT_COMPLETE,
    CONNECT_STATE_NONE,
  };

  class ReadWatcher : public MessageLoopForIO::Watcher {
   public:
    explicit ReadWatcher(TCPClientSocketLibevent* socket) : socket_(socket) {}

    // MessageLoopForIO::Watcher methods

    virtual void OnFileCanReadWithoutBlocking(int /* fd */) {
      if (socket_->read_callback_)
        socket_->DidCompleteRead();
    }

    virtual void OnFileCanWriteWithoutBlocking(int /* fd */) {}

   private:
    TCPClientSocketLibevent* const socket_;

    DISALLOW_COPY_AND_ASSIGN(ReadWatcher);
  };

  class WriteWatcher : public MessageLoopForIO::Watcher {
   public:
    explicit WriteWatcher(TCPClientSocketLibevent* socket) : socket_(socket) {}

    // MessageLoopForIO::Watcher methods

    virtual void OnFileCanReadWithoutBlocking(int /* fd */) {}

    virtual void OnFileCanWriteWithoutBlocking(int /* fd */) {
      if (socket_->waiting_connect()) {
        socket_->DidCompleteConnect();
      } else if (socket_->write_callback_) {
        socket_->DidCompleteWrite();
      }
    }

   private:
    TCPClientSocketLibevent* const socket_;

    DISALLOW_COPY_AND_ASSIGN(WriteWatcher);
  };

  // State machine used by Connect().
  int DoConnectLoop(int result);
  int DoConnect();
  int DoConnectComplete(int result);

  // Helper used by Disconnect(), which disconnects minus the logging and
  // resetting of current_ai_.
  void DoDisconnect();

  void DoReadCallback(int rv);
  void DoWriteCallback(int rv);
  void DidCompleteRead();
  void DidCompleteWrite();
  void DidCompleteConnect();

  // Returns true if a Connect() is in progress.
  bool waiting_connect() const {
    return next_connect_state_ != CONNECT_STATE_NONE;
  }

  // Returns the OS error code (or 0 on success).
  int CreateSocket(const struct addrinfo* ai);

  // Returns the OS error code (or 0 on success).
  int SetupSocket();

  // Helper to add a TCP_CONNECT (end) event to the NetLog.
  void LogConnectCompletion(int net_error);

  // Internal function to write to a socket.
  int InternalWrite(IOBuffer* buf, int buf_len);

  int socket_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list, or NULL if all addrinfos have been tried.
  const struct addrinfo* current_ai_;

  // The socket's libevent wrappers
  MessageLoopForIO::FileDescriptorWatcher read_socket_watcher_;
  MessageLoopForIO::FileDescriptorWatcher write_socket_watcher_;

  // The corresponding watchers for reads and writes.
  ReadWatcher read_watcher_;
  WriteWatcher write_watcher_;

  // The buffer used by OnSocketReady to retry Read requests
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;

  // The buffer used by OnSocketReady to retry Write requests
  scoped_refptr<IOBuffer> write_buf_;
  int write_buf_len_;

  // External callback; called when read is complete.
  CompletionCallback* read_callback_;

  // External callback; called when write is complete.
  CompletionCallback* write_callback_;

  // The next state for the Connect() state machine.
  ConnectState next_connect_state_;

  // The OS error that CONNECT_STATE_CONNECT last completed with.
  int connect_os_error_;

  BoundNetLog net_log_;

  // This socket was previously disconnected and has not been re-connected.
  bool previously_disconnected_;

  // Record of connectivity and transmissions, for use in speculative connection
  // histograms.
  UseHistory use_history_;

  // Enables experimental TCP FastOpen option.
  bool use_tcp_fastopen_;

  // True when TCP FastOpen is in use and we have done the connect.
  bool tcp_fastopen_connected_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketLibevent);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CLIENT_SOCKET_LIBEVENT_H_
