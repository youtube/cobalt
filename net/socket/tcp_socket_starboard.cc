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

#include "net/socket/tcp_socket_starboard.h"

#include <memory>
#include <utility>

#if SB_API_VERSION >= 16
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "base/functional/callback_helpers.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/task/current_thread.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/socket/socket_net_log_params.h"
#include "net/socket/socket_options.h"
#include "starboard/configuration_constants.h"

namespace net {

TCPSocketStarboard::TCPSocketStarboard(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLog* net_log,
    const NetLogSource& source)
    : socket_performance_watcher_(std::move(socket_performance_watcher)),
#if SB_API_VERSION >= 16
      socket_(-1),
#else
      socket_(kSbSocketInvalid),
#endif
      socket_watcher_(FROM_HERE),
      family_(ADDRESS_FAMILY_UNSPECIFIED),
      logging_multiple_connect_attempts_(false),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)),
      listening_(false),
      waiting_connect_(false) {
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE, source);
}

TCPSocketStarboard::~TCPSocketStarboard() {
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
  Close();
}

int TCPSocketStarboard::Open(AddressFamily family) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16

  DCHECK(socket_ < 0);

  int socket_family;
  switch(family) {
    case ADDRESS_FAMILY_IPV4:
      socket_family = AF_INET;
      break;
    case ADDRESS_FAMILY_IPV6:
      socket_family = AF_INET6;
      break;
    case ADDRESS_FAMILY_UNSPECIFIED:
    default:
      socket_family = AF_UNSPEC;
      break;
  }

  socket_ = socket(socket_family, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ < 0) {
    return MapSystemError(errno);
  }

  // All Starboard sockets are non-blocking, so let's ensure it.
  int flags = fcntl(socket_, F_GETFL, 0);
  if(fcntl(socket_, F_SETFL, flags | O_NONBLOCK) < 0){
    close(socket_);
    return MapSystemError(errno);
  }
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
  // Use SO_NOSIGPIPE to mute SIGPIPE on darwin systems.
  int optval_set = 1;
  setsockopt(socket_, SOL_SOCKET, SO_NOSIGPIPE,
             reinterpret_cast<void*>(&optval_set), sizeof(int));
#endif

#else
  DCHECK(!SbSocketIsValid(socket_));
  socket_ = SbSocketCreate(ConvertAddressFamily(family), kSbSocketProtocolTcp);
  if (!SbSocketIsValid(socket_)) {
    return MapLastSystemError();
  }
#endif

  family_ = family;
  return OK;
}

int TCPSocketStarboard::AdoptConnectedSocket(SocketDescriptor socket,
                                             const IPEndPoint& peer_address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16
  DCHECK(socket_ < 0);
#else
  DCHECK(!SbSocketIsValid(socket_));
#endif

  SbSocketAddress storage;
  if (!peer_address.ToSbSocketAddress(&storage) &&
      !(peer_address == IPEndPoint())) {
    return ERR_ADDRESS_INVALID;
  }

  socket_ = socket;
  peer_address_.reset(new IPEndPoint(peer_address));
  return OK;
}

int TCPSocketStarboard::AdoptUnconnectedSocket(SocketDescriptor socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16
  DCHECK(socket_ < 0);
#else
  DCHECK(!SbSocketIsValid(socket_));
#endif

  socket_ = socket;
  return OK;
}

int TCPSocketStarboard::Bind(const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  printf(" -- [Bind]\n");

#if SB_API_VERSION >= 16
  DCHECK(socket_ >= 0);

  // Must have already called Open() on this socket with the same address
  // family as is being passed in here.
  DCHECK_EQ(family_, address.GetFamily());

  struct sockaddr_storage saddr_store;
  socklen_t address_length = sizeof(saddr_store);
  if (!address.ToSockAddr(reinterpret_cast<struct sockaddr*>(&saddr_store), &address_length)) {
    return ERR_ADDRESS_INVALID;
  }

  int error = bind(socket_, reinterpret_cast<struct sockaddr*>(&saddr_store), address_length);

  if (error != 0) {
    DLOG(ERROR) << "Socket bind() returned an error";
    return MapSystemError(errno);
  }
#else
  DCHECK(SbSocketIsValid(socket_));

  // Must have already called Open() on this socket with the same address
  // family as is being passed in here.
  DCHECK_EQ(family_, address.GetFamily());

  SbSocketAddress storage;
  if (!address.ToSbSocketAddress(&storage)) {
    return ERR_ADDRESS_INVALID;
  }

  SbSocketError error = SbSocketBind(socket_, &storage);
  if (error != kSbSocketOk) {
    DLOG(ERROR) << "SbSocketBind() returned an error";
    return MapLastSocketError(socket_);
  }
#endif  // SB_API_VERSION >= 16

  local_address_.reset(new IPEndPoint(address));

  return OK;
}

int TCPSocketStarboard::Listen(int backlog) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(backlog, 0);
#if SB_API_VERSION >= 16
  DCHECK(socket_ >= 0);
  DCHECK(local_address_) << "Must have previously called Bind().";

  printf(" -- [Listen]\n");

  int error = listen(socket_, backlog);
  if (error != 0) {
    DLOG(ERROR) << "Socket listen() returned an error";
    int rv = MapSystemError(errno);
    Close();
    return rv;
  }
#else
  DCHECK(SbSocketIsValid(socket_));
  DCHECK(local_address_) << "Must have previously called Bind().";
  SbSocketError error = SbSocketListen(socket_);
  if (error != kSbSocketOk) {
    DLOG(ERROR) << "SbSocketListen() returned an error";
    int rv = MapLastSocketError(socket_);
    Close();
    return rv;
  }
#endif

  listening_ = true;

  return OK;
}

int TCPSocketStarboard::Accept(std::unique_ptr<TCPSocketStarboard>* socket,
                               IPEndPoint* address,
                               CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(socket);
  DCHECK(!callback.is_null());
  DCHECK(accept_callback_.is_null());

  printf(" -- [Accept]\n");

  net_log_.BeginEvent(NetLogEventType::TCP_ACCEPT);

  int result = AcceptInternal(socket, address);

  if (result == ERR_IO_PENDING) {
    if (!base::CurrentIOThread::Get()->Watch(
            socket_, true, base::MessagePumpIOStarboard::WATCH_READ,
            &socket_watcher_, this)) {
      DLOG(ERROR) << "WatchSocket failed on read";
#if SB_API_VERSION >= 16
      return MapSystemError(errno);
#else
      return MapLastSocketError(socket_);
#endif
    }

    accept_socket_ = socket;
    accept_address_ = address;
    accept_callback_ = std::move(callback);
  }

  return result;
}

int TCPSocketStarboard::SetDefaultOptionsForServer() {
#if SB_API_VERSION >= 16
  DCHECK(socket_ >= 0);
  const int on = 1;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
    return MapSystemError(errno);
  }
#else
  DCHECK(SbSocketIsValid(socket_));
  if (!SbSocketSetReuseAddress(socket_, true)) {
    return MapLastSocketError(socket_);
  }
#endif

  return OK;
}

void TCPSocketStarboard::SetDefaultOptionsForClient() {
#if SB_API_VERSION >= 16
  bool on = true;
  // SbSocketSetTcpNoDelay
  setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                      reinterpret_cast<const char*>(&on), sizeof(on));
#else
  SbSocketSetTcpNoDelay(socket_, true);
#endif

  const int64_t kTCPKeepAliveDurationUsec = 45 * base::Time::kMicrosecondsPerSecond;

#if SB_API_VERSION >= 16
  // SbSocketSetTcpKeepAlive
  setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
#if defined(__APPLE__)
    // In tvOS, TCP_KEEPIDLE and SOL_TCP are not available.
    // For reference:
    // https://stackoverflow.com/questions/15860127/how-to-configure-tcp-keepalive-under-mac-os-x
    result |= setsockopt(socket_, IPPROTO_TCP, TCP_KEEPALIVE, &kTCPKeepAliveDurationUsec,
                       sizeof(kTCPKeepAliveDurationUsec));
#elif !defined(_WIN32)
    // In Windows, the SOL_TCP and TCP_KEEPIDLE options are not available.
    // For reference:
    // https://stackoverflow.com/questions/8176821/how-to-set-the-keep-alive-interval-for-winsock
  setsockopt(socket_, SOL_TCP, TCP_KEEPIDLE, &kTCPKeepAliveDurationUsec,
                       sizeof(kTCPKeepAliveDurationUsec));
  setsockopt(socket_, SOL_TCP, TCP_KEEPINTVL, &kTCPKeepAliveDurationUsec,
                       sizeof(kTCPKeepAliveDurationUsec));
#endif
  // SbSocketSetTcpWindowScaling - This API is not supported in standard POSIX.

  // SbSocketSetReceiveBufferSize
  if (kSbNetworkReceiveBufferSize != 0) {
    setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, "SO_RCVBUF", kSbNetworkReceiveBufferSize);
  }
#else
  SbSocketSetTcpKeepAlive(socket_, true, kTCPKeepAliveDurationUsec);
  SbSocketSetTcpWindowScaling(socket_, true);
  if (kSbNetworkReceiveBufferSize != 0) {
    SbSocketSetReceiveBufferSize(socket_, kSbNetworkReceiveBufferSize);
  }
#endif  // SB_API_VERSION >= 16

}

#if SB_API_VERSION >= 16
int TCPSocketStarboard::AcceptInternal(
    std::unique_ptr<TCPSocketStarboard>* socket,
    IPEndPoint* address) {
  int new_socket = accept(socket_, nullptr, nullptr);
  if (new_socket < 0) {
    int net_error = MapSystemError(errno);
    if (net_error != ERR_IO_PENDING) {
      net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT, net_error);
    }
    return net_error;
  }

  char unused_byte;
  struct sockaddr_storage saddr_store;
  socklen_t length = sizeof(saddr_store);
  int result = getpeername(new_socket, reinterpret_cast<struct sockaddr*>(&saddr_store), &length);

  if (result != 0) {
    int net_error = MapSystemError(errno);
    if (net_error != OK && net_error != ERR_IO_PENDING) {
      close(new_socket);
      net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT, net_error);
      return net_error;
    } else {
      // SbSocketReceiveFrom could return -1 and setting net_error to OK on
      // some platforms, it is non-fatal. And we are not returning
      // ERR_IO_PENDING to try again because 1. Some tests hang and time out
      // waiting for Accept() to succeed and 2. Peer address is unused in
      // most use cases. Chromium implementations get the address from accept()
      // directly, but Starboard API is incapable of that.
      DVLOG(1) << "Could not get peer address for the server socket.";
    }
  }

  IPEndPoint ip_end_point;
  if (!ip_end_point.FromSockAddr(reinterpret_cast<struct sockaddr*>(&saddr_store), length)) {
    close(new_socket);
    int net_error = ERR_ADDRESS_INVALID;
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT, net_error);
    return net_error;
  }

  std::unique_ptr<TCPSocketStarboard> tcp_socket(
      new TCPSocketStarboard(nullptr, net_log_.net_log(), net_log_.source()));
  int adopt_result = tcp_socket->AdoptConnectedSocket(new_socket, ip_end_point);
  if (adopt_result != OK) {
    if (!close(new_socket)) {
      DPLOG(ERROR) << "Socket close()";
    }
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT,
                                      adopt_result);
    return adopt_result;
  }
  *socket = std::move(tcp_socket);
  *address = ip_end_point;
  net_log_.EndEvent(NetLogEventType::TCP_ACCEPT,
                    [&] { return CreateNetLogIPEndPointParams(&ip_end_point); });
  return OK;
}

#else
int TCPSocketStarboard::AcceptInternal(
    std::unique_ptr<TCPSocketStarboard>* socket,
    IPEndPoint* address) {
  SbSocket new_socket = SbSocketAccept(socket_);
  if (!SbSocketIsValid(new_socket)) {
    int net_error = MapLastSocketError(socket_);
    if (net_error != ERR_IO_PENDING) {
      net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT, net_error);
    }
    return net_error;
  }

  SbSocketAddress sb_address;
  char unused_byte;
  // We use ReceiveFrom to get peer address of the newly connected socket.
  int received = SbSocketReceiveFrom(new_socket, &unused_byte, 0, &sb_address);
  if (received != 0) {
    int net_error = MapLastSocketError(new_socket);
    if (net_error != OK && net_error != ERR_IO_PENDING) {
      SbSocketDestroy(new_socket);
      net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT, net_error);
      return net_error;
    } else {
      // SbSocketReceiveFrom could return -1 and setting net_error to OK on
      // some platforms, it is non-fatal. And we are not returning
      // ERR_IO_PENDING to try again because 1. Some tests hang and time out
      // waiting for Accept() to succeed and 2. Peer address is unused in
      // most use cases. Chromium implementations get the address from accept()
      // directly, but Starboard API is incapable of that.
      DVLOG(1) << "Could not get peer address for the server socket.";
    }
  }

  IPEndPoint ip_end_point;
  if (!ip_end_point.FromSbSocketAddress(&sb_address)) {
    SbSocketDestroy(new_socket);
    int net_error = ERR_ADDRESS_INVALID;
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT, net_error);
    return net_error;
  }

  std::unique_ptr<TCPSocketStarboard> tcp_socket(
      new TCPSocketStarboard(nullptr, net_log_.net_log(), net_log_.source()));
  int adopt_result = tcp_socket->AdoptConnectedSocket(new_socket, ip_end_point);
  if (adopt_result != OK) {
    if (!SbSocketDestroy(new_socket)) {
      DPLOG(ERROR) << "SbSocketDestroy";
    }
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT,
                                      adopt_result);
    return adopt_result;
  }

  *socket = std::move(tcp_socket);
  *address = ip_end_point;
  net_log_.EndEvent(NetLogEventType::TCP_ACCEPT,
                    [&] { return CreateNetLogIPEndPointParams(&ip_end_point); });
  return OK;
}
#endif  // SB_API_VERSION >= 16

void TCPSocketStarboard::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  printf(" -- [Close]\n");

#if SB_API_VERSION >= 16
  if (socket_ >= 0) {
    StopWatchingAndCleanUp();
    net_log_.AddEvent(NetLogEventType::SOCKET_CLOSED);

    if (!close(socket_)) {
      DPLOG(ERROR) << "Socket close()";
    }
    socket_ = -1;
  }
#else
  if (SbSocketIsValid(socket_)) {
    StopWatchingAndCleanUp();

    net_log_.AddEvent(NetLogEventType::SOCKET_CLOSED);

    if (!SbSocketDestroy(socket_)) {
      DPLOG(ERROR) << "SbSocketDestroy";
    }
    socket_ = kSbSocketInvalid;
  }
#endif

  listening_ = false;
}

void TCPSocketStarboard::StopWatchingAndCleanUp() {
  bool ok = socket_watcher_.StopWatchingSocket();
  DCHECK(ok);

  if (!accept_callback_.is_null()) {
    accept_socket_ = nullptr;
    accept_callback_.Reset();
  }

  if (!read_callback_.is_null()) {
    read_buf_ = nullptr;
    read_buf_len_ = 0;
    read_if_ready_callback_.Reset();
    read_callback_.Reset();
  }

  read_if_ready_callback_.Reset();

  if (!write_callback_.is_null()) {
    write_buf_ = nullptr;
    write_buf_len_ = 0;
    write_callback_.Reset();
  }

  waiting_connect_ = false;
  peer_address_.reset();
}

#if SB_API_VERSION >= 16
void TCPSocketStarboard::OnSocketReadyToRead(int socket) {
#else
void TCPSocketStarboard::OnSocketReadyToRead(SbSocket socket) {
#endif
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (accept_pending()) {
    int result = AcceptInternal(accept_socket_, accept_address_);
    if (result != ERR_IO_PENDING) {
      accept_socket_ = nullptr;
      CompletionOnceCallback callback = std::move(accept_callback_);
      accept_callback_.Reset();
      std::move(callback).Run(result);
    }
  } else if (read_pending()) {
    DidCompleteRead();
  }
}

#if SB_API_VERSION >= 16
void TCPSocketStarboard::OnSocketReadyToWrite(int socket) {
#else
void TCPSocketStarboard::OnSocketReadyToWrite(SbSocket socket) {
#endif
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (listening_) {
    NOTREACHED();
  } else {
    // is this a connection callback?
    if (connect_pending()) {
      DidCompleteConnect();
    } else if (write_pending()) {
      DidCompleteWrite();
    }
  }
}

int TCPSocketStarboard::Connect(const IPEndPoint& address,
                                CompletionOnceCallback callback) {

  printf(" -- [Connect]\n");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16
  DCHECK(socket_ >= 0);
#else
  DCHECK(SbSocketIsValid(socket_));
#endif
  DCHECK(!peer_address_);
  DCHECK(!connect_pending());

  if (!logging_multiple_connect_attempts_) {
    LogConnectBegin(AddressList(address));
  }

  net_log_.BeginEvent(NetLogEventType::TCP_CONNECT_ATTEMPT,
                      [&] { return CreateNetLogIPEndPointParams(&address); });

#if SB_API_VERSION >= 16
  struct sockaddr_storage saddr_store;
  socklen_t length = sizeof(saddr_store);
  if (!address.ToSockAddr(reinterpret_cast<struct sockaddr*>(&saddr_store), &length)) {
    return ERR_ADDRESS_INVALID;
  }

  peer_address_.reset(new IPEndPoint(address));
  int result = connect(socket_, reinterpret_cast<struct sockaddr*>(&saddr_store), length);
  int rv = 0;
  if (result != 0){
    rv = MapSystemError(errno);
  }
#else
  SbSocketAddress storage;
  if (!address.ToSbSocketAddress(&storage)) {
    return ERR_ADDRESS_INVALID;
  }

  peer_address_.reset(new IPEndPoint(address));

  SbSocketError result = SbSocketConnect(socket_, &storage);

  int rv = MapLastSocketError(socket_);
#endif

  if (rv != ERR_IO_PENDING) {
    return HandleConnectCompleted(rv);
  }

  waiting_connect_ = true;
  write_callback_ = std::move(callback);

  // When it is ready to write, it will have connected.
  base::CurrentIOThread::Get()->Watch(
      socket_, true, base::MessagePumpIOStarboard::WATCH_WRITE,
      &socket_watcher_, this);

  return ERR_IO_PENDING;
}

int TCPSocketStarboard::HandleConnectCompleted(int rv) {
  // Log the end of this attempt (and any OS error it threw).
  if (rv != OK) {
    net_log_.EndEventWithIntParams(NetLogEventType::TCP_CONNECT_ATTEMPT,
                                   "os_error", errno);
  } else {
    net_log_.EndEvent(NetLogEventType::TCP_CONNECT_ATTEMPT);
  }

  // Give a more specific error when the user is offline.
  if (!logging_multiple_connect_attempts_)
    LogConnectEnd(rv);

  return rv;
}

void TCPSocketStarboard::DidCompleteConnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16
  int rv = false;

  if (socket_ < 0) {
    rv = false;
  } else {
    char c;
    int result = recv(socket_, &c, 1, MSG_PEEK);
    if (result == 0) {
      rv = false;
    } else {
      rv = result > 0 || MapSystemError(errno) == ERR_IO_PENDING;
    }
  }
#else
  int rv = SbSocketIsConnected(socket_) ? OK : ERR_FAILED;
#endif

  waiting_connect_ = false;
  CompletionOnceCallback callback = std::move(write_callback_);
  write_callback_.Reset();
  ClearWatcherIfOperationsNotPending();
  std::move(callback).Run(HandleConnectCompleted(rv));
}

void TCPSocketStarboard::StartLoggingMultipleConnectAttempts(
    const AddressList& addresses) {
  if (!logging_multiple_connect_attempts_) {
    logging_multiple_connect_attempts_ = true;
    LogConnectBegin(addresses);
  } else {
    NOTREACHED();
  }
}

bool TCPSocketStarboard::IsConnected() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16
  if (socket_ < 0) {
    return false;
  }
  char c;
  int result = recv(socket_, &c, 1, MSG_PEEK);
  if (result == 0) {
    return false;
  }
  return (result > 0 || MapSystemError(errno) == ERR_IO_PENDING);
#else
  return SbSocketIsConnected(socket_);
#endif
}

bool TCPSocketStarboard::IsConnectedAndIdle() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16
  if (socket_ < 0)
    return false;  // not connected
  char c;
  if (recv(socket_, &c, 1, MSG_PEEK) >= 0)
    return false;  // not idle
  return MapSystemError(errno) == ERR_IO_PENDING;
#else
  return SbSocketIsConnectedAndIdle(socket_);
#endif
}

void TCPSocketStarboard::EndLoggingMultipleConnectAttempts(int net_error) {
  if (logging_multiple_connect_attempts_) {
    LogConnectEnd(net_error);
    logging_multiple_connect_attempts_ = false;
  } else {
    NOTREACHED();
  }
}

void TCPSocketStarboard::LogConnectBegin(const AddressList& addresses) const {
  net_log_.BeginEvent(NetLogEventType::TCP_CONNECT,
                      [&] { return addresses.NetLogParams(); });
}

void TCPSocketStarboard::LogConnectEnd(int net_error) {
  if (net_error != OK) {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_CONNECT, net_error);
    return;
  }

  net_log_.EndEvent(NetLogEventType::TCP_CONNECT);
}

int TCPSocketStarboard::Read(IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16
  DCHECK(socket_ >= 0);
#else
  DCHECK(SbSocketIsValid(socket_));
#endif
  DCHECK(!connect_pending());
  DCHECK_GT(buf_len, 0);

  int rv = ReadIfReady(
      buf, buf_len,
      base::Bind(&TCPSocketStarboard::RetryRead, base::Unretained(this)));

  if (rv != ERR_IO_PENDING) {
    return rv;
  }

  // We'll callback when there is more data.
  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = std::move(callback);
  return rv;
}

int TCPSocketStarboard::ReadIfReady(IOBuffer* buf,
                                    int buf_len,
                                    CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16
  DCHECK(socket_ >= 0);
#else
  DCHECK(SbSocketIsValid(socket_));
#endif
  DCHECK(read_if_ready_callback_.is_null());

  int rv = DoRead(buf, buf_len);

  if (rv != ERR_IO_PENDING) {
    return rv;
  }

  read_if_ready_callback_ = std::move(callback);
    base::CurrentIOThread::Get()->Watch(
      socket_, true, base::MessagePumpIOStarboard::WATCH_READ,
      &socket_watcher_, this);

  return rv;
}

int TCPSocketStarboard::CancelReadIfReady() {
  DCHECK(read_if_ready_callback_);

  bool ok = socket_watcher_.StopWatchingSocket();
  DCHECK(ok);

  read_if_ready_callback_.Reset();
  return net::OK;
}

void TCPSocketStarboard::RetryRead(int rv) {
  DCHECK(read_callback_);
  DCHECK(read_buf_);
  DCHECK_LT(0, read_buf_len_);

  if (rv == OK) {
    rv = ReadIfReady(
        read_buf_, read_buf_len_,
        base::Bind(&TCPSocketStarboard::RetryRead, base::Unretained(this)));
    if (rv == ERR_IO_PENDING)
      return;
  }
  read_buf_ = nullptr;
  read_buf_len_ = 0;
  std::move(read_callback_).Run(rv);
}

int TCPSocketStarboard::DoRead(IOBuffer* buf, int buf_len) {
#if SB_API_VERSION >= 16
  const int kRecvFlag = 0;
  int bytes_read = recv(socket_, buf->data(), buf_len, kRecvFlag);
#else
  int bytes_read = SbSocketReceiveFrom(socket_, buf->data(), buf_len, NULL);
#endif

  if (bytes_read >= 0) {
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED,
                                  bytes_read, buf->data());
    activity_monitor::IncrementBytesReceived(bytes_read);

    return bytes_read;
  } else {
    // If |bytes_read| < 0, some kind of error occurred.
#if SB_API_VERSION >= 16
    int rv = MapSystemError(errno);
    if (rv != 0) {
      DLOG(ERROR) << __FUNCTION__ << "[" << this << "]: Error: " << rv;
    }
#else
    SbSocketError starboard_error = SbSocketGetLastError(socket_);
    int rv = MapSocketError(starboard_error);
    if (rv != ERR_IO_PENDING) {
      NetLogSocketError(net_log_, NetLogEventType::SOCKET_READ_ERROR, rv,
                        starboard_error);
      DLOG(ERROR) << __FUNCTION__ << "[" << this << "]: Error: " << rv;
    }
#endif

    return rv;
  }
}

void TCPSocketStarboard::DidCompleteRead() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CompletionOnceCallback callback = std::move(read_if_ready_callback_);
  read_if_ready_callback_.Reset();

  ClearWatcherIfOperationsNotPending();
  std::move(callback).Run(OK);
}

int TCPSocketStarboard::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16
  DCHECK(socket_ >= 0);
#else
  DCHECK(SbSocketIsValid(socket_));
#endif
  DCHECK(!connect_pending());
  DCHECK(!write_pending());
  DCHECK_GT(buf_len, 0);

  int rv = DoWrite(buf, buf_len);

  if (rv == ERR_IO_PENDING) {
    // We are just blocked, so let's set up the callback.
    write_buf_ = buf;
    write_buf_len_ = buf_len;
    write_callback_ = std::move(callback);
    base::CurrentIOThread::Get()->Watch(
        socket_, true, base::MessagePumpIOStarboard::WATCH_WRITE,
        &socket_watcher_, this);
  }

  return rv;
}

int TCPSocketStarboard::DoWrite(IOBuffer* buf, int buf_len) {
#if SB_API_VERSION >= 16
#if defined(MSG_NOSIGNAL)
  const int kSendFlags = MSG_NOSIGNAL;
#else
  const int kSendFlags = 0;
#endif
  int bytes_sent = send(socket_, buf->data(), buf_len, kSendFlags);
#else
  int bytes_sent = SbSocketSendTo(socket_, buf->data(), buf_len, NULL);
#endif

  if (bytes_sent >= 0) {
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_SENT,
                                  bytes_sent, buf->data());

    return bytes_sent;
  } else {
#if SB_API_VERSION >= 16
    int rv = MapSystemError(errno);
#else
    SbSocketError starboard_error = SbSocketGetLastError(socket_);
    int rv = MapSocketError(starboard_error);
    NetLogSocketError(net_log_, NetLogEventType::SOCKET_WRITE_ERROR, rv,
                      starboard_error);
#endif
    if (rv != ERR_IO_PENDING) {
      DLOG(ERROR) << __FUNCTION__ << "[" << this << "]: Error: " << rv;
    }

    return rv;
  }
}

void TCPSocketStarboard::DidCompleteWrite() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(write_pending());

  int rv = DoWrite(write_buf_, write_buf_len_);

  if (rv != ERR_IO_PENDING) {
    write_buf_ = nullptr;
    write_buf_len_ = 0;

    CompletionOnceCallback callback = std::move(write_callback_);
    write_callback_.Reset();

    ClearWatcherIfOperationsNotPending();
    std::move(callback).Run(rv);
  }
}

bool TCPSocketStarboard::SetReceiveBufferSize(int32_t size) {
  return SetSocketReceiveBufferSize(socket_, size);
}

bool TCPSocketStarboard::SetSendBufferSize(int32_t size) {
  return SetSocketSendBufferSize(socket_, size);
}

bool TCPSocketStarboard::SetKeepAlive(bool enable, int delay) {
  int delay_second = delay * base::Time::kMicrosecondsPerSecond;

#if SB_API_VERSION >= 16
  int on = enable? 1:0;
  int result = 0;
  result |= setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

#if defined(__APPLE__)
    // In tvOS, TCP_KEEPIDLE and SOL_TCP are not available.
    // For reference:
    // https://stackoverflow.com/questions/15860127/how-to-configure-tcp-keepalive-under-mac-os-x
    result |= setsockopt(socket_, IPPROTO_TCP, TCP_KEEPALIVE, &delay_second,
                       sizeof(delay_second));
#elif !defined(_WIN32)
    // In Windows, the SOL_TCP and TCP_KEEPIDLE options are not available.
    // For reference:
    // https://stackoverflow.com/questions/8176821/how-to-set-the-keep-alive-interval-for-winsock
  result |= setsockopt(socket_, SOL_TCP, TCP_KEEPIDLE, &delay_second,
                       sizeof(delay_second));

  result |= setsockopt(socket_, SOL_TCP, TCP_KEEPINTVL, &delay_second,
                       sizeof(delay_second));
#endif


  return result == 0;
#else
  return SbSocketSetTcpKeepAlive(socket_, enable, delay_second);
#endif
}

bool TCPSocketStarboard::SetNoDelay(bool no_delay) {
  if (!socket_)
    return false;
  return SetTCPNoDelay(socket_, no_delay) == OK;
}

bool TCPSocketStarboard::GetEstimatedRoundTripTime(
    base::TimeDelta* out_rtt) const {
  DCHECK(out_rtt);
  // Intentionally commented to suppress tons of warnings.
  // net calls this all the time.
  // NOTIMPLEMENTED();
  return false;
}

int TCPSocketStarboard::GetLocalAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);
  printf(" -- [GetLocalAddress]\n");

#if SB_API_VERSION >= 16
  DCHECK(socket_ >= 0);
  struct sockaddr_storage saddr_store = {0};
  socklen_t socklen = sizeof(saddr_store);

  int result = getsockname(socket_, reinterpret_cast<struct sockaddr*>(&saddr_store), &socklen);
  if (0 != result){
    return MapSystemError(errno);
  }
  if (!address->FromSockAddr(reinterpret_cast<struct sockaddr*>(&saddr_store), socklen)) {
    return ERR_ADDRESS_INVALID;
  }
#else
  DCHECK(SbSocketIsValid(socket_));
  SbSocketAddress sb_address;
  if (!SbSocketGetLocalAddress(socket_, &sb_address)) {
    SbSocketError starboard_error = SbSocketGetLastError(socket_);
    return MapSocketError(starboard_error);
  }
  if (!address->FromSbSocketAddress(&sb_address)) {
    return ERR_ADDRESS_INVALID;
  }
#endif

  return OK;
}

int TCPSocketStarboard::GetPeerAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = *peer_address_;
  return OK;
}

void TCPSocketStarboard::DetachFromThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

SocketDescriptor TCPSocketStarboard::ReleaseSocketDescriptorForTesting() {
  SocketDescriptor socket_descriptor = socket_;
#if SB_API_VERSION >= 16
  socket_ = -1;
#else
  socket_ = kSbSocketInvalid;
#endif
  Close();
  return socket_descriptor;
}

SocketDescriptor TCPSocketStarboard::SocketDescriptorForTesting() const {
  return socket_;
}

void TCPSocketStarboard::ApplySocketTag(const SocketTag& tag) {
#if SB_API_VERSION >= 16
  if (socket_ >= 0) {
#else
  if (SbSocketIsValid(socket_)) {
#endif
    if (tag != tag_) {
      tag.Apply(socket_);
    }
  }
  tag_ = tag;
}

void TCPSocketStarboard::ClearWatcherIfOperationsNotPending() {
  if (!read_pending() && !write_pending() && !accept_pending() &&
      !connect_pending()) {
    bool ok = socket_watcher_.StopWatchingSocket();
    DCHECK(ok);
  }
}

int TCPSocketStarboard::BindToNetwork(handles::NetworkHandle network) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int TCPSocketStarboard::SetIPv6Only(bool ipv6_only) {
    NOTIMPLEMENTED();
    return 0;
}

}  // namespace net
