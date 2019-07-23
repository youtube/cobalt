// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef NET_SOCKET_TCP_SOCKET_STARBOARD_H_
#define NET_SOCKET_TCP_SOCKET_STARBOARD_H_

#include "base/message_loop/message_loop.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#include "starboard/common/socket.h"

namespace net {

class NET_EXPORT TCPSocketStarboard
    : public base::MessageLoopCurrentForIO::Watcher {
 public:
  TCPSocketStarboard(
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetLog* net_log, const NetLogSource& source);
  ~TCPSocketStarboard() override;

  int Open(AddressFamily family);

  // Takes ownership of |socket|, which is known to already be connected to the
  // given peer address. However, peer address may be the empty address, for
  // compatibility. The given peer address will be returned by GetPeerAddress.
  int AdoptConnectedSocket(SocketDescriptor socket,
                           const IPEndPoint& peer_address);
  // Takes ownership of |socket|, which may or may not be open, bound, or
  // listening. The caller must determine the state of the socket based on its
  // provenance and act accordingly. The socket may have connections waiting
  // to be accepted, but must not be actually connected.
  int AdoptUnconnectedSocket(SocketDescriptor socket);

  int Bind(const IPEndPoint& address);

  int Listen(int backlog);
  // Accepts incoming connection.
  // Returns a net error code.
  int Accept(std::unique_ptr<TCPSocketStarboard>* socket,
             IPEndPoint* address,
             CompletionOnceCallback callback);

  int Connect(const IPEndPoint& address, CompletionOnceCallback callback);
  bool IsConnected() const;
  bool IsConnectedAndIdle() const;

  int Read(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);
  int ReadIfReady(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);
  int CancelReadIfReady();
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag&);

  int GetLocalAddress(IPEndPoint* address) const;
  int GetPeerAddress(IPEndPoint* address) const;

  int SetDefaultOptionsForServer();
  void SetDefaultOptionsForClient();
  int AllowAddressReuse();
  bool SetReceiveBufferSize(int32_t size);
  bool SetSendBufferSize(int32_t size);
  bool SetKeepAlive(bool enable, int delay);
  bool SetNoDelay(bool no_delay);

  // Gets the estimated RTT. Returns false if the RTT is
  // unavailable. May also return false when estimated RTT is 0.
  bool GetEstimatedRoundTripTime(base::TimeDelta* out_rtt) const
      WARN_UNUSED_RESULT;

  void Close();

  // NOOP since TCP FastOpen is not implemented in Windows.
  void EnableTCPFastOpenIfSupported() {}

  bool IsValid() const { return SbSocketIsValid(socket_); }

  // Detachs from the current thread, to allow the socket to be transferred to
  // a new thread. Should only be called when the object is no longer used by
  // the old thread.
  void DetachFromThread();

  // Marks the start/end of a series of connect attempts for logging purpose.
  //
  // TCPClientSocket may attempt to connect to multiple addresses until it
  // succeeds in establishing a connection. The corresponding log will have
  // multiple NetLogEventType::TCP_CONNECT_ATTEMPT entries nested within a
  // NetLogEventType::TCP_CONNECT. These methods set the start/end of
  // NetLogEventType::TCP_CONNECT.
  //
  // TODO(yzshen): Change logging format and let TCPClientSocket log the
  // start/end of a series of connect attempts itself.
  void StartLoggingMultipleConnectAttempts(const AddressList& addresses);
  void EndLoggingMultipleConnectAttempts(int net_error);

  const NetLogWithSource& net_log() const { return net_log_; }

  // Return the underlying SocketDescriptor and clean up this object, which may
  // no longer be used. This method should be used only for testing. No read,
  // write, or accept operations should be pending.
  SocketDescriptor ReleaseSocketDescriptorForTesting();

  // Exposes the underlying socket descriptor for testing its state. Does not
  // release ownership of the descriptor.
  SocketDescriptor SocketDescriptorForTesting() const;

  // Apply |tag| to this socket.
  void ApplySocketTag(const SocketTag& tag);

  // May return nullptr.
  SocketPerformanceWatcher* socket_performance_watcher() const {
    return socket_performance_watcher_.get();
  }

  // MessageLoopCurrentForIO::Watcher implementation.
  void OnSocketReadyToRead(SbSocket socket) override;
  void OnSocketReadyToWrite(SbSocket socket) override;

 private:
  void RetryRead(int rv);
  bool waiting_connect() const { return waiting_connect_; }
  void DidCompleteConnect();
  void DidCompleteWrite();
  int AcceptInternal(std::unique_ptr<TCPSocketStarboard>* socket,
                     IPEndPoint* address);

  int HandleConnectCompleted(int rv);
  int DoRead(IOBuffer* buf, int buf_len);
  void DidCompleteRead();
  void LogConnectBegin(const AddressList& addresses) const;
  void LogConnectEnd(int net_error);

  int DoWrite(IOBuffer* buf, int buf_len);

  void StopWatchingAndCleanUp();
  void ClearWatcherIfOperationsNotPending();

  bool read_pending() const { return !read_if_ready_callback_.is_null(); }
  bool write_pending() const {
    return !write_callback_.is_null() && !waiting_connect_;
  }
  bool accept_pending() const { return !accept_callback_.is_null(); }
  bool connect_pending() const { return waiting_connect_; }

  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher_;

  SbSocket socket_;

  base::MessageLoopCurrentForIO::SocketWatcher socket_watcher_;

  std::unique_ptr<TCPSocketStarboard>* accept_socket_;
  IPEndPoint* accept_address_;
  CompletionOnceCallback accept_callback_;

  std::unique_ptr<IPEndPoint> peer_address_;
  std::unique_ptr<IPEndPoint> local_address_;

  AddressFamily family_;

  bool logging_multiple_connect_attempts_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;
  // Where we are in above list. Set to -1 if uninitialized.
  int current_address_index_;

  NetLogWithSource net_log_;

  bool listening_;
  bool waiting_connect_;
  bool waiting_write_;

  IOBuffer* read_buf_;
  int read_buf_len_;
  CompletionOnceCallback read_callback_;
  CompletionOnceCallback read_if_ready_callback_;

  IOBuffer* write_buf_;
  int write_buf_len_;
  CompletionOnceCallback write_callback_;

  THREAD_CHECKER(thread_checker_);

  // Current socket tag if |socket_| is valid, otherwise the tag to apply when
  // |socket_| is opened.
  SocketTag tag_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocketStarboard);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SOCKET_STARBOARD_H_
