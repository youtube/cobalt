// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/udp/udp_socket_win.h"

#include <mstcpip.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/memory/memory_debug.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/winsock_init.h"
#include "net/base/winsock_util.h"

namespace net {

void UDPSocketWin::ReadDelegate::OnObjectSignaled(HANDLE object) {
  DCHECK_EQ(object, socket_->read_overlapped_.hEvent);
  socket_->DidCompleteRead();
}

void UDPSocketWin::WriteDelegate::OnObjectSignaled(HANDLE object) {
  DCHECK_EQ(object, socket_->write_overlapped_.hEvent);
  socket_->DidCompleteWrite();
}

UDPSocketWin::UDPSocketWin(net::NetLog* net_log,
                           const net::NetLog::Source& source)
    : socket_(INVALID_SOCKET),
      ALLOW_THIS_IN_INITIALIZER_LIST(read_delegate_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(write_delegate_(this)),
      recv_from_address_(NULL),
      read_callback_(NULL),
      write_callback_(NULL),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  EnsureWinsockInit();
  scoped_refptr<NetLog::EventParameters> params;
  if (source.is_valid())
    params = new NetLogSourceParameter("source_dependency", source);
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE, params);
  memset(&read_overlapped_, 0, sizeof(read_overlapped_));
  read_overlapped_.hEvent = WSACreateEvent();
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));
  write_overlapped_.hEvent = WSACreateEvent();
}

UDPSocketWin::~UDPSocketWin() {
  Close();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE, NULL);
}

void UDPSocketWin::Close() {
  DCHECK(CalledOnValidThread());

  if (!is_connected())
    return;

  // Zero out any pending read/write callback state.
  read_callback_ = NULL;
  recv_from_address_ = NULL;
  write_callback_ = NULL;

  read_watcher_.StopWatching();
  write_watcher_.StopWatching();

  closesocket(socket_);
  socket_ = INVALID_SOCKET;
}

int UDPSocketWin::GetPeerAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  if (!remote_address_.get()) {
    struct sockaddr_storage addr_storage;
    int addr_len = sizeof(addr_storage);
    struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
    if (getpeername(socket_, addr, &addr_len))
      return MapSystemError(WSAGetLastError());
    scoped_ptr<IPEndPoint> address(new IPEndPoint());
    if (!address->FromSockAddr(addr, addr_len))
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

  if (!local_address_.get()) {
    struct sockaddr_storage addr_storage;
    socklen_t addr_len = sizeof(addr_storage);
    struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
    if (getsockname(socket_, addr, &addr_len))
      return MapSystemError(WSAGetLastError());
    scoped_ptr<IPEndPoint> address(new IPEndPoint());
    if (!address->FromSockAddr(addr, addr_len))
      return ERR_FAILED;
    local_address_.reset(address.release());
  }

  *address = *local_address_;
  return OK;
}

int UDPSocketWin::Read(IOBuffer* buf,
                       int buf_len,
                       CompletionCallback* callback) {
  return RecvFrom(buf, buf_len, NULL, callback);
}

int UDPSocketWin::RecvFrom(IOBuffer* buf,
                           int buf_len,
                           IPEndPoint* address,
                           CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(INVALID_SOCKET, socket_);
  DCHECK(!read_callback_);
  DCHECK(!recv_from_address_);
  DCHECK(callback);  // Synchronous operation not supported.
  DCHECK_GT(buf_len, 0);

  int nread = InternalRecvFrom(buf, buf_len, address);
  if (nread != ERR_IO_PENDING)
    return nread;

  read_iobuffer_ = buf;
  read_callback_ = callback;
  recv_from_address_ = address;
  return ERR_IO_PENDING;
}

int UDPSocketWin::Write(IOBuffer* buf,
                        int buf_len,
                        CompletionCallback* callback) {
  return SendToOrWrite(buf, buf_len, NULL, callback);
}

int UDPSocketWin::SendTo(IOBuffer* buf,
                         int buf_len,
                         const IPEndPoint& address,
                         CompletionCallback* callback) {
  return SendToOrWrite(buf, buf_len, &address, callback);
}

int UDPSocketWin::SendToOrWrite(IOBuffer* buf,
                                int buf_len,
                                const IPEndPoint* address,
                                CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(INVALID_SOCKET, socket_);
  DCHECK(!write_callback_);
  DCHECK(callback);  // Synchronous operation not supported.
  DCHECK_GT(buf_len, 0);

  int nwrite = InternalSendTo(buf, buf_len, address);
  if (nwrite != ERR_IO_PENDING)
    return nwrite;

  write_iobuffer_ = buf;
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

int UDPSocketWin::Connect(const IPEndPoint& address) {
  DCHECK(!is_connected());
  DCHECK(!remote_address_.get());
  int rv = CreateSocket(address);
  if (rv < 0)
    return rv;

  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
  if (!address.ToSockAddr(addr, &addr_len))
    return ERR_FAILED;

  rv = connect(socket_, addr, addr_len);
  if (rv < 0)
    return MapSystemError(WSAGetLastError());

  remote_address_.reset(new IPEndPoint(address));
  return rv;
}

int UDPSocketWin::Bind(const IPEndPoint& address) {
  DCHECK(!is_connected());
  DCHECK(!local_address_.get());
  int rv = CreateSocket(address);
  if (rv < 0)
    return rv;

  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
  if (!address.ToSockAddr(addr, &addr_len))
    return ERR_FAILED;

  rv = bind(socket_, addr, addr_len);
  if (rv < 0)
    return MapSystemError(WSAGetLastError());

  local_address_.reset();
  return rv;
}

int UDPSocketWin::CreateSocket(const IPEndPoint& address) {
  socket_ = WSASocket(address.GetFamily(), SOCK_DGRAM, IPPROTO_UDP, NULL, 0,
                      WSA_FLAG_OVERLAPPED);
  if (socket_ == INVALID_SOCKET)
    return MapSystemError(WSAGetLastError());
  return OK;
}

void UDPSocketWin::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(read_callback_);

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback* c = read_callback_;
  read_callback_ = NULL;
  c->Run(rv);
}

void UDPSocketWin::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(write_callback_);

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback* c = write_callback_;
  write_callback_ = NULL;
  c->Run(rv);
}

void UDPSocketWin::DidCompleteRead() {
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &read_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(read_overlapped_.hEvent);
  int result = ok ? num_bytes : MapSystemError(WSAGetLastError());
  if (ok) {
    if (!ProcessSuccessfulRead(num_bytes, recv_from_address_))
      result = ERR_FAILED;
  }
  read_iobuffer_ = NULL;
  recv_from_address_ = NULL;
  DoReadCallback(result);
}

bool UDPSocketWin::ProcessSuccessfulRead(int num_bytes, IPEndPoint* address) {
  base::StatsCounter read_bytes("udp.read_bytes");
  read_bytes.Add(num_bytes);

  // Convert address.
  if (address) {
    struct sockaddr* addr =
        reinterpret_cast<struct sockaddr*>(&recv_addr_storage_);
    if (!address->FromSockAddr(addr, recv_addr_len_))
      return false;
  }

  return true;
}

void UDPSocketWin::DidCompleteWrite() {
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &write_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(write_overlapped_.hEvent);
  int result = ok ? num_bytes : MapSystemError(WSAGetLastError());
  if (ok)
    ProcessSuccessfulWrite(num_bytes);
  write_iobuffer_ = NULL;
  DoWriteCallback(result);
}

void UDPSocketWin::ProcessSuccessfulWrite(int num_bytes) {
  base::StatsCounter write_bytes("udp.write_bytes");
  write_bytes.Add(num_bytes);
}

int UDPSocketWin::InternalRecvFrom(IOBuffer* buf, int buf_len,
                                   IPEndPoint* address) {
  recv_addr_len_ = sizeof(recv_addr_storage_);
  struct sockaddr* addr =
      reinterpret_cast<struct sockaddr*>(&recv_addr_storage_);

  WSABUF read_buffer;
  read_buffer.buf = buf->data();
  read_buffer.len = buf_len;

  DWORD flags = 0;
  DWORD num;
  AssertEventNotSignaled(read_overlapped_.hEvent);
  int rv = WSARecvFrom(socket_, &read_buffer, 1, &num, &flags, addr,
                       &recv_addr_len_, &read_overlapped_, NULL);
  if (rv == 0) {
    if (ResetEventIfSignaled(read_overlapped_.hEvent)) {
      // Because of how WSARecv fills memory when used asynchronously, Purify
      // isn't able to detect that it's been initialized, so it scans for 0xcd
      // in the buffer and reports UMRs (uninitialized memory reads) for those
      // individual bytes. We override that in PURIFY builds to avoid the
      // false error reports.
      // See bug 5297.
      base::MemoryDebug::MarkAsInitialized(read_buffer.buf, num);
      if (!ProcessSuccessfulRead(num, address))
        return ERR_FAILED;
      return static_cast<int>(num);
    }
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSA_IO_PENDING)
      return MapSystemError(os_error);
  }
  read_watcher_.StartWatching(read_overlapped_.hEvent, &read_delegate_);
  return ERR_IO_PENDING;
}

int UDPSocketWin::InternalSendTo(IOBuffer* buf, int buf_len,
                                 const IPEndPoint* address) {
  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);

  // Convert address.
  if (!address) {
    addr = NULL;
    addr_len = 0;
  } else {
    if (!address->ToSockAddr(addr, &addr_len))
      return ERR_FAILED;
  }

  WSABUF write_buffer;
  write_buffer.buf = buf->data();
  write_buffer.len = buf_len;

  DWORD flags = 0;
  DWORD num;
  AssertEventNotSignaled(write_overlapped_.hEvent);
  int rv = WSASendTo(socket_, &write_buffer, 1, &num, flags,
                     addr, addr_len, &write_overlapped_, NULL);
  if (rv == 0) {
    if (ResetEventIfSignaled(write_overlapped_.hEvent)) {
      ProcessSuccessfulWrite(num);
      return static_cast<int>(num);
    }
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSA_IO_PENDING)
      return MapSystemError(os_error);
  }

  write_watcher_.StartWatching(write_overlapped_.hEvent, &write_delegate_);
  return ERR_IO_PENDING;
}

}  // namespace net
