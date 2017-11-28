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

// Adapted from udp_socket_libevent.h

#ifndef NET_UDP_UDP_SOCKET_STARBOARD_H_
#define NET_UDP_UDP_SOCKET_STARBOARD_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/rand_callback.h"
#include "net/udp/datagram_socket.h"
#include "starboard/socket.h"

namespace net {

class NET_EXPORT UDPSocketStarboard : public base::NonThreadSafe {
 public:
  UDPSocketStarboard(DatagramSocket::BindType bind_type,
                     const RandIntCallback& rand_int_cb,
                     net::NetLog* net_log,
                     const net::NetLog::Source& source);
  virtual ~UDPSocketStarboard();

  // Connect the socket to the host at |address|.
  // Returns a net error code.
  Error Connect(const IPEndPoint& address);

  // Bind the address/port for this socket to |address|.  This is generally
  // only used on a server.
  // Returns a net error code.
  Error Bind(const IPEndPoint& address);

  // Close the socket.
  void Close();

  // Copy the remote udp address into |address| and return a network error code.
  Error GetPeerAddress(IPEndPoint* address) const;

  // Copy the local udp address into |address| and return a network error code.
  // (similar to getsockname)
  Error GetLocalAddress(IPEndPoint* address) const;

  // IO:
  // Multiple outstanding read requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported

  // Read from the socket.
  // Only usable from the client-side of a UDP socket, after the socket
  // has been connected.
  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback);

  // Write to the socket.
  // Only usable from the client-side of a UDP socket, after the socket
  // has been connected.
  int Write(IOBuffer* buf, int buf_len, const CompletionCallback& callback);

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
               IPEndPoint* address,
               const CompletionCallback& callback);

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
             const IPEndPoint& address,
             const CompletionCallback& callback);

  // Set the receive buffer size (in bytes) for the socket.
  bool SetReceiveBufferSize(int32 size);

  // Set the send buffer size (in bytes) for the socket.
  bool SetSendBufferSize(int32 size);

  // Returns true if the socket is already connected or bound.
  bool is_connected() const { return socket_ != kSbSocketInvalid; }

  const BoundNetLog& NetLog() const { return net_log_; }

  // Sets corresponding flags in |socket_options_| to allow the socket
  // to share the local address to which the socket will be bound with
  // other processes. Should be called before Bind().
  void AllowAddressReuse();

  // Sets corresponding flags in |socket_options_| to allow sending
  // and receiving packets to and from broadcast addresses. Should be
  // called before Bind().
  void AllowBroadcast();

 private:
  enum SocketOptions {
    SOCKET_OPTION_REUSE_ADDRESS = 1 << 0,
    SOCKET_OPTION_BROADCAST = 1 << 1
  };

  class ReadWatcher : public MessageLoopForIO::Watcher {
   public:
    explicit ReadWatcher(UDPSocketStarboard* socket) : socket_(socket) {}

    // MessageLoopForIO::Watcher methods

    void OnSocketReadyToRead(SbSocket /*socket*/) override;
    void OnSocketReadyToWrite(SbSocket /*socket*/) override{};

   private:
    UDPSocketStarboard* const socket_;

    DISALLOW_COPY_AND_ASSIGN(ReadWatcher);
  };

  class WriteWatcher : public MessageLoopForIO::Watcher {
   public:
    explicit WriteWatcher(UDPSocketStarboard* socket) : socket_(socket) {}

    // MessageLoopForIO::Watcher methods

    void OnSocketReadyToRead(SbSocket /*socket*/) override{};
    void OnSocketReadyToWrite(SbSocket /*socket*/) override;

   private:
    UDPSocketStarboard* const socket_;

    DISALLOW_COPY_AND_ASSIGN(WriteWatcher);
  };

  void DoReadCallback(int rv);
  void DoWriteCallback(int rv);
  void DidCompleteRead();
  void DidCompleteWrite();

  // Handles stats and logging. |result| is the number of bytes transferred, on
  // success, or the net error code on failure. On success, LogRead takes in a
  // sockaddr and its length, which are mandatory, while LogWrite takes in an
  // optional IPEndPoint.
  void LogRead(int result, const char* bytes, const IPEndPoint* address) const;
  void LogWrite(int result, const char* bytes, const IPEndPoint* address) const;

  // Returns the OS error code (or 0 on success).
  Error CreateSocket(const IPEndPoint& address);

  // Same as SendTo(), except that address is passed by pointer
  // instead of by reference. It is called from Write() with |address|
  // set to NULL.
  int SendToOrWrite(IOBuffer* buf,
                    int buf_len,
                    const IPEndPoint* address,
                    const CompletionCallback& callback);

  Error InternalConnect(const IPEndPoint& address);
  int InternalRecvFrom(IOBuffer* buf, int buf_len, IPEndPoint* address);
  int InternalSendTo(IOBuffer* buf, int buf_len, const IPEndPoint* address);

  // Applies |socket_options_| to |socket_|. Should be called before
  // Bind().
  Error SetSocketOptions();
  Error DoBind(const IPEndPoint& address);
  Error RandomBind(const IPEndPoint& address);

  SbSocket socket_;

  // Bitwise-or'd combination of SocketOptions. Specifies the set of
  // options that should be applied to |socket_| before Bind().
  int socket_options_;

  // How to do source port binding, used only when UDPSocket is part of
  // UDPClientSocket, since UDPServerSocket provides Bind.
  DatagramSocket::BindType bind_type_;

  // PRNG function for generating port numbers.
  RandIntCallback rand_int_cb_;

  // These are mutable since they're just cached copies to make
  // GetPeerAddress/GetLocalAddress smarter.
  mutable scoped_ptr<IPEndPoint> local_address_;
  mutable scoped_ptr<IPEndPoint> remote_address_;

  // The socket's SbSocketWaiter wrappers
  MessageLoopForIO::SocketWatcher read_socket_watcher_;
  MessageLoopForIO::SocketWatcher write_socket_watcher_;

  // The corresponding watchers for reads and writes.
  ReadWatcher read_watcher_;
  WriteWatcher write_watcher_;

  // The buffer used by InternalRead() to retry Read requests
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;
  IPEndPoint* recv_from_address_;

  // The buffer used by InternalWrite() to retry Write requests
  scoped_refptr<IOBuffer> write_buf_;
  int write_buf_len_;
  scoped_ptr<IPEndPoint> send_to_address_;

  // External callback; called when read is complete.
  CompletionCallback read_callback_;

  // External callback; called when write is complete.
  CompletionCallback write_callback_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketStarboard);
};

}  // namespace net

#endif  // NET_UDP_UDP_SOCKET_STARBOARD_H_
