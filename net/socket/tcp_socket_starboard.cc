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

#include "base/callback_helpers.h"
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
      socket_(kSbSocketInvalid),
      family_(ADDRESS_FAMILY_UNSPECIFIED),
      logging_multiple_connect_attempts_(false),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)),
      listening_(false),
      waiting_connect_(false) {
  net_log_.BeginEvent(NetLogEventType::SOCKET_ALIVE,
                      source.ToEventParametersCallback());
}

TCPSocketStarboard::~TCPSocketStarboard() {
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
  Close();
}

int TCPSocketStarboard::Open(AddressFamily family) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!SbSocketIsValid(socket_));
  socket_ = SbSocketCreate(ConvertAddressFamily(family), kSbSocketProtocolTcp);
  if (!SbSocketIsValid(socket_)) {
    return MapLastSystemError();
  }

  family_ = family;
  return OK;
}

int TCPSocketStarboard::AdoptConnectedSocket(SocketDescriptor socket,
                                             const IPEndPoint& peer_address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!SbSocketIsValid(socket_));

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
  DCHECK(!SbSocketIsValid(socket_));

  socket_ = socket;
  return OK;
}

int TCPSocketStarboard::Bind(const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

  local_address_.reset(new IPEndPoint(address));

  return OK;
}

int TCPSocketStarboard::Listen(int backlog) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(backlog, 0);
  DCHECK(SbSocketIsValid(socket_));
  DCHECK(local_address_) << "Must have previously called Bind().";

  SbSocketError error = SbSocketListen(socket_);
  if (error != kSbSocketOk) {
    DLOG(ERROR) << "SbSocketListen() returned an error";
    int rv = MapLastSocketError(socket_);
    Close();
    return rv;
  }

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

  net_log_.BeginEvent(NetLogEventType::TCP_ACCEPT);

  int result = AcceptInternal(socket, address);

  if (result == ERR_IO_PENDING) {
    if (!base::MessageLoopForIO::current()->Watch(
            socket_, true, base::MessageLoopCurrentForIO::WATCH_READ,
            &socket_watcher_, this)) {
      DLOG(ERROR) << "WatchSocket failed on read";
      return MapLastSocketError(socket_);
    }

    accept_socket_ = socket;
    accept_address_ = address;
    accept_callback_ = std::move(callback);
  }

  return result;
}

int TCPSocketStarboard::SetDefaultOptionsForServer() {
  DCHECK(SbSocketIsValid(socket_));
  if (!SbSocketSetReuseAddress(socket_, true)) {
    return MapLastSocketError(socket_);
  }
  return OK;
}

void TCPSocketStarboard::SetDefaultOptionsForClient() {
  SbSocketSetTcpNoDelay(socket_, true);

  const SbTime kTCPKeepAliveDuration = 45 * kSbTimeSecond;
  SbSocketSetTcpKeepAlive(socket_, true, kTCPKeepAliveDuration);

  SbSocketSetTcpWindowScaling(socket_, true);

  if (kSbNetworkReceiveBufferSize != 0) {
    SbSocketSetReceiveBufferSize(socket_, kSbNetworkReceiveBufferSize);
  }
}

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
                    CreateNetLogIPEndPointCallback(&ip_end_point));
  return OK;
}

void TCPSocketStarboard::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (SbSocketIsValid(socket_)) {
    StopWatchingAndCleanUp();

    net_log_.AddEvent(NetLogEventType::SOCKET_CLOSED);

    if (!SbSocketDestroy(socket_)) {
      DPLOG(ERROR) << "SbSocketDestroy";
    }
    socket_ = kSbSocketInvalid;
  }

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

void TCPSocketStarboard::OnSocketReadyToRead(SbSocket socket) {
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

void TCPSocketStarboard::OnSocketReadyToWrite(SbSocket socket) {
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(SbSocketIsValid(socket_));
  DCHECK(!peer_address_);
  DCHECK(!connect_pending());

  if (!logging_multiple_connect_attempts_) {
    LogConnectBegin(AddressList(address));
  }

  net_log_.BeginEvent(NetLogEventType::TCP_CONNECT_ATTEMPT,
                      CreateNetLogIPEndPointCallback(&address));

  SbSocketAddress storage;
  if (!address.ToSbSocketAddress(&storage)) {
    return ERR_ADDRESS_INVALID;
  }

  peer_address_.reset(new IPEndPoint(address));

  SbSocketError result = SbSocketConnect(socket_, &storage);
  DCHECK_NE(kSbSocketErrorFailed, result);

  int rv = MapLastSocketError(socket_);
  if (rv != ERR_IO_PENDING) {
    return HandleConnectCompleted(rv);
  }

  waiting_connect_ = true;
  write_callback_ = std::move(callback);

  // When it is ready to write, it will have connected.
  base::MessageLoopForIO::current()->Watch(
      socket_, true, base::MessageLoopCurrentForIO::WATCH_WRITE,
      &socket_watcher_, this);

  return ERR_IO_PENDING;
}

int TCPSocketStarboard::HandleConnectCompleted(int rv) {
  // Log the end of this attempt (and any OS error it threw).
  if (rv != OK) {
    net_log_.EndEvent(NetLogEventType::TCP_CONNECT_ATTEMPT,
                      NetLog::IntCallback("mapped_error", rv));
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
  int rv = SbSocketIsConnected(socket_) ? OK : ERR_FAILED;

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

  return SbSocketIsConnected(socket_);
}

bool TCPSocketStarboard::IsConnectedAndIdle() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return SbSocketIsConnectedAndIdle(socket_);
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
                      addresses.CreateNetLogCallback());
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
  DCHECK(SbSocketIsValid(socket_));
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
  DCHECK(SbSocketIsValid(socket_));
  DCHECK(read_if_ready_callback_.is_null());

  int rv = DoRead(buf, buf_len);

  if (rv != ERR_IO_PENDING) {
    return rv;
  }

  read_if_ready_callback_ = std::move(callback);
  base::MessageLoopForIO::current()->Watch(
      socket_, true, base::MessageLoopCurrentForIO::WATCH_READ,
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
  base::ResetAndReturn(&read_callback_).Run(rv);
}

int TCPSocketStarboard::DoRead(IOBuffer* buf, int buf_len) {
  int bytes_read = SbSocketReceiveFrom(socket_, buf->data(), buf_len, NULL);

  if (bytes_read >= 0) {
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED,
                                  bytes_read, buf->data());
    NetworkActivityMonitor::GetInstance()->IncrementBytesReceived(bytes_read);

    return bytes_read;
  } else {
    // If |bytes_read| < 0, some kind of error occurred.
    SbSocketError starboard_error = SbSocketGetLastError(socket_);
    int rv = MapSocketError(starboard_error);
    net_log_.AddEvent(NetLogEventType::SOCKET_READ_ERROR,
                      CreateNetLogSocketErrorCallback(rv, starboard_error));
    if (rv != ERR_IO_PENDING) {
      DLOG(ERROR) << __FUNCTION__ << "[" << this << "]: Error: " << rv;
    }
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
  DCHECK(SbSocketIsValid(socket_));
  DCHECK(!connect_pending());
  DCHECK(!write_pending());
  DCHECK_GT(buf_len, 0);

  int rv = DoWrite(buf, buf_len);

  if (rv == ERR_IO_PENDING) {
    // We are just blocked, so let's set up the callback.
    write_buf_ = buf;
    write_buf_len_ = buf_len;
    write_callback_ = std::move(callback);
    base::MessageLoopForIO::current()->Watch(
        socket_, true, base::MessageLoopCurrentForIO::WATCH_WRITE,
        &socket_watcher_, this);
  }

  return rv;
}

int TCPSocketStarboard::DoWrite(IOBuffer* buf, int buf_len) {
  int bytes_sent = SbSocketSendTo(socket_, buf->data(), buf_len, NULL);

  if (bytes_sent >= 0) {
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_SENT,
                                  bytes_sent, buf->data());
    NetworkActivityMonitor::GetInstance()->IncrementBytesSent(bytes_sent);

    return bytes_sent;
  } else {
    SbSocketError starboard_error = SbSocketGetLastError(socket_);
    int rv = MapSocketError(starboard_error);
    net_log_.AddEvent(NetLogEventType::SOCKET_WRITE_ERROR,
                      CreateNetLogSocketErrorCallback(rv, starboard_error));
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
  return SbSocketSetTcpKeepAlive(socket_, enable, delay);
}

bool TCPSocketStarboard::SetNoDelay(bool no_delay) {
  return SetTCPNoDelay(socket_, no_delay);
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

  SbSocketAddress sb_address;
  if (!SbSocketGetLocalAddress(socket_, &sb_address)) {
    SbSocketError starboard_error = SbSocketGetLastError(socket_);
    return MapSocketError(starboard_error);
  }

  if (!address->FromSbSocketAddress(&sb_address)) {
    return ERR_ADDRESS_INVALID;
  }

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
  socket_ = kSbSocketInvalid;
  Close();
  return socket_descriptor;
}

SocketDescriptor TCPSocketStarboard::SocketDescriptorForTesting() const {
  return socket_;
}

void TCPSocketStarboard::ApplySocketTag(const SocketTag& tag) {
  if (SbSocketIsValid(socket_) && tag != tag_) {
    tag.Apply(socket_);
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

}  // namespace net
