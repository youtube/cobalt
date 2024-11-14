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
#include "starboard/extension/socket_receive_multi_msg.h"
#include "starboard/system.h"

namespace net {

namespace {
// Read in larger batches to minimize recvmmsg overhead.
inline constexpr int kNumPacketsPerReadMmsgCall = 64;
inline constexpr size_t kDefaultUdpPacketControlBufferSize = 512;

const CobaltExtensionSocketReceiveMultiMsgApi* g_socket_extension = nullptr;
}  // namespace

UDPSocketStarboard::UDPSocketStarboard(DatagramSocket::BindType bind_type,
                                       net::NetLog* net_log,
                                       const net::NetLogSource& source)
    : write_async_watcher_(std::make_unique<WriteAsyncWatcher>(this)),
      sender_(new UDPSocketStarboardSender()),
      socket_(kSbSocketInvalid),
      socket_options_(0),
      bind_type_(bind_type),
      socket_watcher_(FROM_HERE),
      read_buf_(nullptr),
      read_buf_len_(0),
      recv_from_address_(nullptr),
      write_buf_len_(0),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::UDP_SOCKET)),
      weak_factory_(this) {
  if (g_socket_extension == nullptr) {
    // Note that this initializes a constant pointer, so even if two threads run
    // this in parallel, they will only do redundant work without overwriting
    // previous work. Therefore, there is no need for atomics, locks or memory
    // barriers here.
    g_socket_extension =
        static_cast<const CobaltExtensionSocketReceiveMultiMsgApi*>(
            SbSystemGetExtension(kCobaltExtensionSocketReceiveMultiMsgName));
  }
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
  DCHECK(!SbSocketIsValid(socket_));

  auto owned_socket_count = TryAcquireGlobalUDPSocketCount();
  if (owned_socket_count.empty())
    return ERR_INSUFFICIENT_RESOURCES;

  owned_socket_count_ = std::move(owned_socket_count);

  address_type_ =
      (address_family == ADDRESS_FAMILY_IPV6 ? kSbSocketAddressTypeIpv6
                                             : kSbSocketAddressTypeIpv4);
  socket_ = SbSocketCreate(address_type_, kSbSocketProtocolUdp);
  if (!SbSocketIsValid(socket_)) {
    owned_socket_count_.Reset();
    return MapSystemError(SbSystemGetLastError());
  }

  return OK;
}

int UDPSocketStarboard::AdoptOpenedSocket(AddressFamily address_family,
                                          const SbSocket& socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, kSbSocketInvalid);
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

  if (socket_ == kSbSocketInvalid)
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
  if (!SbSocketDestroy(socket_)) {
    DPLOG(ERROR) << "SbSocketDestroy";
  }

  socket_ = kSbSocketInvalid;
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
    SbSocketAddress address;
    if (!SbSocketGetLocalAddress(socket_, &address))
      return MapSocketError(SbSocketGetLastError(socket_));
    std::unique_ptr<IPEndPoint> endpoint(new IPEndPoint());
    if (!endpoint->FromSbSocketAddress(&address))
      return ERR_FAILED;
    local_address_.reset(endpoint.release());
    net_log_.AddEvent(NetLogEventType::UDP_LOCAL_ADDRESS, [&] {
      return CreateNetLogUDPConnectParams(*local_address_,
                                          handles::kInvalidNetworkHandle);
    });
  }

  *address = *local_address_;
  return OK;
}

int UDPSocketStarboard::ReadMultiplePackets(Socket::ReadPacketResults* results,
                                            int packet_buffer_size,
                                            CompletionOnceCallback callback) {
  if (!g_socket_extension) {
    return ERR_NOT_IMPLEMENTED;
  }
  if (!results || packet_buffer_size <= 0) {
    return ERR_INVALID_ARGUMENT;
  }

  if (!is_connected_) {
    // This is only implemented correctly for connected sockets.
    return ERR_SOCKET_NOT_CONNECTED;
  }

  if ((!results->buffer || results->buffer->size() == 0) ||
      results->packet_buffer_size != packet_buffer_size ||
      results->packets == nullptr) {
    // Request how much memory to allocate for the needed buffers and data
    // structures from the platform.
    int buffer_size = g_socket_extension->ReceiveMultiMsgBufferSize(
        kNumPacketsPerReadMmsgCall, packet_buffer_size,
        kDefaultUdpPacketControlBufferSize);
    // Calculate the space for our ReadPacketResult array.
    int out_packets_size =
        kNumPacketsPerReadMmsgCall * sizeof(Socket::ReadPacketResult);

    // Allocate all the space we will need in one buffer.
    results->buffer =
        base::MakeRefCounted<IOBufferWithSize>(buffer_size + out_packets_size);

    results->packet_buffer_size = packet_buffer_size;
    // Our packets array is at the end of the buffer.
    results->packets = reinterpret_cast<Socket::ReadPacketResult*>(
        results->buffer->data() + buffer_size);
    g_socket_extension->ReceiveMultiMsgBufferInitialize(
        kNumPacketsPerReadMmsgCall, packet_buffer_size,
        kDefaultUdpPacketControlBufferSize, results->buffer->data());
  }

  int nread = InternalReadMultiplePackets(results);
  if (callback.is_null() || nread != ERR_IO_PENDING) {
    return nread;
  }

  if (!base::CurrentIOThread::Get()->Watch(
          socket_, true, base::MessagePumpIOStarboard::WATCH_READ,
          &socket_watcher_, this)) {
    PLOG(ERROR) << "WatchSocket failed on read";
    Error result = MapLastSocketError(socket_);
    if (result == ERR_IO_PENDING) {
      // Watch(...) might call SbSocketWaiterAdd() which does not guarantee
      // setting system error on failure, but we need to treat this as an
      // error since watching the socket failed.
      result = ERR_FAILED;
    }
    LogRead(result, NULL, NULL);
    return result;
  }

  results_ = results;
  read_buf_len_ = packet_buffer_size;
  read_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int UDPSocketStarboard::Read(IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback) {
  return RecvFrom(buf, buf_len, nullptr, std::move(callback));
}

int UDPSocketStarboard::RecvFrom(IOBuffer* buf,
                                 int buf_len,
                                 IPEndPoint* address,
                                 CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(kSbSocketInvalid, socket_);
  DCHECK(read_callback_.is_null());
  DCHECK(!recv_from_address_);
  DCHECK(!callback.is_null());  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int nread = InternalRecvFrom(buf, buf_len, address);
  if (nread != ERR_IO_PENDING)
    return nread;

  if (!base::CurrentIOThread::Get()->Watch(
          socket_, true, kSbSocketWaiterInterestRead, &socket_watcher_, this)) {
    PLOG(ERROR) << "WatchSocket failed on read";
    Error result = MapLastSocketError(socket_);
    if (result == ERR_IO_PENDING) {
      // Watch(...) might call SbSocketWaiterAdd() which does not guarantee
      // setting system error on failure, but we need to treat this as an
      // error since watching the socket failed.
      result = ERR_FAILED;
    }
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
  return SendToOrWrite(buf, buf_len, nullptr, std::move(callback));
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
  DCHECK(SbSocketIsValid(socket_));
  DCHECK(write_callback_.is_null());
  DCHECK(!callback.is_null());  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int result = InternalSendTo(buf, buf_len, address);
  if (result != ERR_IO_PENDING)
    return result;

  if (!base::CurrentIOThread::Get()->Watch(socket_, true,
                                           kSbSocketWaiterInterestWrite,
                                           &socket_watcher_, this)) {
    DVLOG(1) << "Watch failed on write, error "
             << SbSocketGetLastError(socket_);
    Error result = MapLastSocketError(socket_);
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
  DCHECK(SbSocketIsValid(socket_));
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
  DCHECK(SbSocketIsValid(socket_));
  DCHECK(!is_connected());
  DCHECK(!remote_address_.get());

  int rv = 0;
  if (bind_type_ == DatagramSocket::RANDOM_BIND) {
    // Construct IPAddress of appropriate size (IPv4 or IPv6) of 0s,
    // representing INADDR_ANY or in6addr_any.
    rv = RandomBind(address.GetFamily() == ADDRESS_FAMILY_IPV4
                        ? IPAddress::IPv4AllZeros()
                        : IPAddress::IPv6AllZeros());
  }

  if (rv != OK)
    return rv;

  SbSocketAddress storage;
  if (!address.ToSbSocketAddress(&storage)) {
    return ERR_ADDRESS_INVALID;
  }

  remote_address_.reset(new IPEndPoint(address));

  SbSocketError result = SbSocketConnect(socket_, &storage);
  if (result != kSbSocketOk) {
    return MapLastSocketError(socket_);
  }

  return OK;
}

int UDPSocketStarboard::Bind(const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(SbSocketIsValid(socket_));
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
  DCHECK(SbSocketIsValid(socket_));

  int result = OK;
  if (!SbSocketSetReceiveBufferSize(socket_, size)) {
    result = MapLastSocketError(socket_);
  }
  DCHECK_EQ(result, OK) << "Could not " << __FUNCTION__ << ": "
                        << SbSocketGetLastError(socket_);
  return result;
}

int UDPSocketStarboard::SetSendBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(SbSocketIsValid(socket_));

  int result = OK;
  if (!SbSocketSetSendBufferSize(socket_, size)) {
    result = MapLastSocketError(socket_);
  }
  DCHECK_EQ(result, OK) << "Could not " << __FUNCTION__ << ": "
                        << SbSocketGetLastError(socket_);
  return result;
}

int UDPSocketStarboard::AllowAddressReuse() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());
  DCHECK(SbSocketIsValid(socket_));

  return SbSocketSetReuseAddress(socket_, true) ? OK : ERR_FAILED;
}

int UDPSocketStarboard::SetBroadcast(bool broadcast) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());
  DCHECK(SbSocketIsValid(socket_));

  return SbSocketSetBroadcast(socket_, broadcast) ? OK : ERR_FAILED;
}

int UDPSocketStarboard::AllowAddressSharingForMulticast() {
  DCHECK_NE(socket_, kSbSocketInvalid);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());

  int rv = AllowAddressReuse();
  if (rv != OK)
    return rv;

  return OK;
}

void UDPSocketStarboard::OnSocketReadyToRead(SbSocket /*socket*/) {
  if (!read_callback_.is_null()) {
    if (results_) {
      DidCompleteMultiplePacketRead();
    } else {
      DidCompleteRead();
    }
  }
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
  socket_->StopWatchingSocketForWriting();
  socket_->FlushPending();
}

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
    StopWatchingSocketForReading();
    DoReadCallback(result);
  }
}

void UDPSocketStarboard::DidCompleteMultiplePacketRead() {
  int result = InternalReadMultiplePackets(results_);
  if (result != ERR_IO_PENDING) {
    results_ = nullptr;
    StopWatchingSocketForReading();
    DoReadCallback(result);
  }
}

void UDPSocketStarboard::DidCompleteMultiplePacketRead() {
  int result = InternalReadMultiplePackets(results_);
  if (result != ERR_IO_PENDING) {
    results_ = nullptr;
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
    StopWatchingSocketForWriting();
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
  SbSocketAddress sb_address;
  int bytes_transferred = SbSocketReceiveFrom(socket_, buf->data(), buf_len,
                                              address ? &sb_address : nullptr);
  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    // Passing in NULL address is allowed. This is only to align with other
    // platform's implementation.
    if (address && !address->FromSbSocketAddress(&sb_address)) {
      result = ERR_ADDRESS_INVALID;
    } else if (bytes_transferred >= buf_len) {
      result = ERR_MSG_TOO_BIG;
    }
  } else {
    result = MapLastSocketError(socket_);
  }

  if (result != ERR_IO_PENDING) {
    IPEndPoint log_address;
    if (result < 0 || !address ||
        !log_address.FromSbSocketAddress(&sb_address)) {
      LogRead(result, buf->data(), nullptr);
    } else {
      LogRead(result, buf->data(), &log_address);
    }
  }

  return result;
}

int UDPSocketStarboard::InternalReadMultiplePackets(
    Socket::ReadPacketResults* results) {
  SbSocketReceiveMultiMsgResult* msgresult =
      g_socket_extension->ReceiveMultiMsg(socket_, results->buffer->data());
  CHECK(msgresult);
  results->result = msgresult ? msgresult->result : -1;
  if (results->result < 0) {
    results->result = MapLastSocketError(socket_);
    if (results->result == ERR_IO_PENDING) {
      return results->result;
    }
  }
  SbSocketReceiveMultiMsgPacket* packet = msgresult->packets;
  Socket::ReadPacketResult* out_packet = results->packets;
  CHECK(packet);
  CHECK(out_packet);
  for (int i = 0; i < results->result; ++i, ++packet, ++out_packet) {
    out_packet->buffer = packet->buffer;
    out_packet->result = packet->result;
  }
  return results->result;
}

int UDPSocketStarboard::InternalSendTo(IOBuffer* buf,
                                       int buf_len,
                                       const IPEndPoint* address) {
#if SB_HAS_QUIRK(SENDING_CONNECTED_UDP_SOCKETS)
  // If the socket is already connected (remote_address_ is set),
  // call SbSocketSendTo without destination address as it will use
  // the address from the connection
  if (remote_address_) {
    int result = SbSocketSendTo(socket_, buf->data(), buf_len, nullptr);
    if (result < 0) {
      result = MapLastSocketError(socket_);
    }
    if (result != ERR_IO_PENDING) {
      LogWrite(result, buf->data(), nullptr);
    }
    return result;
  }
#endif

  // Old logic for platforms without SENDING_CONNECTED_UDP_SOCKETS support
  // or for unconnected sockets
  SbSocketAddress sb_address;
  if (!address && !g_socket_extension) {
    // Platforms without the socket extension require a destination address.
    address = remote_address_.get();
    if (!address) {
      int result = ERR_FAILED;
      LogWrite(result, NULL, NULL);
      return result;
    }
  }
  if (address && !address->ToSbSocketAddress(&sb_address)) {
    int result = ERR_FAILED;
    LogWrite(result, nullptr, nullptr);
    return result;
  }

  int result = SbSocketSendTo(socket_, buf->data(), buf_len,
                              address ? &sb_address : nullptr);

  if (result < 0)
    result = MapLastSocketError(socket_);

  if (result != ERR_IO_PENDING) {
    if (!address) {
      LogWrite(result, buf->data(), nullptr);
    } else {
      LogWrite(result, buf->data(), address);
    }
  }

  return result;
}

int UDPSocketStarboard::DoBind(const IPEndPoint& address) {
  SbSocketAddress sb_address;
  if (!address.ToSbSocketAddress(&sb_address)) {
    return ERR_UNEXPECTED;
  }

  SbSocketError rv = SbSocketBind(socket_, &sb_address);
  return rv != kSbSocketOk ? MapSystemError(SbSystemGetLastError()) : OK;
}

int UDPSocketStarboard::RandomBind(const IPAddress& address) {
  return DoBind(IPEndPoint(address, 0));
}

int UDPSocketStarboard::JoinGroup(const IPAddress& group_address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  SbSocketAddress sb_address = {0};
  if (!IPEndPoint(group_address, 0).ToSbSocketAddress(&sb_address)) {
    return ERR_ADDRESS_INVALID;
  }

  if (!SbSocketJoinMulticastGroup(socket_, &sb_address)) {
    LOG(WARNING) << "SbSocketJoinMulticastGroup failed on UDP socket.";
    return MapLastSocketError(socket_);
  }
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

int UDPSocketStarboardSender::Send(const SbSocket& socket,
                                   const char* buf,
                                   size_t len,
                                   SbSocketAddress address) const {
  return SbSocketSendTo(socket, buf, len, &address);
}

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
  SbSocketAddress sb_address;
  int result = remote_address_.get()->ToSbSocketAddress(&sb_address);
  DCHECK(result);
  DidSendBuffers(
      sender_->SendBuffers(socket_, std::move(pending_writes_), sb_address));
}

void UDPSocketStarboard::PostSendBuffers() {
  DVLOG(1) << __func__ << " queue " << pending_writes_.size() << " out of "
           << write_async_outstanding_ << " total";
  SbSocketAddress sb_address;
  DCHECK(remote_address_.get()->ToSbSocketAddress(&sb_address));
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UDPSocketStarboardSender::SendBuffers, sender_, socket_,
                     std::move(pending_writes_), sb_address),
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
    if (!WatchSocketForWriting()) {
      last_async_result_ = MapLastSocketError(socket_);
      DVLOG(1) << "WatchSocket failed on write, error: " << last_async_result_;
      LogWrite(last_async_result_, NULL, NULL);
    } else {
      last_async_result_ = 0;
    }
  } else if (last_async_result_ < 0 || pending_writes_.empty()) {
    DVLOG(2) << __func__ << " WatchSocket stop: result "
             << ErrorToShortString(last_async_result_) << " pending_writes "
             << pending_writes_.size();
    StopWatchingSocketForWriting();
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
  DCHECK(SbSocketIsValid(socket_));

  // Starboard does not supported sending non-fragmented packets yet.
  // All QUIC Streams call this function at initialization, setting sockets to
  // send non-fragmented packets may have a slight performance boost.
  return ERR_NOT_IMPLEMENTED;
}

void UDPSocketStarboard::SetMsgConfirm(bool confirm) {
  NOTIMPLEMENTED();
}

bool UDPSocketStarboard::WatchSocketForWriting() {
  if (write_async_watcher_->watching())
    return true;
  bool result = base::CurrentIOThread::Get()->Watch(
      socket_, true, kSbSocketWaiterInterestWrite, &socket_watcher_, this);
  if (result) {
    write_async_watcher_->set_watching(true);
  }
  return result;
}

void UDPSocketStarboard::StopWatchingSocketForWriting() {
  if (!write_async_watcher_->watching())
    return;
  write_async_watcher_->set_watching(false);
  if (!write_buf_) {
    bool ok = socket_watcher_.UnregisterInterest(kSbSocketWaiterInterestWrite);
    DCHECK(ok);
  }
}

void UDPSocketStarboard::StopWatchingSocketForReading() {
  if (!read_buf_) {
    bool ok = socket_watcher_.UnregisterInterest(kSbSocketWaiterInterestRead);
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
