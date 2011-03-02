// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_UDP_UDP_SOCKET_LIBEVENT_H_
#define NET_UDP_UDP_SOCKET_LIBEVENT_H_
#pragma once

// UDPSocketLibevent
// Accessor API for a UDP socket in either client or server form.
//
// Client form:
// In this case, we're connecting to a specific server, so the client will
// usually use:
//       Connect(address)    // Connect to a UDP server
//       Read/Write          // Reads/Writes all go to a single destination
//
// Server form:
// In this case, we want to read/write to many clients which are connecting
// to this server.  First the server 'binds' to an addres, then we read from
// clients and write responses to them.
// Example:
//       Bind(address/port)  // Binds to port for reading from clients
//       RecvFrom/SendTo     // Each read can come from a different client
//                           // Writes need to be directed to a specific
//                           // address.

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/socket/client_socket.h"
#include "net/udp/datagram_socket.h"

namespace net {

class BoundNetLog;

class UDPSocketLibevent : public base::NonThreadSafe {
 public:
  UDPSocketLibevent(net::NetLog* net_log,
                    const net::NetLog::Source& source);
  virtual ~UDPSocketLibevent();

  // Connect the socket to connect with a certain |address|.
  // Returns a net error code.
  int Connect(const AddressList& address);

  // Bind the address/port for this socket to |address|.  This is generally
  // only used on a server.
  // Returns a net error code.
  int Bind(const AddressList& address);

  // Close the socket.
  void Close();

  // Copy the remote udp address into |address| and return a network error code.
  int GetPeerAddress(AddressList* address) const;

  // Copy the local udp address into |address| and return a network error code.
  // (similar to getsockname)
  int GetLocalAddress(AddressList* address) const;

  // IO:
  // Multiple outstanding read requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported

  // Read from the socket.
  // Only usable from the client-side of a UDP socket, after the socket
  // has been connected.
  int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);

  // Write to the socket.
  // Only usable from the client-side of a UDP socket, after the socket
  // has been connected.
  int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);

  // Read from a socket and receive sender address information.
  // |buf| is the buffer to read data into.
  // |buf_len| is the maximum amount of data to read.
  // |address| is a buffer provided by the caller for receiving the sender
  //   address information about the received data.  This buffer must be kept
  //   alive by the caller until the callback is placed.
  // |address_length| is a ptr to the length of the |address| buffer.  This
  //   is an input parameter containing the maximum size |address| can hold
  //   and also an output parameter for the size of |address| upon completion.
  // |callback| the callback on completion of the Recv.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf|, |address|,
  // and |address_length| alive until the callback is called.
  int RecvFrom(IOBuffer* buf,
               int buf_len,
               struct sockaddr* address,
               socklen_t* address_length,
               CompletionCallback* callback);

  // Send to a socket with a particular destination.
  // |buf| is the buffer to send
  // |buf_len| is the number of bytes to send
  // |address| is the recipient address.
  // |address_length| is the size of the recipient address
  // |callback| is the user callback function to call on complete.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf| and |address|
  // alive until the callback is called.
  int SendTo(IOBuffer* buf,
             int buf_len,
             const struct sockaddr* address,
             socklen_t address_length,
             CompletionCallback* callback);

  // Returns true if the socket is already connected or bound.
  bool is_connected() const { return socket_ != kInvalidSocket; }

  AddressList* local_address() { return local_address_.get(); }

 private:
  static const int kInvalidSocket = -1;

  class ReadWatcher : public MessageLoopForIO::Watcher {
   public:
    explicit ReadWatcher(UDPSocketLibevent* socket) : socket_(socket) {}

    // MessageLoopForIO::Watcher methods

    virtual void OnFileCanReadWithoutBlocking(int /* fd */) {
      if (socket_->read_callback_)
        socket_->DidCompleteRead();
    }

    virtual void OnFileCanWriteWithoutBlocking(int /* fd */) {}

   private:
    UDPSocketLibevent* const socket_;

    DISALLOW_COPY_AND_ASSIGN(ReadWatcher);
  };

  class WriteWatcher : public MessageLoopForIO::Watcher {
   public:
    explicit WriteWatcher(UDPSocketLibevent* socket) : socket_(socket) {}

    // MessageLoopForIO::Watcher methods

    virtual void OnFileCanReadWithoutBlocking(int /* fd */) {}

    virtual void OnFileCanWriteWithoutBlocking(int /* fd */) {
      if (socket_->write_callback_)
        socket_->DidCompleteWrite();
    }

   private:
    UDPSocketLibevent* const socket_;

    DISALLOW_COPY_AND_ASSIGN(WriteWatcher);
  };

  void DoReadCallback(int rv);
  void DoWriteCallback(int rv);
  void DidCompleteRead();
  void DidCompleteWrite();

  // Returns the OS error code (or 0 on success).
  int CreateSocket(const struct addrinfo* ai);

  int InternalRead();
  int InternalWrite(IOBuffer* buf, int buf_len);

  int socket_;

  // These are mutable since they're just cached copies to make
  // GetPeerAddress/GetLocalAddress smarter.
  mutable scoped_ptr<AddressList> local_address_;
  mutable scoped_ptr<AddressList> remote_address_;

  // The socket's libevent wrappers
  MessageLoopForIO::FileDescriptorWatcher read_socket_watcher_;
  MessageLoopForIO::FileDescriptorWatcher write_socket_watcher_;

  // The corresponding watchers for reads and writes.
  ReadWatcher read_watcher_;
  WriteWatcher write_watcher_;

  // The buffer used by OnSocketReady to retry Read requests
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;
  struct sockaddr* recv_from_address_;
  socklen_t* recv_from_address_length_;

  // The buffer used by OnSocketReady to retry Write requests
  scoped_refptr<IOBuffer> write_buf_;
  int write_buf_len_;
  const struct sockaddr* send_to_address_;
  socklen_t send_to_address_length_;

  // External callback; called when read is complete.
  CompletionCallback* read_callback_;

  // External callback; called when write is complete.
  CompletionCallback* write_callback_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketLibevent);
};

}  // namespace net

#endif  // NET_UDP_UDP_SOCKET_LIBEVENT_H_
