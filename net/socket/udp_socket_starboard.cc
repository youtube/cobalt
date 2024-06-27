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

// Adapted from udp_socket_libevent.cc

#include "net/socket/udp_socket_starboard.h"

#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/udp_net_log_parameters.h"
#include "starboard/common/socket.h"
#include "starboard/system.h"

namespace net {

UDPSocketStarboard::UDPSocketStarboard(DatagramSocket::BindType bind_type,
                                       net::NetLog* net_log,
                                       const net::NetLogSource& source)
    : write_async_watcher_(std::make_unique<WriteAsyncWatcher>(this)),
      sender_(new UDPSocketStarboardSender()),
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
      socket_(-1),
#else
      socket_(kSbSocketInvalid),
#endif
      socket_options_(0),
      bind_type_(bind_type),
      socket_watcher_(FROM_HERE),
      read_buf_len_(0),
      recv_from_address_(NULL),
      write_buf_len_(0),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::UDP_SOCKET)),
      weak_factory_(this) {
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE,
                      source);
}

UDPSocketStarboard::~UDPSocketStarboard() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Close();
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

int UDPSocketStarboard::Open(AddressFamily address_family) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ < 0);
#else
  DCHECK(!SbSocketIsValid(socket_));
#endif

  auto owned_socket_count = TryAcquireGlobalUDPSocketCount();
  if (owned_socket_count.empty())
    return ERR_INSUFFICIENT_RESOURCES;

  owned_socket_count_ = std::move(owned_socket_count);
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  int socket_family;
  switch(address_family) {
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

  socket_ = socket(socket_family, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ < 0) {
    return errno;
  }
#else
  address_type_ =
      (address_family == ADDRESS_FAMILY_IPV6 ? kSbSocketAddressTypeIpv6
                                             : kSbSocketAddressTypeIpv4);
  socket_ = SbSocketCreate(address_type_, kSbSocketProtocolUdp);
  if (!SbSocketIsValid(socket_)) {
    owned_socket_count_.Reset();
    return MapSystemError(SbSystemGetLastError());
  }
#endif  // SB_API_VERSION >= 16 || SB_IS(MODULAR)

  return OK;
}


#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
int UDPSocketStarboard::AdoptOpenedSocket(AddressFamily address_family,
                                          const int socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, -1);
#else
int UDPSocketStarboard::AdoptOpenedSocket(AddressFamily address_family,
                                          const SbSocket& socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, kSbSocketInvalid);
#endif
  auto owned_socket_count = TryAcquireGlobalUDPSocketCount();
  if (owned_socket_count.empty()) {
    return ERR_INSUFFICIENT_RESOURCES;
  }

  owned_socket_count_ = std::move(owned_socket_count);
  socket_ = socket;
  return OK;
}

void UDPSocketStarboard::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  owned_socket_count_.Reset();
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  if (socket_ <= -1)
#else
  if (socket_ == kSbSocketInvalid)
#endif
    return;

  // Zero out any pending read/write callback state.
  read_buf_ = NULL;
  read_buf_len_ = 0;
  read_callback_.Reset();
  recv_from_address_ = NULL;
  write_buf_ = NULL;
  write_buf_len_ = 0;
  write_callback_.Reset();
  send_to_address_.reset();

  bool ok = socket_watcher_.StopWatchingSocket();
  DCHECK(ok);
  is_connected_ = false;

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  if (close(socket_) != 0) {
    DPLOG(ERROR) << "Socket close";
  }
  socket_ = -1;
#else
  if (!SbSocketDestroy(socket_)) {
    DPLOG(ERROR) << "SbSocketDestroy";
  }

  socket_ = kSbSocketInvalid;
#endif
}

int UDPSocketStarboard::GetPeerAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  DCHECK(remote_address_);
  *address = *remote_address_;
  return OK;
}

int UDPSocketStarboard::GetLocalAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  if (!local_address_.get()) {
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
    struct sockaddr_in6 saddr = {0};
    socklen_t length = sizeof(saddr);
    if (0 != getsockname(socket_, reinterpret_cast<struct sockaddr*>(&saddr), &length))
      return errno;

    std::unique_ptr<IPEndPoint> endpoint(new IPEndPoint());
    if (!endpoint->FromSockAddr(reinterpret_cast<struct sockaddr*>(&saddr), length))
      return ERR_FAILED;
#else
    SbSocketAddress address;
    if (!SbSocketGetLocalAddress(socket_, &address))
      return MapSocketError(SbSocketGetLastError(socket_));
    std::unique_ptr<IPEndPoint> endpoint(new IPEndPoint());
    if (!endpoint->FromSbSocketAddress(&address))
      return ERR_FAILED;
#endif

    local_address_.reset(endpoint.release());
    net_log_.AddEvent(NetLogEventType::UDP_LOCAL_ADDRESS, [&] {
      return CreateNetLogUDPConnectParams(*local_address_,
                                          handles::kInvalidNetworkHandle);
    });
  }

  *address = *local_address_;
  return OK;
}

int UDPSocketStarboard::Read(IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback) {
  return RecvFrom(buf, buf_len, NULL, std::move(callback));
}

int UDPSocketStarboard::RecvFrom(IOBuffer* buf,
                                 int buf_len,
                                 IPEndPoint* address,
                                 CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK_NE(-1, socket_);
#else
  DCHECK_NE(kSbSocketInvalid, socket_);
#endif
  DCHECK(read_callback_.is_null());
  DCHECK(!recv_from_address_);
  DCHECK(!callback.is_null());  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int nread = InternalRecvFrom(buf, buf_len, address);
  if (nread != ERR_IO_PENDING)
    return nread;

  if (!base::CurrentIOThread::Get()->Watch(
          socket_, true, base::MessagePumpIOStarboard::WATCH_READ,
          &socket_watcher_, this)) {
    PLOG(ERROR) << "WatchSocket failed on read";
#if SB_API_VERSION >= 16
    int result = errno;
#else
    Error result = MapLastSocketError(socket_);
    if (result == ERR_IO_PENDING) {
      // Watch(...) might call SbSocketWaiterAdd() which does not guarantee
      // setting system error on failure, but we need to treat this as an
      // error since watching the socket failed.
      result = ERR_FAILED;
    }
#endif
    LogRead(result, NULL, NULL);
    return result;
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  recv_from_address_ = address;
  read_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int UDPSocketStarboard::Write(IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback,
                              const NetworkTrafficAnnotationTag&) {
  DCHECK(remote_address_);
  return SendToOrWrite(buf, buf_len, remote_address_.get(),
                       std::move(callback));
}

int UDPSocketStarboard::SendTo(IOBuffer* buf,
                               int buf_len,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) {
  return SendToOrWrite(buf, buf_len, &address, std::move(callback));
}

int UDPSocketStarboard::SendToOrWrite(IOBuffer* buf,
                                      int buf_len,
                                      const IPEndPoint* address,
                                      CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ >= 0);
#else
  DCHECK(SbSocketIsValid(socket_));
#endif
  DCHECK(write_callback_.is_null());
  DCHECK(!callback.is_null());  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int result = InternalSendTo(buf, buf_len, address);
  if (result != ERR_IO_PENDING)
    return result;

  if (!base::CurrentIOThread::Get()->Watch(
          socket_, true, base::MessagePumpIOStarboard::WATCH_WRITE,
          &socket_watcher_, this)) {
#if SB_API_VERSION >= 16
    DVLOG(1) << "Watch failed on write, error "
             << errno;
    int result = errno;
#else
    DVLOG(1) << "Watch failed on write, error "
             << SbSocketGetLastError(socket_);
    Error result = MapLastSocketError(socket_);
#endif
    LogWrite(result, NULL, NULL);
    return result;
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  DCHECK(!send_to_address_.get());
  if (address) {
    send_to_address_.reset(new IPEndPoint(*address));
  }
  write_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int UDPSocketStarboard::Connect(const IPEndPoint& address) {
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ >= 0);
#else
  DCHECK(SbSocketIsValid(socket_));
#endif
  net_log_.BeginEvent(NetLogEventType::UDP_CONNECT, [&] {
    return CreateNetLogUDPConnectParams(address,
                                        handles::kInvalidNetworkHandle);
  });
  int rv = InternalConnect(address);
  is_connected_ = (rv == OK);
  net_log_.EndEventWithNetErrorCode(NetLogEventType::UDP_CONNECT, rv);
  return rv;
}

int UDPSocketStarboard::InternalConnect(const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ >= 0);
#else
  DCHECK(SbSocketIsValid(socket_));
#endif
  DCHECK(!is_connected());
  DCHECK(!remote_address_.get());

  int rv = 0;
  // Cobalt does random bind despite bind_type_ because we do not connect
  // UDP sockets but Chromium does. And if a socket does recvfrom() without
  // any sendto() before, it needs to be bound to have a local port.
  rv = RandomBind(address.GetFamily() == ADDRESS_FAMILY_IPV4 ?
                      IPAddress::IPv4AllZeros() : IPAddress::IPv6AllZeros());

  if (rv != OK)
    return rv;

  remote_address_.reset(new IPEndPoint(address));

  return OK;
}

int UDPSocketStarboard::Bind(const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ >= 0);
#else
  DCHECK(SbSocketIsValid(socket_));
#endif
  DCHECK(!is_connected());

  int rv = DoBind(address);
  if (rv != OK)
    return rv;
  local_address_.reset();
  is_connected_ = true;
  return OK;
}

int UDPSocketStarboard::BindToNetwork(handles::NetworkHandle network) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int UDPSocketStarboard::SetReceiveBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int result = OK;

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ >= 0);
  if (setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, "SO_RCVBUF", size) != 0) {
    result = errno;
  }
  DCHECK_EQ(result, OK) << "Could not " << __FUNCTION__ << ": ";
#else
  DCHECK(SbSocketIsValid(socket_));

  if (!SbSocketSetReceiveBufferSize(socket_, size)) {
    result = MapLastSocketError(socket_);
  }
  DCHECK_EQ(result, OK) << "Could not " << __FUNCTION__ << ": "
                        << SbSocketGetLastError(socket_);
#endif

  return result;
}

int UDPSocketStarboard::SetSendBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int result = OK;
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ >= 0);
  if (setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, "SO_SNDBUF", size) != 0) {
    result = errno;
  }
  DCHECK_EQ(result, OK) << "Could not " << __FUNCTION__ << ": ";
#else
  DCHECK(SbSocketIsValid(socket_));
  if (!SbSocketSetReceiveBufferSize(socket_, size)) {
    result = MapLastSocketError(socket_);
  }
  DCHECK_EQ(result, OK) << "Could not " << __FUNCTION__ << ": "
                        << SbSocketGetLastError(socket_);
#endif

  return result;
}

int UDPSocketStarboard::AllowAddressReuse() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ >= 0);
  const int on = 1;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
    return errno;
  }
  return OK;
#else
  DCHECK(SbSocketIsValid(socket_));

  return SbSocketSetReuseAddress(socket_, true) ? OK : ERR_FAILED;
#endif

}

int UDPSocketStarboard::SetBroadcast(bool broadcast) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ >= 0);
  int on = broadcast? 1:0;
  return setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) == 0 ?
         OK : ERR_FAILED;
#else
  DCHECK(SbSocketIsValid(socket_));

  return SbSocketSetBroadcast(socket_, broadcast) ? OK : ERR_FAILED;
#endif

}

int UDPSocketStarboard::AllowAddressSharingForMulticast() {
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK_NE(-1, socket_);
#else
  DCHECK_NE(kSbSocketInvalid, socket_);
#endif
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());

  int rv = AllowAddressReuse();
  if (rv != OK)
    return rv;

  return OK;
}

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
void UDPSocketStarboard::OnSocketReadyToRead(int /*socket*/) {
  if (!read_callback_.is_null())
    DidCompleteRead();
}

void UDPSocketStarboard::OnSocketReadyToWrite(int socket) {
  if (write_async_watcher_->watching()) {
    write_async_watcher_->OnSocketReadyToWrite(socket);
    return;
  }

  if (!write_callback_.is_null())
    DidCompleteWrite();
}

void UDPSocketStarboard::WriteAsyncWatcher::OnSocketReadyToWrite(
    int /*socket*/) {
  DVLOG(1) << __func__ << " queue " << socket_->pending_writes_.size()
           << " out of " << socket_->write_async_outstanding_ << " total";
  socket_->StopWatchingSocket();
  socket_->FlushPending();
}
#else
void UDPSocketStarboard::OnSocketReadyToRead(SbSocket /*socket*/) {
  if (!read_callback_.is_null())
    DidCompleteRead();
}

void UDPSocketStarboard::OnSocketReadyToWrite(SbSocket socket) {
  if (write_async_watcher_->watching()) {
    write_async_watcher_->OnSocketReadyToWrite(socket);
    return;
  }

  if (!write_callback_.is_null())
    DidCompleteWrite();
}

void UDPSocketStarboard::WriteAsyncWatcher::OnSocketReadyToWrite(
    SbSocket /*socket*/) {
  DVLOG(1) << __func__ << " queue " << socket_->pending_writes_.size()
           << " out of " << socket_->write_async_outstanding_ << " total";
  socket_->StopWatchingSocket();
  socket_->FlushPending();
}
#endif


void UDPSocketStarboard::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!read_callback_.is_null());

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionOnceCallback c = std::move(read_callback_);
  read_callback_.Reset();
  std::move(c).Run(rv);
}

void UDPSocketStarboard::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!write_callback_.is_null());

  // Run may result in Write being called.
  std::move(write_callback_).Run(rv);
}

void UDPSocketStarboard::DidCompleteRead() {
  int result = InternalRecvFrom(read_buf_, read_buf_len_, recv_from_address_);
  if (result != ERR_IO_PENDING) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    recv_from_address_ = NULL;
    InternalStopWatchingSocket();
    DoReadCallback(result);
  }
}

void UDPSocketStarboard::LogRead(int result,
                                 const char* bytes,
                                 const IPEndPoint* address) const {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLogEventType::UDP_RECEIVE_ERROR,
                                      result);
    return;
  }

  if (net_log_.IsCapturing()) {
    NetLogUDPDataTransfer(net_log_, NetLogEventType::UDP_BYTES_RECEIVED, result,
                          bytes, address);
  }

  activity_monitor::IncrementBytesReceived(result);
}

void UDPSocketStarboard::DidCompleteWrite() {
  int result =
      InternalSendTo(write_buf_, write_buf_len_, send_to_address_.get());

  if (result != ERR_IO_PENDING) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    send_to_address_.reset();
    InternalStopWatchingSocket();
    DoWriteCallback(result);
  }
}

void UDPSocketStarboard::LogWrite(int result,
                                  const char* bytes,
                                  const IPEndPoint* address) const {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLogEventType::UDP_SEND_ERROR, result);
    return;
  }

  if (net_log_.IsCapturing()) {
    NetLogUDPDataTransfer(net_log_, NetLogEventType::UDP_BYTES_SENT, result,
                          bytes, address);
  }

  activity_monitor::IncrementBytesReceived(result);
}

int UDPSocketStarboard::InternalRecvFrom(IOBuffer* buf,
                                         int buf_len,
                                         IPEndPoint* address) {
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  struct sockaddr_in6 saddr = {0};
  socklen_t length = sizeof(saddr);
  int bytes_transferred = recvfrom(socket_, buf->data(), buf_len, 0,
                                   reinterpret_cast<struct sockaddr*>(&saddr), &length);

  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    // Passing in NULL address is allowed. This is only to align with other
    // platform's implementation.
    if (address && !address->FromSockAddr(reinterpret_cast<struct sockaddr*>(&saddr), length)) {
      result = ERR_ADDRESS_INVALID;
    } else if (bytes_transferred == buf_len) {
      result = ERR_MSG_TOO_BIG;
    }
  } else {
    result = errno;
  }

  if (result != ERR_IO_PENDING) {
    IPEndPoint log_address;
    if (result < 0 || !log_address.FromSockAddr(reinterpret_cast<struct sockaddr*>(&saddr), length)) {
      LogRead(result, buf->data(), NULL);
    } else {
      LogRead(result, buf->data(), &log_address);
    }
  }
#else
  SbSocketAddress sb_address;
  int bytes_transferred =
      SbSocketReceiveFrom(socket_, buf->data(), buf_len, &sb_address);
  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    // Passing in NULL address is allowed. This is only to align with other
    // platform's implementation.
    if (address && !address->FromSbSocketAddress(&sb_address)) {
      result = ERR_ADDRESS_INVALID;
    } else if (bytes_transferred == buf_len) {
      result = ERR_MSG_TOO_BIG;
    }
  } else {
    result = MapLastSocketError(socket_);
  }

  if (result != ERR_IO_PENDING) {
    IPEndPoint log_address;
    if (result < 0 || !log_address.FromSbSocketAddress(&sb_address)) {
      LogRead(result, buf->data(), NULL);
    } else {
      LogRead(result, buf->data(), &log_address);
    }
  }
#endif
  return result;
}

int UDPSocketStarboard::InternalSendTo(IOBuffer* buf,
                                       int buf_len,
                                       const IPEndPoint* address) {
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  struct sockaddr_in6 saddr;
  socklen_t length = sizeof(saddr);

  if (!address || !address->ToSockAddr(reinterpret_cast<struct sockaddr*>(&saddr), &length)) {
    int result = ERR_FAILED;
    LogWrite(result, NULL, NULL);
    return result;
  }
#if defined(MSG_NOSIGNAL)
  const int kSendFlags = MSG_NOSIGNAL;
#else
  const int kSendFlags = 0;
#endif
  int result = sendto(socket_, buf->data(), buf_len, kSendFlags,
                      reinterpret_cast<struct sockaddr*>(&saddr), length);

  if (result < 0)
    result = errno;
#else
  SbSocketAddress sb_address;
  if (!address || !address->ToSbSocketAddress(&sb_address)) {
    int result = ERR_FAILED;
    LogWrite(result, NULL, NULL);
    return result;
  }

  int result = SbSocketSendTo(socket_, buf->data(), buf_len, &sb_address);

  if (result < 0)
    result = MapLastSocketError(socket_);
#endif

  if (result != ERR_IO_PENDING)
    LogWrite(result, buf->data(), address);

  return result;
}

int UDPSocketStarboard::DoBind(const IPEndPoint& address) {
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  struct sockaddr_in6 saddr;
  socklen_t length = sizeof(saddr);
  if (!address.ToSockAddr(reinterpret_cast<struct sockaddr*>(&saddr), &length)) {
    return ERR_UNEXPECTED;
  }

  int rv = bind(socket_, reinterpret_cast<struct sockaddr*>(&saddr), length);
  return rv != 0 ? errno : OK;
#else
  SbSocketAddress sb_address;
  if (!address.ToSbSocketAddress(&sb_address)) {
    return ERR_UNEXPECTED;
  }

  SbSocketError rv = SbSocketBind(socket_, &sb_address);
  return rv != kSbSocketOk ? MapSystemError(SbSystemGetLastError()) : OK;
#endif
}

int UDPSocketStarboard::RandomBind(const IPAddress& address) {
  return DoBind(IPEndPoint(address, 0));
}

int UDPSocketStarboard::JoinGroup(const IPAddress& group_address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  struct sockaddr_in6 saddr = {0};
  socklen_t length = sizeof(saddr);
  if (!IPEndPoint(group_address, 0).ToSockAddr(reinterpret_cast<struct sockaddr*>(&saddr), &length)) {
    return ERR_ADDRESS_INVALID;
  }
  struct ip_mreq imreq = {0};
  in_addr_t in_addr = *reinterpret_cast<const in_addr_t*>(&saddr);
  imreq.imr_multiaddr.s_addr = in_addr;
  imreq.imr_interface.s_addr = INADDR_ANY;

  if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq,
                 sizeof(imreq))) {
    LOG(WARNING) << "Socket setsockopt JoinMulticastGroup failed on UDP socket.";
    return errno;
  }
#else
  SbSocketAddress sb_address = {0};
  if (!IPEndPoint(group_address, 0).ToSbSocketAddress(&sb_address)) {
    return ERR_ADDRESS_INVALID;
  }

  if (!SbSocketJoinMulticastGroup(socket_, &sb_address)) {
    LOG(WARNING) << "SbSocketJoinMulticastGroup failed on UDP socket.";
    return MapLastSocketError(socket_);
  }
#endif  // SB_API_VERSION >= 16 || SB_IS(MODULAR)


  return OK;
}

int UDPSocketStarboard::LeaveGroup(const IPAddress& group_address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  DCHECK(false) << "Not supported on Starboard.";
  return ERR_FAILED;
}

int UDPSocketStarboard::SetMulticastInterface(uint32_t interface_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected())
    return ERR_SOCKET_IS_CONNECTED;
  DCHECK_EQ(0, interface_index)
      << "Only the default multicast interface is supported on Starboard.";
  return interface_index == 0 ? OK : ERR_FAILED;
}

int UDPSocketStarboard::SetMulticastTimeToLive(int time_to_live) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected())
    return ERR_SOCKET_IS_CONNECTED;

  DCHECK(false) << "Not supported on Starboard.";
  return ERR_FAILED;
}

int UDPSocketStarboard::SetMulticastLoopbackMode(bool loopback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected())
    return ERR_SOCKET_IS_CONNECTED;

  DCHECK(false) << "Not supported on Starboard.";
  return ERR_FAILED;
}

int UDPSocketStarboard::SetDiffServCodePoint(DiffServCodePoint dscp) {
  NOTIMPLEMENTED();
  return OK;
}

void UDPSocketStarboard::DetachFromThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

void UDPSocketStarboard::ApplySocketTag(const SocketTag&) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // SocketTag is not applicable to Starboard, see socket_tag.h for more info.
  NOTIMPLEMENTED_LOG_ONCE();
}

UDPSocketStarboardSender::UDPSocketStarboardSender() {}
UDPSocketStarboardSender::~UDPSocketStarboardSender() {}

SendResult::SendResult() : rv(0), write_count(0) {}
SendResult::~SendResult() {}
SendResult::SendResult(int _rv, int _write_count, DatagramBuffers _buffers)
    : rv(_rv), write_count(_write_count), buffers(std::move(_buffers)) {}
SendResult::SendResult(SendResult&& other) = default;

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
SendResult UDPSocketStarboardSender::InternalSendBuffers(
    const int socket,
    DatagramBuffers buffers,
    struct sockaddr* paddress,
    socklen_t len) const {
  int rv = 0;
  int write_count = 0;
#if defined(MSG_NOSIGNAL)
  const int kSendFlags = MSG_NOSIGNAL;
#else
  const int kSendFlags = 0;
#endif
  for (auto& buffer : buffers) {
    int result = sendto(socket, buffer->data(), buffer->length(), kSendFlags,
                        paddress, len);
    if (result < 0) {

      rv = errno;
      break;
    }
    write_count++;
  }
  return SendResult(rv, write_count, std::move(buffers));
}

SendResult UDPSocketStarboardSender::SendBuffers(const int socket,
                                                 DatagramBuffers buffers,
                                                 struct sockaddr* paddress,
                                                 socklen_t len) {
  return InternalSendBuffers(socket, std::move(buffers), paddress, len);
}
#else
SendResult UDPSocketStarboardSender::InternalSendBuffers(
    const SbSocket& socket,
    DatagramBuffers buffers,
    SbSocketAddress address) const {
  int rv = 0;
  int write_count = 0;
  for (auto& buffer : buffers) {
    int result = Send(socket, buffer->data(), buffer->length(), address);
    if (result < 0) {
      rv = MapLastSocketError(socket);
      break;
    }
    write_count++;
  }
  return SendResult(rv, write_count, std::move(buffers));
}

SendResult UDPSocketStarboardSender::SendBuffers(const SbSocket& socket,
                                                 DatagramBuffers buffers,
                                                 SbSocketAddress address) {
  return InternalSendBuffers(socket, std::move(buffers), address);
}
#endif

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
int UDPSocketStarboardSender::Send(const int socket,
                                   const char* buf,
                                   size_t len,
                                   struct sockaddr* paddress,
                                   socklen_t length) const {
#if defined(MSG_NOSIGNAL)
  const int kSendFlags = MSG_NOSIGNAL;
#else
  const int kSendFlags = 0;
#endif
  return sendto(socket, buf, len, kSendFlags, paddress, length);
}
#else
int UDPSocketStarboardSender::Send(const SbSocket& socket,
                                   const char* buf,
                                   size_t len,
                                   SbSocketAddress address) const {
  return SbSocketSendTo(socket, buf, len, &address);
}
#endif  // SB_API_VERSION >= 16 || SB_IS(MODULAR)

int UDPSocketStarboard::WriteAsync(
    const char* buffer,
    size_t buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(datagram_buffer_pool_ != nullptr);
  IncreaseWriteAsyncOutstanding(1);
  datagram_buffer_pool_->Enqueue(buffer, buf_len, &pending_writes_);
  return InternalWriteAsync(std::move(callback), traffic_annotation);
}

int UDPSocketStarboard::WriteAsync(
    DatagramBuffers buffers,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  IncreaseWriteAsyncOutstanding(buffers.size());
  pending_writes_.splice(pending_writes_.end(), std::move(buffers));
  return InternalWriteAsync(std::move(callback), traffic_annotation);
}

int UDPSocketStarboard::InternalWriteAsync(
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(write_callback_.is_null());

  // Surface error immediately if one is pending.
  if (last_async_result_ < 0) {
    return ResetLastAsyncResult();
  }

  size_t flush_threshold =
      write_batching_active_ ? kWriteAsyncPostBuffersThreshold : 1;
  if (pending_writes_.size() >= flush_threshold) {
    FlushPending();
    // Surface error immediately if one is pending.
    if (last_async_result_ < 0) {
      return ResetLastAsyncResult();
    }
  }

  if (!write_async_timer_running_) {
    write_async_timer_running_ = true;
    write_async_timer_.Start(FROM_HERE, kWriteAsyncMsThreshold, this,
                             &UDPSocketStarboard::OnWriteAsyncTimerFired);
  }

  int blocking_threshold =
      write_batching_active_ ? kWriteAsyncMaxBuffersThreshold : 1;
  if (write_async_outstanding_ >= blocking_threshold) {
    write_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  DVLOG(2) << __func__ << " pending " << pending_writes_.size()
           << " outstanding " << write_async_outstanding_;
  return ResetWrittenBytes();
}

DatagramBuffers UDPSocketStarboard::GetUnwrittenBuffers() {
  write_async_outstanding_ -= pending_writes_.size();
  return std::move(pending_writes_);
}

void UDPSocketStarboard::FlushPending() {
  // Nothing to do if socket is blocked.
  if (write_async_watcher_->watching())
    return;

  if (pending_writes_.empty())
    return;

  if (write_async_timer_running_)
    write_async_timer_.Reset();

  int num_pending_writes = static_cast<int>(pending_writes_.size());
  if (!write_multi_core_enabled_ ||
      // Don't bother with post if not enough buffers
      (num_pending_writes <= kWriteAsyncMinBuffersThreshold &&
       // but not if there is a previous post
       // outstanding, to prevent out of order transmission.
       (num_pending_writes == write_async_outstanding_))) {
    LocalSendBuffers();
  } else {
    PostSendBuffers();
  }
}

// TODO(ckrasic) Sad face.  Do this lazily because many tests exploded
// otherwise.  |threading_and_tasks.md| advises to instantiate a
// |base::test::ScopedTaskEnvironment| in the test, implementing that
// for all tests that might exercise QUIC is too daunting.  Also, in
// some tests it seemed like following the advice just broke in other
// ways.
base::SequencedTaskRunner* UDPSocketStarboard::GetTaskRunner() {
  if (task_runner_ == nullptr) {
    task_runner_ =
        base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits());
  }
  return task_runner_.get();
}

void UDPSocketStarboard::OnWriteAsyncTimerFired() {
  DVLOG(2) << __func__ << " pending writes " << pending_writes_.size();
  if (pending_writes_.empty()) {
    write_async_timer_.Stop();
    write_async_timer_running_ = false;
    return;
  }
  if (last_async_result_ < 0) {
    DVLOG(1) << __func__ << " socket not writeable";
    return;
  }
  FlushPending();
}

void UDPSocketStarboard::LocalSendBuffers() {
  DVLOG(1) << __func__ << " queue " << pending_writes_.size() << " out of "
           << write_async_outstanding_ << " total";
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  struct sockaddr_in6 saddr = {0};
  socklen_t length = sizeof(saddr);
  int result = remote_address_.get()->ToSockAddr(reinterpret_cast<struct sockaddr*>(&saddr), &length);
#else
  SbSocketAddress sb_address;
  int result = remote_address_.get()->ToSbSocketAddress(&sb_address);
#endif
  DCHECK(result);
  DidSendBuffers(
      sender_->SendBuffers(socket_, std::move(pending_writes_),
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
                           reinterpret_cast<struct sockaddr*>(&saddr), length));
#else
                           sb_address));
#endif
}

void UDPSocketStarboard::PostSendBuffers() {
  DVLOG(1) << __func__ << " queue " << pending_writes_.size() << " out of "
           << write_async_outstanding_ << " total";
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  struct sockaddr_in6 saddr;
  socklen_t length = sizeof(saddr);
  DCHECK(remote_address_.get()->ToSockAddr(reinterpret_cast<struct sockaddr*>(&saddr), &length));
#else
  SbSocketAddress sb_address;
  DCHECK(remote_address_.get()->ToSbSocketAddress(&sb_address));
#endif
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UDPSocketStarboardSender::SendBuffers, sender_, socket_,
                     std::move(pending_writes_),
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
                               reinterpret_cast<struct sockaddr*>(&saddr), length),
#else
                               sb_address),
#endif
      base::BindOnce(&UDPSocketStarboard::DidSendBuffers,
                     weak_factory_.GetWeakPtr()));
}

void UDPSocketStarboard::DidSendBuffers(SendResult send_result) {
  DVLOG(3) << __func__;
  int write_count = send_result.write_count;
  DatagramBuffers& buffers = send_result.buffers;

  DCHECK(!buffers.empty());
  int num_buffers = buffers.size();

  // Dequeue buffers that have been written.
  if (write_count > 0) {
    write_async_outstanding_ -= write_count;

    DatagramBuffers::iterator it;
    // Generate logs for written buffers
    it = buffers.begin();
    for (int i = 0; i < write_count; i++, it++) {
      auto& buffer = *it;
      LogWrite(buffer->length(), buffer->data(), NULL);
      written_bytes_ += buffer->length();
    }
    // Return written buffers to pool
    DatagramBuffers written_buffers;
    if (write_count == num_buffers) {
      it = buffers.end();
    } else {
      it = buffers.begin();
      for (int i = 0; i < write_count; i++) {
        it++;
      }
    }
    written_buffers.splice(written_buffers.end(), buffers, buffers.begin(), it);
    DCHECK(datagram_buffer_pool_ != nullptr);
    datagram_buffer_pool_->Dequeue(&written_buffers);
  }

  // Requeue left-over (unwritten) buffers.
  if (!buffers.empty()) {
    DVLOG(2) << __func__ << " requeue " << buffers.size() << " buffers";
    pending_writes_.splice(pending_writes_.begin(), std::move(buffers));
  }

  last_async_result_ = send_result.rv;
  if (last_async_result_ == ERR_IO_PENDING) {
    DVLOG(2) << __func__ << " WatchSocket start";
    if (!WatchSocket()) {
#if SB_API_VERSION >= 16
      last_async_result_ = errno;
#else
      last_async_result_ = MapLastSocketError(socket_);
#endif
      DVLOG(1) << "WatchSocket failed on write, error: " << last_async_result_;
      LogWrite(last_async_result_, NULL, NULL);
    } else {
      last_async_result_ = 0;
    }
  } else if (last_async_result_ < 0 || pending_writes_.empty()) {
    DVLOG(2) << __func__ << " WatchSocket stop: result "
             << ErrorToShortString(last_async_result_) << " pending_writes "
             << pending_writes_.size();
    StopWatchingSocket();
  }
  DCHECK(last_async_result_ != ERR_IO_PENDING);

  if (write_callback_.is_null())
    return;

  if (last_async_result_ < 0) {
    DVLOG(1) << last_async_result_;
    // Update the writer with the latest result.
    DoWriteCallback(ResetLastAsyncResult());
  } else if (write_async_outstanding_ < kWriteAsyncCallbackBuffersThreshold) {
    DVLOG(1) << write_async_outstanding_ << " < "
             << kWriteAsyncCallbackBuffersThreshold;
    DoWriteCallback(ResetWrittenBytes());
  }
}

int UDPSocketStarboard::SetDoNotFragment() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  DCHECK(socket_ >= 0);
#else
  DCHECK(SbSocketIsValid(socket_));
#endif

  // Starboard does not supported sending non-fragmented packets yet.
  // All QUIC Streams call this function at initialization, setting sockets to
  // send non-fragmented packets may have a slight performance boost.
  return ERR_NOT_IMPLEMENTED;
}

void UDPSocketStarboard::SetMsgConfirm(bool confirm) {
  NOTIMPLEMENTED();
}

bool UDPSocketStarboard::WatchSocket() {
  if (write_async_watcher_->watching())
    return true;
  bool result = InternalWatchSocket();
  if (result) {
    write_async_watcher_->set_watching(true);
  }
  return result;
}

void UDPSocketStarboard::StopWatchingSocket() {
  if (!write_async_watcher_->watching())
    return;
  write_async_watcher_->set_watching(false);
  InternalStopWatchingSocket();
}

bool UDPSocketStarboard::InternalWatchSocket() {
  return base::CurrentIOThread::Get()->Watch(
      socket_, true, base::MessagePumpIOStarboard::WATCH_WRITE,
      &socket_watcher_, this);
}

void UDPSocketStarboard::InternalStopWatchingSocket() {
  if (!read_buf_ && !write_buf_ && !write_async_watcher_->watching()) {
    bool ok = socket_watcher_.StopWatchingSocket();
    DCHECK(ok);
  }
}

void UDPSocketStarboard::SetMaxPacketSize(size_t max_packet_size) {
  datagram_buffer_pool_ = std::make_unique<DatagramBufferPool>(max_packet_size);
}

int UDPSocketStarboard::ResetLastAsyncResult() {
  int result = last_async_result_;
  last_async_result_ = 0;
  return result;
}

int UDPSocketStarboard::ResetWrittenBytes() {
  int bytes = written_bytes_;
  written_bytes_ = 0;
  return bytes;
}

}  // namespace net
