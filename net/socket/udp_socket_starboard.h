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

#ifndef NET_SOCKET_UDP_SOCKET_STARBOARD_H_
#define NET_SOCKET_UDP_SOCKET_STARBOARD_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/datagram_buffer.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/rand_callback.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/diff_serv_code_point.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "starboard/common/socket.h"

namespace net {

// Sendresult is inspired by sendmmsg, but unlike sendmmsg it is not
// convenient to require that a positive |write_count| and a negative
// error code are mutually exclusive.
struct NET_EXPORT SendResult {
  explicit SendResult();
  ~SendResult();
  SendResult(int rv, int write_count, DatagramBuffers buffers);
  SendResult(SendResult& other) = delete;
  SendResult& operator=(SendResult& other) = delete;
  SendResult(SendResult&& other);
  SendResult& operator=(SendResult&& other) = default;
  int rv;
  // number of successful writes.
  int write_count;
  DatagramBuffers buffers;
};

// Don't delay writes more than this.
const base::TimeDelta kWriteAsyncMsThreshold =
    base::TimeDelta::FromMilliseconds(1);
// Prefer local if number of writes is not more than this.
const int kWriteAsyncMinBuffersThreshold = 2;
// Don't allow more than this many outstanding async writes.
const int kWriteAsyncMaxBuffersThreshold = 16;
// PostTask immediately when unwritten buffers reaches this.
const int kWriteAsyncPostBuffersThreshold = kWriteAsyncMaxBuffersThreshold / 2;
// Don't unblock writer unless pending async writes are less than this.
const int kWriteAsyncCallbackBuffersThreshold = kWriteAsyncMaxBuffersThreshold;

// To allow mock |Send|/|Sendmsg| in testing.  This has to be
// reference counted thread safe because |SendBuffers| may be invoked in
// another thread via PostTask*.
class NET_EXPORT UDPSocketStarboardSender
    : public base::RefCountedThreadSafe<UDPSocketStarboardSender> {
 public:
  explicit UDPSocketStarboardSender();

  SendResult SendBuffers(const SbSocket& socket,
                         DatagramBuffers buffers,
                         SbSocketAddress address);

 protected:
  friend class base::RefCountedThreadSafe<UDPSocketStarboardSender>;

  virtual ~UDPSocketStarboardSender();
  virtual int Send(const SbSocket& socket,
                   const char* buf,
                   size_t len,
                   SbSocketAddress address) const;

  SendResult InternalSendBuffers(const SbSocket& socket,
                                 DatagramBuffers buffers,
                                 SbSocketAddress address) const;

 private:
  UDPSocketStarboardSender(const UDPSocketStarboardSender&) = delete;
  UDPSocketStarboardSender& operator=(const UDPSocketStarboardSender&) = delete;
};

class NET_EXPORT UDPSocketStarboard
    : public base::MessageLoopCurrentForIO::Watcher {
 public:
  UDPSocketStarboard(DatagramSocket::BindType bind_type,
                     net::NetLog* net_log,
                     const net::NetLogSource& source);
  virtual ~UDPSocketStarboard();

  // Opens the socket.
  // Returns a net error code.
  int Open(AddressFamily address_family);

  // Binds this socket to |network|. All data traffic on the socket will be sent
  // and received via |network|. Must be called before Connect(). This call will
  // fail if |network| has disconnected. Communication using this socket will
  // fail if |network| disconnects.
  // Returns a net error code.
  int BindToNetwork(NetworkChangeNotifier::NetworkHandle network);

  // Connect the socket to the host at |address|.
  // Should be called after Open().
  // Returns a net error code.
  int Connect(const IPEndPoint& address);

  // Bind the address/port for this socket to |address|.  This is generally
  // only used on a server.  Should be called after Open().
  // Returns a net error code.
  int Bind(const IPEndPoint& address);

  // Close the socket.
  void Close();

  // Copy the remote udp address into |address| and return a network error code.
  int GetPeerAddress(IPEndPoint* address) const;

  // Copy the local udp address into |address| and return a network error code.
  // (similar to getsockname)
  int GetLocalAddress(IPEndPoint* address) const;

  // IO:
  // Multiple outstanding read requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported

  // Reads from the socket.
  // Only usable from the client-side of a UDP socket, after the socket
  // has been connected.
  int Read(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);

  // Writes to the socket.
  // Only usable from the client-side of a UDP socket, after the socket
  // has been connected.
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation);

  // Refer to datagram_client_socket.h
  int WriteAsync(DatagramBuffers buffers,
                 CompletionOnceCallback callback,
                 const NetworkTrafficAnnotationTag& traffic_annotation);
  int WriteAsync(const char* buffer,
                 size_t buf_len,
                 CompletionOnceCallback callback,
                 const NetworkTrafficAnnotationTag& traffic_annotation);

  DatagramBuffers GetUnwrittenBuffers();

  // Reads from a socket and receive sender address information.
  // |buf| is the buffer to read data into.
  // |buf_len| is the maximum amount of data to read.
  // |address| is a buffer provided by the caller for receiving the sender
  //   address information about the received data.  This buffer must be kept
  //   alive by the caller until the callback is placed.
  // |callback| is the callback on completion of the RecvFrom.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf| and |address|
  // alive until the callback is called.
  int RecvFrom(IOBuffer* buf,
               int buf_len,
               IPEndPoint* address,
               CompletionOnceCallback callback);

  // Sends to a socket with a particular destination.
  // |buf| is the buffer to send.
  // |buf_len| is the number of bytes to send.
  // |address| is the recipient address.
  // |callback| is the user callback function to call on complete.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf| and |address|
  // alive until the callback is called.
  int SendTo(IOBuffer* buf,
             int buf_len,
             const IPEndPoint& address,
             CompletionOnceCallback callback);

  // Sets the receive buffer size (in bytes) for the socket.
  // Returns a net error code.
  int SetReceiveBufferSize(int32_t size);

  // Sets the send buffer size (in bytes) for the socket.
  // Returns a net error code.
  int SetSendBufferSize(int32_t size);

  // Requests that packets sent by this socket not be fragment, either locally
  // by the host, or by routers (via the DF bit in the IPv4 packet header).
  // May not be supported by all platforms. Returns a return a network error
  // code if there was a problem, but the socket will still be usable. Can not
  // return ERR_IO_PENDING.
  int SetDoNotFragment();

  // If |confirm| is true, then the MSG_CONFIRM flag will be passed to
  // subsequent writes if it's supported by the platform.
  void SetMsgConfirm(bool confirm);

  // Returns true if the socket is already connected or bound.
  bool is_connected() const { return is_connected_; }

  const NetLogWithSource& NetLog() const { return net_log_; }

  // Call this to enable SO_REUSEADDR on the underlying socket.
  // Should be called between Open() and Bind().
  // Returns a net error code.
  int AllowAddressReuse();

  // Call this to allow or disallow sending and receiving packets to and from
  // broadcast addresses.
  // Returns a net error code.
  int SetBroadcast(bool broadcast);

  // Joins the multicast group.
  // |group_address| is the group address to join, could be either
  // an IPv4 or IPv6 address.
  // Returns a net error code.
  int JoinGroup(const IPAddress& group_address) const;

  // Leaves the multicast group.
  // |group_address| is the group address to leave, could be either
  // an IPv4 or IPv6 address. If the socket hasn't joined the group,
  // it will be ignored.
  // It's optional to leave the multicast group before destroying
  // the socket. It will be done by the OS.
  // Returns a net error code.
  int LeaveGroup(const IPAddress& group_address) const;

  // Sets interface to use for multicast. If |interface_index| set to 0,
  // default interface is used.
  // Should be called before Bind().
  // Returns a net error code.
  int SetMulticastInterface(uint32_t interface_index);

  // Sets the time-to-live option for UDP packets sent to the multicast
  // group address. The default value of this option is 1.
  // Cannot be negative or more than 255.
  // Should be called before Bind().
  // Returns a net error code.
  int SetMulticastTimeToLive(int time_to_live);

  // Sets the loopback flag for UDP socket. If this flag is true, the host
  // will receive packets sent to the joined group from itself.
  // The default value of this option is true.
  // Should be called before Bind().
  // Returns a net error code.
  //
  // Note: the behavior of |SetMulticastLoopbackMode| is slightly
  // different between Windows and Unix-like systems. The inconsistency only
  // happens when there are more than one applications on the same host
  // joined to the same multicast group while having different settings on
  // multicast loopback mode. On Windows, the applications with loopback off
  // will not RECEIVE the loopback packets; while on Unix-like systems, the
  // applications with loopback off will not SEND the loopback packets to
  // other applications on the same host. See MSDN: http://goo.gl/6vqbj
  int SetMulticastLoopbackMode(bool loopback);

  // Sets the differentiated services flags on outgoing packets. May not
  // do anything on some platforms.
  // Returns a net error code.
  int SetDiffServCodePoint(DiffServCodePoint dscp);

  // Resets the thread to be used for thread-safety checks.
  void DetachFromThread();

  // Apply |tag| to this socket.
  void ApplySocketTag(const SocketTag& tag);

  void SetWriteAsyncEnabled(bool enabled) { write_async_enabled_ = enabled; }
  bool WriteAsyncEnabled() { return write_async_enabled_; }

  void SetMaxPacketSize(size_t max_packet_size);

  void SetWriteMultiCoreEnabled(bool enabled) {
    write_multi_core_enabled_ = enabled;
  }

  void SetWriteBatchingActive(bool active) { write_batching_active_ = active; }

  void SetWriteAsyncMaxBuffers(int value) {
    LOG(INFO) << "SetWriteAsyncMaxBuffers: " << value;
    write_async_max_buffers_ = value;
  }

  // Enables experimental optimization. This method should be called
  // before the socket is used to read data for the first time.
  void enable_experimental_recv_optimization() {
    DCHECK(SbSocketIsValid(socket_));
    experimental_recv_optimization_enabled_ = true;
  };

 protected:
  // Watcher for WriteAsync paths.
  class WriteAsyncWatcher : public base::MessageLoopCurrentForIO::Watcher {
   public:
    explicit WriteAsyncWatcher(UDPSocketStarboard* socket)
        : socket_(socket), watching_(false) {}

    // MessageLoopCurrentForIO::Watcher methods

    void OnSocketReadyToRead(SbSocket socket) override{};
    void OnSocketReadyToWrite(SbSocket socket) override;

    void set_watching(bool watching) { watching_ = watching; }

    bool watching() { return watching_; }

   private:
    UDPSocketStarboard* const socket_;
    bool watching_;

    DISALLOW_COPY_AND_ASSIGN(WriteAsyncWatcher);
  };

  void IncreaseWriteAsyncOutstanding(int increment) {
    write_async_outstanding_ += increment;
  }

  virtual bool InternalWatchSocket();
  virtual void InternalStopWatchingSocket();

  void SetWriteCallback(CompletionOnceCallback callback) {
    write_callback_ = std::move(callback);
  }

  void DidSendBuffers(SendResult buffers);
  void FlushPending();

  std::unique_ptr<WriteAsyncWatcher> write_async_watcher_;
  scoped_refptr<UDPSocketStarboardSender> sender_;
  std::unique_ptr<DatagramBufferPool> datagram_buffer_pool_;
  // |WriteAsync| pending writes, does not include buffers that have
  // been |PostTask*|'d.
  DatagramBuffers pending_writes_;

 private:
  // MessageLoopCurrentForIO::Watcher implementation.
  void OnSocketReadyToRead(SbSocket socket) override;
  void OnSocketReadyToWrite(SbSocket socket) override;

  int InternalWriteAsync(CompletionOnceCallback callback,
                         const NetworkTrafficAnnotationTag& traffic_annotation);
  bool WatchSocket();
  void StopWatchingSocket();

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

  // Same as SendTo(), except that address is passed by pointer
  // instead of by reference. It is called from Write() with |address|
  // set to NULL.
  int SendToOrWrite(IOBuffer* buf,
                    int buf_len,
                    const IPEndPoint* address,
                    CompletionOnceCallback callback);

  int InternalConnect(const IPEndPoint& address);
  // Reads data from a UDP socket, if address is not nullptr, the sender's
  // address will be copied to |*address|.
  int InternalRecvFrom(IOBuffer* buf, int buf_len, IPEndPoint* address);
  int InternalSendTo(IOBuffer* buf, int buf_len, const IPEndPoint* address);

  // Applies |socket_options_| to |socket_|. Should be called before
  // Bind().
  int DoBind(const IPEndPoint& address);
  int RandomBind(const IPAddress& address);

  // Helpers for |WriteAsync|
  base::SequencedTaskRunner* GetTaskRunner();
  void OnWriteAsyncTimerFired();
  void LocalSendBuffers();
  void PostSendBuffers();
  int ResetLastAsyncResult();
  int ResetWrittenBytes();

  SbSocket socket_;
  bool is_connected_ = false;

  SbSocketAddressType address_type_;

  // Bitwise-or'd combination of SocketOptions. Specifies the set of
  // options that should be applied to |socket_| before Bind().
  int socket_options_;

  // How to do source port binding, used only when UDPSocket is part of
  // UDPClientSocket, since UDPServerSocket provides Bind.
  DatagramSocket::BindType bind_type_;

  // These are mutable since they're just cached copies to make
  // GetPeerAddress/GetLocalAddress smarter.
  mutable std::unique_ptr<IPEndPoint> local_address_;
  mutable std::unique_ptr<IPEndPoint> remote_address_;

  // The socket's SbSocketWaiter wrappers
  base::MessageLoopCurrentForIO::SocketWatcher socket_watcher_;

  // Various bits to support |WriteAsync()|.
  bool write_async_enabled_ = false;
  bool write_batching_active_ = false;
  bool write_multi_core_enabled_ = false;
  int write_async_max_buffers_ = 16;
  int written_bytes_ = 0;

  int last_async_result_;
  base::RepeatingTimer write_async_timer_;
  bool write_async_timer_running_;
  // Total writes in flight, including those |PostTask*|'d.
  int write_async_outstanding_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The buffer used by InternalRead() to retry Read requests
  IOBuffer* read_buf_;
  int read_buf_len_;
  IPEndPoint* recv_from_address_;

  // The buffer used by InternalWrite() to retry Write requests
  IOBuffer* write_buf_;
  int write_buf_len_;
  std::unique_ptr<IPEndPoint> send_to_address_;

  // External callback; called when read is complete.
  CompletionOnceCallback read_callback_;

  // External callback; called when write is complete.
  CompletionOnceCallback write_callback_;

  NetLogWithSource net_log_;

  // If set to true, the socket will use an optimized experimental code path.
  // By default, the value is set to false. To use the optimization, the
  // client of the socket has to opt-in by calling the
  // enable_experimental_recv_optimization() method.
  bool experimental_recv_optimization_enabled_ = false;

  THREAD_CHECKER(thread_checker_);

  // Used for alternate writes that are posted for concurrent execution.
  base::WeakPtrFactory<UDPSocketStarboard> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketStarboard);
};

}  // namespace net

#endif  // NET_SOCKET_UDP_SOCKET_STARBOARD_H_
