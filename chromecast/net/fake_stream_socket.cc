// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/fake_stream_socket.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/next_proto.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace chromecast {

// Buffer used for communication between two FakeStreamSockets.
class SocketBuffer {
 public:
  SocketBuffer() : pending_read_data_(nullptr), pending_read_len_(0) {}

  SocketBuffer(const SocketBuffer&) = delete;
  SocketBuffer& operator=(const SocketBuffer&) = delete;

  ~SocketBuffer() {}

  // Reads |len| bytes from the buffer and writes it to |data|. Returns the
  // number of bytes written to |data| if the read is synchronous, or
  // ERR_IO_PENDING if the read is asynchronous. If the read is asynchronous,
  // |callback| is called with the number of bytes written to |data| once the
  // data has been written.
  int Read(char* data, size_t len, net::CompletionOnceCallback callback) {
    DCHECK(data);
    DCHECK_GT(len, 0u);
    DCHECK(callback);
    if (data_.empty()) {
      if (eos_) {
        return 0;
      }
      pending_read_data_ = data;
      pending_read_len_ = len;
      pending_read_callback_ = std::move(callback);
      return net::ERR_IO_PENDING;
    }
    return ReadInternal(data, len);
  }

  // Writes |len| bytes from |data| to the buffer. The write is always completed
  // synchronously and all bytes are guaranteed to be written.
  void Write(const char* data, size_t len) {
    DCHECK(data);
    DCHECK_GT(len, 0u);
    data_.insert(data_.end(), data, data + len);
    if (!pending_read_callback_.is_null()) {
      int result = ReadInternal(pending_read_data_, pending_read_len_);
      pending_read_data_ = nullptr;
      pending_read_len_ = 0;
      PostReadCallback(std::move(pending_read_callback_), result);
    }
  }

  // Called when the remote end of the fake connection disconnects.
  void ReceiveEOS() {
    eos_ = true;
    if (pending_read_callback_ && data_.empty()) {
      PostReadCallback(std::move(pending_read_callback_), 0);
    }
  }

 private:
  int ReadInternal(char* data, size_t len) {
    DCHECK(data);
    DCHECK_GT(len, 0u);
    len = std::min(len, data_.size());
    std::memcpy(data, data_.data(), len);
    data_.erase(data_.begin(), data_.begin() + len);
    return len;
  }

  void PostReadCallback(net::CompletionOnceCallback callback, int result) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SocketBuffer::CallReadCallback,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback), result));
  }

  // Need a member function to asynchronously call the read callback, so we
  // can use weak ptr.
  void CallReadCallback(net::CompletionOnceCallback callback, int result) {
    std::move(callback).Run(result);
  }

  std::vector<char> data_;
  char* pending_read_data_;
  size_t pending_read_len_;
  net::CompletionOnceCallback pending_read_callback_;
  bool eos_ = false;
  base::WeakPtrFactory<SocketBuffer> weak_factory_{this};
};

FakeStreamSocket::FakeStreamSocket() : FakeStreamSocket(net::IPEndPoint()) {}

FakeStreamSocket::FakeStreamSocket(const net::IPEndPoint& local_address)
    : local_address_(local_address),
      buffer_(std::make_unique<SocketBuffer>()),
      peer_(nullptr) {}

FakeStreamSocket::~FakeStreamSocket() {
  if (peer_) {
    peer_->RemoteDisconnected();
  }
}

void FakeStreamSocket::SetPeer(FakeStreamSocket* peer) {
  DCHECK(peer);
  peer_ = peer;
}

void FakeStreamSocket::RemoteDisconnected() {
  peer_ = nullptr;
  buffer_->ReceiveEOS();
}

void FakeStreamSocket::SetBadSenderMode(bool bad_sender) {
  bad_sender_mode_ = bad_sender;
}

int FakeStreamSocket::Read(net::IOBuffer* buf,
                           int buf_len,
                           net::CompletionOnceCallback callback) {
  DCHECK(buf);
  return buffer_->Read(buf->data(), buf_len, std::move(callback));
}

int FakeStreamSocket::Write(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback /* callback */,
    const net::NetworkTrafficAnnotationTag& /*traffic_annotation*/) {
  DCHECK(buf);
  if (!peer_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  int amount_to_send = buf_len;
  if (bad_sender_mode_) {
    amount_to_send = std::min(buf_len, buf_len / 2 + 1);
  }
  peer_->buffer_->Write(buf->data(), amount_to_send);
  return amount_to_send;
}

int FakeStreamSocket::SetReceiveBufferSize(int32_t /* size */) {
  return net::OK;
}

int FakeStreamSocket::SetSendBufferSize(int32_t /* size */) {
  return net::OK;
}

int FakeStreamSocket::Connect(net::CompletionOnceCallback /* callback */) {
  return net::OK;
}

void FakeStreamSocket::Disconnect() {}

bool FakeStreamSocket::IsConnected() const {
  return true;
}

bool FakeStreamSocket::IsConnectedAndIdle() const {
  return false;
}

int FakeStreamSocket::GetPeerAddress(net::IPEndPoint* address) const {
  DCHECK(address);
  if (!peer_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  *address = peer_->local_address_;
  return net::OK;
}

int FakeStreamSocket::GetLocalAddress(net::IPEndPoint* address) const {
  DCHECK(address);
  *address = local_address_;
  return net::OK;
}

const net::NetLogWithSource& FakeStreamSocket::NetLog() const {
  return net_log_;
}

bool FakeStreamSocket::WasEverUsed() const {
  return false;
}

bool FakeStreamSocket::WasAlpnNegotiated() const {
  return false;
}

net::NextProto FakeStreamSocket::GetNegotiatedProtocol() const {
  return net::kProtoUnknown;
}

bool FakeStreamSocket::GetSSLInfo(net::SSLInfo* /* ssl_info */) {
  return false;
}

int64_t FakeStreamSocket::GetTotalReceivedBytes() const {
  return 0;
}

void FakeStreamSocket::ApplySocketTag(const net::SocketTag& tag) {}

}  // namespace chromecast
