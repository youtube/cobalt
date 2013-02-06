// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/udp/udp_socket_win.h"

#include <mstcpip.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/winsock_init.h"
#include "net/base/winsock_util.h"
#include "net/udp/udp_net_log_parameters.h"

namespace {

static const int kBindRetries = 10;
static const int kPortStart = 1024;
static const int kPortEnd = 65535;

}  // namespace net

namespace net {

// This class encapsulates all the state that has to be preserved as long as
// there is a network IO operation in progress. If the owner UDPSocketWin
// is destroyed while an operation is in progress, the Core is detached and it
// lives until the operation completes and the OS doesn't reference any resource
// declared on this class anymore.
class UDPSocketWin::Core : public base::RefCounted<Core> {
 public:
  explicit Core(UDPSocketWin* socket);

  // Start watching for the end of a read or write operation.
  void WatchForRead();
  void WatchForWrite();

  // The UDPSocketWin is going away.
  void Detach() { socket_ = NULL; }

  // The separate OVERLAPPED variables for asynchronous operation.
  OVERLAPPED read_overlapped_;
  OVERLAPPED write_overlapped_;

  // The buffers used in Read() and Write().
  scoped_refptr<IOBuffer> read_iobuffer_;
  scoped_refptr<IOBuffer> write_iobuffer_;

  // The address storage passed to WSARecvFrom().
  SockaddrStorage recv_addr_storage_;

 private:
  friend class base::RefCounted<Core>;

  class ReadDelegate : public base::win::ObjectWatcher::Delegate {
   public:
    explicit ReadDelegate(Core* core) : core_(core) {}
    virtual ~ReadDelegate() {}

    // base::ObjectWatcher::Delegate methods:
    virtual void OnObjectSignaled(HANDLE object);

   private:
    Core* const core_;
  };

  class WriteDelegate : public base::win::ObjectWatcher::Delegate {
   public:
    explicit WriteDelegate(Core* core) : core_(core) {}
    virtual ~WriteDelegate() {}

    // base::ObjectWatcher::Delegate methods:
    virtual void OnObjectSignaled(HANDLE object);

   private:
    Core* const core_;
  };

  ~Core();

  // The socket that created this object.
  UDPSocketWin* socket_;

  // |reader_| handles the signals from |read_watcher_|.
  ReadDelegate reader_;
  // |writer_| handles the signals from |write_watcher_|.
  WriteDelegate writer_;

  // |read_watcher_| watches for events from Read().
  base::win::ObjectWatcher read_watcher_;
  // |write_watcher_| watches for events from Write();
  base::win::ObjectWatcher write_watcher_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

UDPSocketWin::Core::Core(UDPSocketWin* socket)
    : socket_(socket),
      ALLOW_THIS_IN_INITIALIZER_LIST(reader_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(writer_(this)) {
  memset(&read_overlapped_, 0, sizeof(read_overlapped_));
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));

  read_overlapped_.hEvent = WSACreateEvent();
  write_overlapped_.hEvent = WSACreateEvent();
}

UDPSocketWin::Core::~Core() {
  // Make sure the message loop is not watching this object anymore.
  read_watcher_.StopWatching();
  write_watcher_.StopWatching();

  WSACloseEvent(read_overlapped_.hEvent);
  memset(&read_overlapped_, 0xaf, sizeof(read_overlapped_));
  WSACloseEvent(write_overlapped_.hEvent);
  memset(&write_overlapped_, 0xaf, sizeof(write_overlapped_));
}

void UDPSocketWin::Core::WatchForRead() {
  // We grab an extra reference because there is an IO operation in progress.
  // Balanced in ReadDelegate::OnObjectSignaled().
  AddRef();
  read_watcher_.StartWatching(read_overlapped_.hEvent, &reader_);
}

void UDPSocketWin::Core::WatchForWrite() {
  // We grab an extra reference because there is an IO operation in progress.
  // Balanced in WriteDelegate::OnObjectSignaled().
  AddRef();
  write_watcher_.StartWatching(write_overlapped_.hEvent, &writer_);
}

void UDPSocketWin::Core::ReadDelegate::OnObjectSignaled(HANDLE object) {
  DCHECK_EQ(object, core_->read_overlapped_.hEvent);
  if (core_->socket_)
    core_->socket_->DidCompleteRead();

  core_->Release();
}

void UDPSocketWin::Core::WriteDelegate::OnObjectSignaled(HANDLE object) {
  DCHECK_EQ(object, core_->write_overlapped_.hEvent);
  if (core_->socket_)
    core_->socket_->DidCompleteWrite();

  core_->Release();
}

//-----------------------------------------------------------------------------

UDPSocketWin::UDPSocketWin(DatagramSocket::BindType bind_type,
                           const RandIntCallback& rand_int_cb,
                           net::NetLog* net_log,
                           const net::NetLog::Source& source)
    : socket_(INVALID_SOCKET),
      socket_options_(0),
      bind_type_(bind_type),
      rand_int_cb_(rand_int_cb),
      recv_from_address_(NULL),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_UDP_SOCKET)) {
  EnsureWinsockInit();
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE,
                      source.ToEventParametersCallback());
  if (bind_type == DatagramSocket::RANDOM_BIND)
    DCHECK(!rand_int_cb.is_null());
}

UDPSocketWin::~UDPSocketWin() {
  Close();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE);
}

void UDPSocketWin::Close() {
  DCHECK(CalledOnValidThread());

  if (!is_connected())
    return;

  // Zero out any pending read/write callback state.
  read_callback_.Reset();
  recv_from_address_ = NULL;
  write_callback_.Reset();

  base::TimeTicks start_time = base::TimeTicks::Now();
  closesocket(socket_);
  UMA_HISTOGRAM_TIMES("Net.UDPSocketWinClose",
                      base::TimeTicks::Now() - start_time);
  socket_ = INVALID_SOCKET;

  core_->Detach();
  core_ = NULL;
}

int UDPSocketWin::GetPeerAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  // TODO(szym): Simplify. http://crbug.com/126152
  if (!remote_address_.get()) {
    SockaddrStorage storage;
    if (getpeername(socket_, storage.addr, &storage.addr_len))
      return MapSystemError(WSAGetLastError());
    scoped_ptr<IPEndPoint> address(new IPEndPoint());
    if (!address->FromSockAddr(storage.addr, storage.addr_len))
      return ERR_FAILED;
    remote_address_.reset(address.release());
  }

  *address = *remote_address_;
  return OK;
}

int UDPSocketWin::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  // TODO(szym): Simplify. http://crbug.com/126152
  if (!local_address_.get()) {
    SockaddrStorage storage;
    if (getsockname(socket_, storage.addr, &storage.addr_len))
      return MapSystemError(WSAGetLastError());
    scoped_ptr<IPEndPoint> address(new IPEndPoint());
    if (!address->FromSockAddr(storage.addr, storage.addr_len))
      return ERR_FAILED;
    local_address_.reset(address.release());
  }

  *address = *local_address_;
  return OK;
}

int UDPSocketWin::Read(IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback) {
  return RecvFrom(buf, buf_len, NULL, callback);
}

int UDPSocketWin::RecvFrom(IOBuffer* buf,
                           int buf_len,
                           IPEndPoint* address,
                           const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(INVALID_SOCKET, socket_);
  DCHECK(read_callback_.is_null());
  DCHECK(!recv_from_address_);
  DCHECK(!callback.is_null());  // Synchronous operation not supported.
  DCHECK_GT(buf_len, 0);

  int nread = InternalRecvFrom(buf, buf_len, address);
  if (nread != ERR_IO_PENDING)
    return nread;

  read_callback_ = callback;
  recv_from_address_ = address;
  return ERR_IO_PENDING;
}

int UDPSocketWin::Write(IOBuffer* buf,
                        int buf_len,
                        const CompletionCallback& callback) {
  return SendToOrWrite(buf, buf_len, NULL, callback);
}

int UDPSocketWin::SendTo(IOBuffer* buf,
                         int buf_len,
                         const IPEndPoint& address,
                         const CompletionCallback& callback) {
  return SendToOrWrite(buf, buf_len, &address, callback);
}

int UDPSocketWin::SendToOrWrite(IOBuffer* buf,
                                int buf_len,
                                const IPEndPoint* address,
                                const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(INVALID_SOCKET, socket_);
  DCHECK(write_callback_.is_null());
  DCHECK(!callback.is_null());  // Synchronous operation not supported.
  DCHECK_GT(buf_len, 0);
  DCHECK(!send_to_address_.get());

  int nwrite = InternalSendTo(buf, buf_len, address);
  if (nwrite != ERR_IO_PENDING)
    return nwrite;

  if (address)
    send_to_address_.reset(new IPEndPoint(*address));
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

int UDPSocketWin::Connect(const IPEndPoint& address) {
  net_log_.BeginEvent(NetLog::TYPE_UDP_CONNECT,
                      CreateNetLogUDPConnectCallback(&address));
  int rv = InternalConnect(address);
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_UDP_CONNECT, rv);
  return rv;
}

int UDPSocketWin::InternalConnect(const IPEndPoint& address) {
  DCHECK(!is_connected());
  DCHECK(!remote_address_.get());
  int rv = CreateSocket(address);
  if (rv < 0)
    return rv;

  if (bind_type_ == DatagramSocket::RANDOM_BIND)
    rv = RandomBind(address);
  // else connect() does the DatagramSocket::DEFAULT_BIND

  if (rv < 0)
    return rv;

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_FAILED;

  rv = connect(socket_, storage.addr, storage.addr_len);
  if (rv < 0)
    return MapSystemError(WSAGetLastError());

  remote_address_.reset(new IPEndPoint(address));
  return rv;
}

int UDPSocketWin::Bind(const IPEndPoint& address) {
  DCHECK(!is_connected());
  int rv = CreateSocket(address);
  if (rv < 0)
    return rv;
  rv = SetSocketOptions();
  if (rv < 0)
    return rv;
  rv = DoBind(address);
  if (rv < 0)
    return rv;
  local_address_.reset();
  return rv;
}

int UDPSocketWin::CreateSocket(const IPEndPoint& address) {
  socket_ = WSASocket(address.GetSockAddrFamily(), SOCK_DGRAM, IPPROTO_UDP,
                      NULL, 0, WSA_FLAG_OVERLAPPED);
  if (socket_ == INVALID_SOCKET)
    return MapSystemError(WSAGetLastError());
  core_ = new Core(this);
  return OK;
}

bool UDPSocketWin::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  int rv = setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size));
  DCHECK(!rv) << "Could not set socket receive buffer size: " << errno;
  return rv == 0;
}

bool UDPSocketWin::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  int rv = setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size));
  DCHECK(!rv) << "Could not set socket send buffer size: " << errno;
  return rv == 0;
}

void UDPSocketWin::AllowAddressReuse() {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_connected());

  socket_options_ |= SOCKET_OPTION_REUSE_ADDRESS;
}

void UDPSocketWin::AllowBroadcast() {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_connected());

  socket_options_ |= SOCKET_OPTION_BROADCAST;
}

void UDPSocketWin::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!read_callback_.is_null());

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback c = read_callback_;
  read_callback_.Reset();
  c.Run(rv);
}

void UDPSocketWin::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!write_callback_.is_null());

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback c = write_callback_;
  write_callback_.Reset();
  c.Run(rv);
}

void UDPSocketWin::DidCompleteRead() {
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &core_->read_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(core_->read_overlapped_.hEvent);
  int result = ok ? num_bytes : MapSystemError(WSAGetLastError());
  // Convert address.
  if (recv_from_address_ && result >= 0) {
    if (!ReceiveAddressToIPEndpoint(recv_from_address_))
      result = ERR_FAILED;
  }
  LogRead(result, core_->read_iobuffer_->data());
  core_->read_iobuffer_ = NULL;
  recv_from_address_ = NULL;
  DoReadCallback(result);
}

void UDPSocketWin::LogRead(int result, const char* bytes) const {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLog::TYPE_UDP_RECEIVE_ERROR, result);
    return;
  }

  if (net_log_.IsLoggingAllEvents()) {
    // Get address for logging, if |address| is NULL.
    IPEndPoint address;
    bool is_address_valid = ReceiveAddressToIPEndpoint(&address);
    net_log_.AddEvent(
        NetLog::TYPE_UDP_BYTES_RECEIVED,
        CreateNetLogUDPDataTranferCallback(
            result, bytes,
            is_address_valid ? &address : NULL));
  }

  base::StatsCounter read_bytes("udp.read_bytes");
  read_bytes.Add(result);
}

void UDPSocketWin::DidCompleteWrite() {
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &core_->write_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(core_->write_overlapped_.hEvent);
  int result = ok ? num_bytes : MapSystemError(WSAGetLastError());
  LogWrite(result, core_->write_iobuffer_->data(), send_to_address_.get());

  send_to_address_.reset();
  core_->write_iobuffer_ = NULL;
  DoWriteCallback(result);
}

void UDPSocketWin::LogWrite(int result,
                            const char* bytes,
                            const IPEndPoint* address) const {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLog::TYPE_UDP_SEND_ERROR, result);
    return;
  }

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        NetLog::TYPE_UDP_BYTES_SENT,
        CreateNetLogUDPDataTranferCallback(result, bytes, address));
  }

  base::StatsCounter write_bytes("udp.write_bytes");
  write_bytes.Add(result);
}

int UDPSocketWin::InternalRecvFrom(IOBuffer* buf, int buf_len,
                                   IPEndPoint* address) {
  DCHECK(!core_->read_iobuffer_);
  SockaddrStorage& storage = core_->recv_addr_storage_;
  storage.addr_len = sizeof(storage.addr_storage);

  WSABUF read_buffer;
  read_buffer.buf = buf->data();
  read_buffer.len = buf_len;

  DWORD flags = 0;
  DWORD num;
  CHECK_NE(INVALID_SOCKET, socket_);
  AssertEventNotSignaled(core_->read_overlapped_.hEvent);
  int rv = WSARecvFrom(socket_, &read_buffer, 1, &num, &flags, storage.addr,
                       &storage.addr_len, &core_->read_overlapped_, NULL);
  if (rv == 0) {
    if (ResetEventIfSignaled(core_->read_overlapped_.hEvent)) {
      int result = num;
      // Convert address.
      if (address && result >= 0) {
        if (!ReceiveAddressToIPEndpoint(address))
          result = ERR_FAILED;
      }
      LogRead(result, buf->data());
      return result;
    }
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSA_IO_PENDING) {
      int result = MapSystemError(os_error);
      LogRead(result, NULL);
      return result;
    }
  }
  core_->WatchForRead();
  core_->read_iobuffer_ = buf;
  return ERR_IO_PENDING;
}

int UDPSocketWin::InternalSendTo(IOBuffer* buf, int buf_len,
                                 const IPEndPoint* address) {
  DCHECK(!core_->write_iobuffer_);
  SockaddrStorage storage;
  struct sockaddr* addr = storage.addr;
  // Convert address.
  if (!address) {
    addr = NULL;
    storage.addr_len = 0;
  } else {
    if (!address->ToSockAddr(addr, &storage.addr_len)) {
      int result = ERR_FAILED;
      LogWrite(result, NULL, NULL);
      return result;
    }
  }

  WSABUF write_buffer;
  write_buffer.buf = buf->data();
  write_buffer.len = buf_len;

  DWORD flags = 0;
  DWORD num;
  AssertEventNotSignaled(core_->write_overlapped_.hEvent);
  int rv = WSASendTo(socket_, &write_buffer, 1, &num, flags,
                     addr, storage.addr_len, &core_->write_overlapped_, NULL);
  if (rv == 0) {
    if (ResetEventIfSignaled(core_->write_overlapped_.hEvent)) {
      int result = num;
      LogWrite(result, buf->data(), address);
      return result;
    }
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSA_IO_PENDING) {
      int result = MapSystemError(os_error);
      LogWrite(result, NULL, NULL);
      return result;
    }
  }

  core_->WatchForWrite();
  core_->write_iobuffer_ = buf;
  return ERR_IO_PENDING;
}

int UDPSocketWin::SetSocketOptions() {
  BOOL true_value = 1;
  if (socket_options_ & SOCKET_OPTION_REUSE_ADDRESS) {
    int rv = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                        reinterpret_cast<const char*>(&true_value),
                        sizeof(true_value));
    if (rv < 0)
      return MapSystemError(errno);
  }
  if (socket_options_ & SOCKET_OPTION_BROADCAST) {
    int rv = setsockopt(socket_, SOL_SOCKET, SO_BROADCAST,
                        reinterpret_cast<const char*>(&true_value),
                        sizeof(true_value));
    if (rv < 0)
      return MapSystemError(errno);
  }
  return OK;
}

int UDPSocketWin::DoBind(const IPEndPoint& address) {
  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_UNEXPECTED;
  int rv = bind(socket_, storage.addr, storage.addr_len);
  return rv < 0 ? MapSystemError(WSAGetLastError()) : rv;
}

int UDPSocketWin::RandomBind(const IPEndPoint& address) {
  DCHECK(bind_type_ == DatagramSocket::RANDOM_BIND && !rand_int_cb_.is_null());

  // Construct IPAddressNumber of appropriate size (IPv4 or IPv6) of 0s.
  IPAddressNumber ip(address.address().size());

  for (int i = 0; i < kBindRetries; ++i) {
    int rv = DoBind(IPEndPoint(ip, rand_int_cb_.Run(kPortStart, kPortEnd)));
    if (rv == OK || rv != ERR_ADDRESS_IN_USE)
      return rv;
  }
  return DoBind(IPEndPoint(ip, 0));
}

bool UDPSocketWin::ReceiveAddressToIPEndpoint(IPEndPoint* address) const {
  SockaddrStorage& storage = core_->recv_addr_storage_;
  return address->FromSockAddr(storage.addr, storage.addr_len);
}

}  // namespace net
