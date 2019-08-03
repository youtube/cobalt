// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/buffered_write_stream_socket.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

void AppendBuffer(GrowableIOBuffer* dst, IOBuffer* src, int src_len) {
  int old_capacity = dst->capacity();
  dst->SetCapacity(old_capacity + src_len);
  memcpy(dst->StartOfBuffer() + old_capacity, src->data(), src_len);
}

}  // anonymous namespace

BufferedWriteStreamSocket::BufferedWriteStreamSocket(
    StreamSocket* socket_to_wrap)
    : wrapped_socket_(socket_to_wrap),
      io_buffer_(new GrowableIOBuffer()),
      backup_buffer_(new GrowableIOBuffer()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      callback_pending_(false),
      wrapped_write_in_progress_(false),
      error_(0) {
}

BufferedWriteStreamSocket::~BufferedWriteStreamSocket() {
}

int BufferedWriteStreamSocket::Read(IOBuffer* buf, int buf_len,
                                    const CompletionCallback& callback) {
  return wrapped_socket_->Read(buf, buf_len, callback);
}

int BufferedWriteStreamSocket::Write(IOBuffer* buf, int buf_len,
                                     const CompletionCallback& callback) {
  if (error_) {
    return error_;
  }
  GrowableIOBuffer* idle_buffer =
      wrapped_write_in_progress_ ? backup_buffer_.get() : io_buffer_.get();
  AppendBuffer(idle_buffer, buf, buf_len);
  if (!callback_pending_) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&BufferedWriteStreamSocket::DoDelayedWrite,
                   weak_factory_.GetWeakPtr()));
    callback_pending_ = true;
  }
  return buf_len;
}

bool BufferedWriteStreamSocket::SetReceiveBufferSize(int32 size) {
  return wrapped_socket_->SetReceiveBufferSize(size);
}

bool BufferedWriteStreamSocket::SetSendBufferSize(int32 size) {
  return wrapped_socket_->SetSendBufferSize(size);
}

int BufferedWriteStreamSocket::Connect(const CompletionCallback& callback) {
  return wrapped_socket_->Connect(callback);
}

void BufferedWriteStreamSocket::Disconnect() {
  wrapped_socket_->Disconnect();
}

bool BufferedWriteStreamSocket::IsConnected() const {
  return wrapped_socket_->IsConnected();
}

bool BufferedWriteStreamSocket::IsConnectedAndIdle() const {
  return wrapped_socket_->IsConnectedAndIdle();
}

int BufferedWriteStreamSocket::GetPeerAddress(IPEndPoint* address) const {
  return wrapped_socket_->GetPeerAddress(address);
}

int BufferedWriteStreamSocket::GetLocalAddress(IPEndPoint* address) const {
  return wrapped_socket_->GetLocalAddress(address);
}

const BoundNetLog& BufferedWriteStreamSocket::NetLog() const {
  return wrapped_socket_->NetLog();
}

void BufferedWriteStreamSocket::SetSubresourceSpeculation() {
  wrapped_socket_->SetSubresourceSpeculation();
}

void BufferedWriteStreamSocket::SetOmniboxSpeculation() {
  wrapped_socket_->SetOmniboxSpeculation();
}

bool BufferedWriteStreamSocket::WasEverUsed() const {
  return wrapped_socket_->WasEverUsed();
}

bool BufferedWriteStreamSocket::UsingTCPFastOpen() const {
  return wrapped_socket_->UsingTCPFastOpen();
}

int64 BufferedWriteStreamSocket::NumBytesRead() const {
  return wrapped_socket_->NumBytesRead();
}

base::TimeDelta BufferedWriteStreamSocket::GetConnectTimeMicros() const {
  return wrapped_socket_->GetConnectTimeMicros();
}

bool BufferedWriteStreamSocket::WasNpnNegotiated() const {
  return wrapped_socket_->WasNpnNegotiated();
}

NextProto BufferedWriteStreamSocket::GetNegotiatedProtocol() const {
  return wrapped_socket_->GetNegotiatedProtocol();
}

bool BufferedWriteStreamSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return wrapped_socket_->GetSSLInfo(ssl_info);
}

void BufferedWriteStreamSocket::DoDelayedWrite() {
  int result = wrapped_socket_->Write(
      io_buffer_, io_buffer_->RemainingCapacity(),
      base::Bind(&BufferedWriteStreamSocket::OnIOComplete,
                 base::Unretained(this)));
  if (result == ERR_IO_PENDING) {
    callback_pending_ = true;
    wrapped_write_in_progress_ = true;
  } else {
    OnIOComplete(result);
  }
}

void BufferedWriteStreamSocket::OnIOComplete(int result) {
  callback_pending_ = false;
  wrapped_write_in_progress_ = false;
  if (backup_buffer_->RemainingCapacity()) {
    AppendBuffer(io_buffer_.get(), backup_buffer_.get(),
                 backup_buffer_->RemainingCapacity());
    backup_buffer_->SetCapacity(0);
  }
  if (result < 0) {
    error_ = result;
    io_buffer_->SetCapacity(0);
  } else {
    io_buffer_->set_offset(io_buffer_->offset() + result);
    if (io_buffer_->RemainingCapacity()) {
      DoDelayedWrite();
    } else {
      io_buffer_->SetCapacity(0);
    }
  }
}

}  // namespace net
