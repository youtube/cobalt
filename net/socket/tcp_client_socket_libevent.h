// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CLIENT_SOCKET_LIBEVENT_H_
#define NET_SOCKET_TCP_CLIENT_SOCKET_LIBEVENT_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/socket/stream_socket.h"

namespace net {

class BoundNetLog;

// A client socket that uses TCP as the transport layer.
class NET_EXPORT_PRIVATE TCPClientSocketLibevent : public StreamSocket,
                                                   public base::NonThreadSafe {
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
  // function is used by TCPServerSocket() to adopt accepted connections
  // and for testing.
  int AdoptSocket(int socket);

  // Binds the socket to a local IP address and port.
  int Bind(const IPEndPoint& address);

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(AddressList* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE;

  // Socket implementation.
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

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

    virtual void OnFileCanReadWithoutBlocking(int /* fd */) OVERRIDE {
      if (!socket_->read_callback_.is_null())
        socket_->DidCompleteRead();
    }

    virtual void OnFileCanWriteWithoutBlocking(int /* fd */) OVERRIDE {}

   private:
    TCPClientSocketLibevent* const socket_;

    DISALLOW_COPY_AND_ASSIGN(ReadWatcher);
  };

  class WriteWatcher : public MessageLoopForIO::Watcher {
   public:
    explicit WriteWatcher(TCPClientSocketLibevent* socket) : socket_(socket) {}

    // MessageLoopForIO::Watcher implementation.
    virtual void OnFileCanReadWithoutBlocking(int /* fd */) OVERRIDE {}
    virtual void OnFileCanWriteWithoutBlocking(int /* fd */) OVERRIDE {
      if (socket_->waiting_connect()) {
        socket_->DidCompleteConnect();
      } else if (!socket_->write_callback_.is_null()) {
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
  // resetting of current_address_index_.
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

  // Helper to add a TCP_CONNECT (end) event to the NetLog.
  void LogConnectCompletion(int net_error);

  // Internal function to write to a socket.
  int InternalWrite(IOBuffer* buf, int buf_len);

  int socket_;

  // Local IP address and port we are bound to. Set to NULL if Bind()
  // was't called (in that cases OS chooses address/port).
  scoped_ptr<IPEndPoint> bind_address_;

  // Stores bound socket between Bind() and Connect() calls.
  int bound_socket_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list. Set to -1 if uninitialized.
  int current_address_index_;

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
  CompletionCallback read_callback_;

  // External callback; called when write is complete.
  CompletionCallback write_callback_;

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

  base::TimeTicks connect_start_time_;
  base::TimeDelta connect_time_micros_;
  int64 num_bytes_read_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketLibevent);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CLIENT_SOCKET_LIBEVENT_H_
