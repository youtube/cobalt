// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CLIENT_SOCKET_WIN_H_
#define NET_SOCKET_TCP_CLIENT_SOCKET_WIN_H_
#pragma once

#include <winsock2.h>

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/socket/stream_socket.h"

namespace net {

class BoundNetLog;

class NET_EXPORT TCPClientSocketWin : public StreamSocket,
                                      NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  TCPClientSocketWin(const AddressList& addresses,
                     net::NetLog* net_log,
                     const net::NetLog::Source& source);

  virtual ~TCPClientSocketWin();

  // AdoptSocket causes the given, connected socket to be adopted as a TCP
  // socket. This object must not be connected. This object takes ownership of
  // the given socket and then acts as if Connect() had been called. This
  // function is used by TCPServerSocket() to adopt accepted connections
  // and for testing.
  int AdoptSocket(SOCKET socket);

  // Binds the socket to a local IP address and port.
  int Bind(const IPEndPoint& address);

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual int GetPeerAddress(AddressList* address) const;
  virtual int GetLocalAddress(IPEndPoint* address) const;
  virtual const BoundNetLog& NetLog() const { return net_log_; }
  virtual void SetSubresourceSpeculation();
  virtual void SetOmniboxSpeculation();
  virtual bool WasEverUsed() const;
  virtual bool UsingTCPFastOpen() const;
  virtual int64 NumBytesRead() const;
  virtual base::TimeDelta GetConnectTimeMicros() const;
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE;

  // Socket implementation.
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback);
  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback);

  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  // State machine for connecting the socket.
  enum ConnectState {
    CONNECT_STATE_CONNECT,
    CONNECT_STATE_CONNECT_COMPLETE,
    CONNECT_STATE_NONE,
  };

  class Core;

  // State machine used by Connect().
  int DoConnectLoop(int result);
  int DoConnect();
  int DoConnectComplete(int result);

  // Helper used by Disconnect(), which disconnects minus the logging and
  // resetting of current_address_index_.
  void DoDisconnect();

  // Returns true if a Connect() is in progress.
  bool waiting_connect() const {
    return next_connect_state_ != CONNECT_STATE_NONE;
  }

  // Called after Connect() has completed with |net_error|.
  void LogConnectCompletion(int net_error);

  void DoReadCallback(int rv);
  void DoWriteCallback(int rv);
  void DidCompleteConnect();
  void DidCompleteRead();
  void DidCompleteWrite();

  SOCKET socket_;

  // Local IP address and port we are bound to. Set to NULL if Bind()
  // was't called (in that cases OS chooses address/port).
  scoped_ptr<IPEndPoint> bind_address_;

  // Stores bound socket between Bind() and Connect() calls.
  SOCKET bound_socket_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list. Set to -1 if uninitialized.
  int current_address_index_;

  // The various states that the socket could be in.
  bool waiting_read_;
  bool waiting_write_;

  // The core of the socket that can live longer than the socket itself. We pass
  // resources to the Windows async IO functions and we have to make sure that
  // they are not destroyed while the OS still references them.
  scoped_refptr<Core> core_;

  // External callback; called when connect or read is complete.
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

  base::TimeTicks connect_start_time_;
  base::TimeDelta connect_time_micros_;
  int64 num_bytes_read_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketWin);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CLIENT_SOCKET_WIN_H_
