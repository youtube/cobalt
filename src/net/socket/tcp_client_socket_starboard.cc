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

#include "net/socket/tcp_client_socket.h"

#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/posix/eintr_wrapper.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "nb/memory_scope.h"
#include "net/base/address_family.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/socket_net_log_params.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/time.h"

namespace net {

namespace {

const SbTime kTCPKeepAliveDuration = 45 * kSbTimeSecond;

int SetupSocket(SbSocket socket) {
  // This mirrors the behaviour on Windows. See the comment in
  // tcp_client_socket_win.cc after searching for "NODELAY".
  SbSocketSetTcpNoDelay(socket, true);
  SbSocketSetTcpKeepAlive(socket, true, kTCPKeepAliveDuration);
  SbSocketSetTcpWindowScaling(socket, true);
  if (TCPClientSocketStarboard::kReceiveBufferSize != 0) {
    SbSocketSetReceiveBufferSize(socket,
                                 TCPClientSocketStarboard::kReceiveBufferSize);
  }
  return 0;
}

// Creates a new socket and sets default parameters for it. Returns the OS error
// code (or 0 on success).
int CreateSocket(AddressFamily family, SbSocket* socket) {
  SbSocketAddressType type = family == ADDRESS_FAMILY_IPV6
                                 ? kSbSocketAddressTypeIpv6
                                 : kSbSocketAddressTypeIpv4;
  *socket = SbSocketCreate(type, kSbSocketProtocolTcp);
  if (!SbSocketIsValid(*socket)) {
    DLOG(ERROR) << "SbSocketCreate";
    return SbSystemGetLastError();
  }

  int error = SetupSocket(*socket);
  if (error) {
    if (!SbSocketDestroy(*socket)) {
      DLOG(ERROR) << "SbSocketDestroy";
    }

    *socket = kSbSocketInvalid;
    return error;
  }
  return 0;
}

}  // namespace

//-----------------------------------------------------------------------------

class TCPClientSocketStarboard::Watcher : public MessageLoopForIO::Watcher {
 public:
  explicit Watcher(TCPClientSocketStarboard* socket) : socket_(socket) {}
  virtual ~Watcher() {}

  void OnSocketReadyToRead(SbSocket socket) override {
    socket_->DidCompleteRead();
  }

  void OnSocketReadyToWrite(SbSocket socket) override {
    // is this a connection callback?
    if (socket_->waiting_connect()) {
      socket_->DidCompleteConnect();
    } else {
      socket_->DidCompleteWrite();
    }
  }

 private:
  TCPClientSocketStarboard* socket_;
};

//-----------------------------------------------------------------------------

TCPClientSocketStarboard::TCPClientSocketStarboard(
    const AddressList& addresses,
    net::NetLog* net_log,
    const net::NetLog::Source& source)
    : next_connect_state_(CONNECT_STATE_NONE),
      connect_error_(OK),
      socket_watcher_(new MessageLoopForIO::SocketWatcher()),
      ALLOW_THIS_IN_INITIALIZER_LIST(watcher_(new Watcher(this))),
      read_buf_len_(0),
      write_buf_len_(0),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)),
      socket_(kSbSocketInvalid),
      addresses_(addresses),
      current_address_index_(-1),
      num_bytes_read_(0),
      previously_disconnected_(true),
      waiting_read_(false),
      waiting_write_(false) {
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE,
                      source.ToEventParametersCallback());
}

TCPClientSocketStarboard::~TCPClientSocketStarboard() {
  Disconnect();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE);
}

int TCPClientSocketStarboard::AdoptSocket(SbSocket socket) {
  DCHECK(!SbSocketIsValid(socket_));

  int error = SetupSocket(socket);
  if (error) {
    return error;
  }

  socket_ = socket;

  // This is to make GetPeerAddress() work. It's up to the caller to ensure that
  // |address_| contains a reasonable address for this socket. (i.e. at least
  // match IPv4 vs IPv6!).
  current_address_index_ = 0;
  use_history_.set_was_ever_connected();

  return OK;
}

int TCPClientSocketStarboard::Connect(const CompletionCallback& callback) {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK(CalledOnValidThread());

  // If this socket is already valid, then just return OK.
  if (SbSocketIsValid(socket_)) {
    return OK;
  }

  base::StatsCounter connects("tcp.connect");
  connects.Increment();

  // we should not have another call to Connect() already in flight
  DCHECK_EQ(CONNECT_STATE_NONE, next_connect_state_);

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT,
                      addresses_.CreateNetLogCallback());

  next_connect_state_ = CONNECT_STATE_CONNECT;
  current_address_index_ = 0;

  int rv = DoConnectLoop(OK);
  if (rv == ERR_IO_PENDING) {
    write_callback_ = callback;
  } else {
    LogConnectCompletion(rv);
  }

  return rv;
}

// pretty much cribbed directly from TCPClientSocketLibevent and/or
// TCPClientSocketWin, take your pick, they're identical
int TCPClientSocketStarboard::DoConnectLoop(int result) {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK_NE(next_connect_state_, CONNECT_STATE_NONE);

  int rv = result;
  do {
    ConnectState state = next_connect_state_;
    next_connect_state_ = CONNECT_STATE_NONE;
    switch (state) {
      case CONNECT_STATE_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoConnect();
        break;
      case CONNECT_STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      default:
        NOTREACHED() << "Bad state: " << state;
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_connect_state_ != CONNECT_STATE_NONE);

  return rv;
}

int TCPClientSocketStarboard::DoConnect() {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK_GE(current_address_index_, 0);
  DCHECK_LT(current_address_index_, static_cast<int>(addresses_.size()));

  const IPEndPoint& endpoint = addresses_[current_address_index_];

  if (previously_disconnected_) {
    use_history_.Reset();
    previously_disconnected_ = false;
  }

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT,
                      CreateNetLogIPEndPointCallback(&endpoint));

  next_connect_state_ = CONNECT_STATE_CONNECT_COMPLETE;

  // create a non-blocking socket
  connect_error_ = CreateSocket(endpoint.GetFamily(), &socket_);
  if (connect_error_ != OK) {
    return connect_error_;
  }

  connect_start_time_ = base::TimeTicks::Now();

  SbSocketAddress address;
  if (!endpoint.ToSbSocketAddress(&address)) {
    DLOG(ERROR) << __FUNCTION__ << "[" << this << "]: Error: Invalid address";
    return ERR_INVALID_ARGUMENT;
  }

  bool success = SbSocketConnect(socket_, &address);
  DCHECK_EQ(true, success);

  int rv = MapLastSocketError(socket_);
  if (rv != ERR_IO_PENDING) {
    DLOG(ERROR) << __FUNCTION__ << "[" << this << "]: Error: " << rv;
    return rv;
  }

  // When it is ready to write, it will have connected.
  MessageLoopForIO::current()->Watch(socket_, false,
                                     MessageLoopForIO::WATCH_WRITE,
                                     socket_watcher_.get(), watcher_.get());
  return ERR_IO_PENDING;
}

int TCPClientSocketStarboard::DoConnectComplete(int result) {
  TRACK_MEMORY_SCOPE("Network");
  // Log the end of this attempt (and any OS error it threw).
  int error = connect_error_;
  connect_error_ = OK;
  if (result != OK) {
    net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT,
                      NetLog::IntegerCallback("error", error));
  } else {
    net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT);
  }

  if (result == OK) {
    connect_time_micros_ = base::TimeTicks::Now() - connect_start_time_;
    use_history_.set_was_ever_connected();
    return OK;  // Done!
  }

  // ok, didn't work, clean up
  DoDisconnect();

  // Try to fall back to the next address in the list.
  if (current_address_index_ + 1 < static_cast<int>(addresses_.size())) {
    next_connect_state_ = CONNECT_STATE_CONNECT;
    ++current_address_index_;
    return OK;
  }
  // Otherwise there is nothing to fall back to, so give up.
  return result;
}

void TCPClientSocketStarboard::DidCompleteConnect() {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK_EQ(next_connect_state_, CONNECT_STATE_CONNECT_COMPLETE);

  connect_error_ = SbSocketIsConnected(socket_) ? OK : ERR_FAILED;
  int rv = DoConnectLoop(connect_error_);
  if (rv != ERR_IO_PENDING) {
    LogConnectCompletion(rv);
    DoWriteCallback(rv);
  }
}

void TCPClientSocketStarboard::LogConnectCompletion(int net_error) {
  TRACK_MEMORY_SCOPE("Network");
  if (net_error == OK)
    UpdateConnectionTypeHistograms(CONNECTION_ANY);

  if (net_error != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_CONNECT, net_error);
    return;
  }

  SbSocketAddress address;
  bool result = SbSocketGetLocalAddress(socket_, &address);
  if (!result) {
    int error = MapLastSocketError(socket_);
    DLOG(ERROR) << "SbSocketGetLocalAddress() [rv: " << error << "] error";
    NOTREACHED();
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_CONNECT, error);
    return;
  }

  net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT);
}

void TCPClientSocketStarboard::Disconnect() {
  DoDisconnect();
  current_address_index_ = -1;
}

void TCPClientSocketStarboard::DoDisconnect() {
  if (!SbSocketIsValid(socket_))
    return;

  // Make sure to clean up our watch accounting before we destroy the socket, or
  // else we would later try to stop watching a deleted socket.
  socket_watcher_->StopWatchingSocket();

  SbSocketDestroy(socket_);
  socket_ = kSbSocketInvalid;

  waiting_read_ = false;
  waiting_write_ = false;

  previously_disconnected_ = true;
}

bool TCPClientSocketStarboard::IsConnected() const {
  if (!SbSocketIsValid(socket_) || waiting_connect())
    return false;

  return SbSocketIsConnected(socket_);
}

bool TCPClientSocketStarboard::IsConnectedAndIdle() const {
  if (!SbSocketIsValid(socket_) || waiting_connect())
    return false;

  return SbSocketIsConnectedAndIdle(socket_);
}

int TCPClientSocketStarboard::GetPeerAddress(IPEndPoint* address) const {
  DCHECK(address);
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = addresses_[current_address_index_];
  return OK;
}

int TCPClientSocketStarboard::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(address);
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;

  SbSocketAddress sb_address;
  if (!SbSocketGetLocalAddress(socket_, &sb_address))
    return ERR_FAILED;

  if (!address->FromSbSocketAddress(&sb_address))
    return ERR_FAILED;

  return OK;
}

const BoundNetLog& TCPClientSocketStarboard::NetLog() const {
  return net_log_;
}

void TCPClientSocketStarboard::SetSubresourceSpeculation() {
  use_history_.set_subresource_speculation();
}

void TCPClientSocketStarboard::SetOmniboxSpeculation() {
  use_history_.set_omnibox_speculation();
}

bool TCPClientSocketStarboard::WasEverUsed() const {
  return use_history_.was_used_to_convey_data();
}

bool TCPClientSocketStarboard::UsingTCPFastOpen() const {
  // This is not supported on some platforms.
  return false;
}

int64 TCPClientSocketStarboard::NumBytesRead() const {
  return num_bytes_read_;
}

base::TimeDelta TCPClientSocketStarboard::GetConnectTimeMicros() const {
  return connect_time_micros_;
}

bool TCPClientSocketStarboard::WasNpnNegotiated() const {
  return false;
}

NextProto TCPClientSocketStarboard::GetNegotiatedProtocol() const {
  return kProtoUnknown;
}

bool TCPClientSocketStarboard::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

int TCPClientSocketStarboard::Read(IOBuffer* buf,
                                   int buf_len,
                                   const CompletionCallback& callback) {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK(SbSocketIsValid(socket_));
  DCHECK(!waiting_connect());
  DCHECK(!waiting_read_);
  DCHECK_GT(buf_len, 0);

  int bytes_read = SbSocketReceiveFrom(socket_, buf->data(), buf_len, NULL);
  // Maybe we got some data. If so, do stats and return.
  if (bytes_read >= 0) {
    base::StatsCounter read_bytes("tcp.read_bytes");
    read_bytes.Add(bytes_read);
    num_bytes_read_ += bytes_read;
    if (bytes_read > 0)
      use_history_.set_was_used_to_convey_data();
    net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_RECEIVED,
                                  bytes_read, buf->data());
    return bytes_read;
  }

  // If bytes_read < 0, then we got some kind of error.
  int error = MapLastSocketError(socket_);
  if (error != ERR_IO_PENDING) {
    DLOG(ERROR) << __FUNCTION__ << "[" << this << "]: Error: " << error;
    return error;
  }

  // We'll callback when there is more data.
  waiting_read_ = true;
  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = callback;
  MessageLoopForIO::current()->Watch(socket_, false,
                                     MessageLoopForIO::WATCH_READ,
                                     socket_watcher_.get(), watcher_.get());
  return ERR_IO_PENDING;
}

void TCPClientSocketStarboard::DidCompleteRead() {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK(waiting_read_);
  int bytes_read =
      SbSocketReceiveFrom(socket_, read_buf_->data(), read_buf_len_, NULL);
  int result;
  if (bytes_read >= 0) {
    result = bytes_read;
    base::StatsCounter read_bytes("tcp.read_bytes");
    read_bytes.Add(bytes_read);
    num_bytes_read_ += bytes_read;
    if (bytes_read > 0)
      use_history_.set_was_used_to_convey_data();
    net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_SENT, result,
                                  read_buf_->data());
  } else {
    result = MapLastSocketError(socket_);
  }

  if (result != ERR_IO_PENDING) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    waiting_read_ = false;
    DoReadCallback(result);
  }
}

void TCPClientSocketStarboard::DoReadCallback(int rv) {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK_NE(rv, ERR_IO_PENDING);

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback c = read_callback_;
  read_callback_ = CompletionCallback();
  c.Run(rv);
}

int TCPClientSocketStarboard::Write(IOBuffer* buf,
                                    int buf_len,
                                    const CompletionCallback& callback) {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK(SbSocketIsValid(socket_));
  DCHECK(!waiting_connect());
  DCHECK(!waiting_write_);
  DCHECK_GT(buf_len, 0);

  base::StatsCounter writes("tcp.writes");
  writes.Increment();

  int rv = SbSocketSendTo(socket_, buf->data(), buf_len, NULL);
  // did it happen right away?
  if (rv >= 0) {
    base::StatsCounter write_bytes("tcp.write_bytes");
    write_bytes.Add(rv);
    if (rv > 0)
      use_history_.set_was_used_to_convey_data();
    net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_SENT, rv,
                                  buf->data());
    return rv;
  }

  // Some error occurred.
  int mapped_result = MapLastSocketError(socket_);
  if (mapped_result != ERR_IO_PENDING) {
    DLOG(ERROR) << __FUNCTION__ << "[" << this << "]: Error: " << mapped_result;
    return mapped_result;
  }

  // We are just blocked, so let's set up the callback.
  waiting_write_ = true;
  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = callback;
  MessageLoopForIO::current()->Watch(socket_, false,
                                     MessageLoopForIO::WATCH_WRITE,
                                     socket_watcher_.get(), watcher_.get());
  return ERR_IO_PENDING;
}

void TCPClientSocketStarboard::DidCompleteWrite() {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK(waiting_write_);
  int nwrite =
      SbSocketSendTo(socket_, write_buf_->data(), write_buf_len_, NULL);
  int result;
  if (nwrite >= 0) {
    result = nwrite;
    base::StatsCounter write_bytes("tcp.write_bytes");
    write_bytes.Add(nwrite);
    if (nwrite > 0)
      use_history_.set_was_used_to_convey_data();
    net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_SENT, result,
                                  write_buf_->data());
  } else {
    result = MapLastSocketError(socket_);
  }

  if (result != ERR_IO_PENDING) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    waiting_write_ = false;
    DoWriteCallback(result);
  }
}

void TCPClientSocketStarboard::DoWriteCallback(int rv) {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK_NE(rv, ERR_IO_PENDING);

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback c = write_callback_;
  write_callback_ = CompletionCallback();
  c.Run(rv);
}

bool TCPClientSocketStarboard::SetReceiveBufferSize(int32 size) {
  TRACK_MEMORY_SCOPE("Network");
  return SbSocketSetReceiveBufferSize(socket_, size);
}

bool TCPClientSocketStarboard::SetSendBufferSize(int32 size) {
  TRACK_MEMORY_SCOPE("Network");
  return SbSocketSetSendBufferSize(socket_, size);
}

bool TCPClientSocketStarboard::SetKeepAlive(bool enable, int delay) {
  TRACK_MEMORY_SCOPE("Network");
  return SbSocketSetTcpKeepAlive(socket_, enable, delay);
}

bool TCPClientSocketStarboard::SetNoDelay(bool no_delay) {
  TRACK_MEMORY_SCOPE("Network");
  return SbSocketSetTcpNoDelay(socket_, no_delay);
}

bool TCPClientSocketStarboard::SetWindowScaling(bool enable) {
  TRACK_MEMORY_SCOPE("Network");
  return SbSocketSetTcpWindowScaling(socket_, enable);
}

}  // namespace net
