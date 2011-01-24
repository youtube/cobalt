// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket_win.h"

#include <mstcpip.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory_debug.h"
#include "base/metrics/stats_counters.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/win/object_watcher.h"
#include "net/base/address_list_net_log_param.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/winsock_init.h"

namespace net {

namespace {

// Assert that the (manual-reset) event object is not signaled.
void AssertEventNotSignaled(WSAEVENT hEvent) {
  DWORD wait_rv = WaitForSingleObject(hEvent, 0);
  if (wait_rv != WAIT_TIMEOUT) {
    DWORD err = ERROR_SUCCESS;
    if (wait_rv == WAIT_FAILED)
      err = GetLastError();
    CHECK(false);  // Crash.
    // This LOG statement is unreachable since we have already crashed, but it
    // should prevent the compiler from optimizing away the |wait_rv| and
    // |err| variables so they appear nicely on the stack in crash dumps.
    VLOG(1) << "wait_rv=" << wait_rv << ", err=" << err;
  }
}

// If the (manual-reset) event object is signaled, resets it and returns true.
// Otherwise, does nothing and returns false.  Called after a Winsock function
// succeeds synchronously
//
// Our testing shows that except in rare cases (when running inside QEMU),
// the event object is already signaled at this point, so we call this method
// to avoid a context switch in common cases.  This is just a performance
// optimization.  The code still works if this function simply returns false.
bool ResetEventIfSignaled(WSAEVENT hEvent) {
  // TODO(wtc): Remove the CHECKs after enough testing.
  DWORD wait_rv = WaitForSingleObject(hEvent, 0);
  if (wait_rv == WAIT_TIMEOUT)
    return false;  // The event object is not signaled.
  CHECK_EQ(WAIT_OBJECT_0, wait_rv);
  BOOL ok = WSAResetEvent(hEvent);
  CHECK(ok);
  return true;
}

//-----------------------------------------------------------------------------

int MapWinsockError(int os_error) {
  // There are numerous Winsock error codes, but these are the ones we thus far
  // find interesting.
  switch (os_error) {
    case WSAEACCES:
      return ERR_ACCESS_DENIED;
    case WSAENETDOWN:
      return ERR_INTERNET_DISCONNECTED;
    case WSAETIMEDOUT:
      return ERR_TIMED_OUT;
    case WSAECONNRESET:
    case WSAENETRESET:  // Related to keep-alive
      return ERR_CONNECTION_RESET;
    case WSAECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case WSAECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case WSA_IO_INCOMPLETE:
    case WSAEDISCON:
      // WSAEDISCON is returned by WSARecv or WSARecvFrom for message-oriented
      // sockets (where a return value of zero means a zero-byte message) to
      // indicate graceful connection shutdown.  We should not ever see this
      // error code for TCP sockets, which are byte stream oriented.
      LOG(DFATAL) << "Unexpected error " << os_error
                  << " mapped to net::ERR_UNEXPECTED";
      return ERR_UNEXPECTED;
    case WSAEHOSTUNREACH:
    case WSAENETUNREACH:
      return ERR_ADDRESS_UNREACHABLE;
    case WSAEADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case ERROR_SUCCESS:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << os_error
                   << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

int MapConnectError(int os_error) {
  switch (os_error) {
    // connect fails with WSAEACCES when Windows Firewall blocks the
    // connection.
    case WSAEACCES:
      return ERR_NETWORK_ACCESS_DENIED;
    case WSAETIMEDOUT:
      return ERR_CONNECTION_TIMED_OUT;
    default: {
      int net_error = MapWinsockError(os_error);
      if (net_error == ERR_FAILED)
        return ERR_CONNECTION_FAILED;  // More specific than ERR_FAILED.

      // Give a more specific error when the user is offline.
      if (net_error == ERR_ADDRESS_UNREACHABLE &&
          NetworkChangeNotifier::IsOffline()) {
        return ERR_INTERNET_DISCONNECTED;
      }

      return net_error;
    }
  }
}

}  // namespace

//-----------------------------------------------------------------------------

// This class encapsulates all the state that has to be preserved as long as
// there is a network IO operation in progress. If the owner TCPClientSocketWin
// is destroyed while an operation is in progress, the Core is detached and it
// lives until the operation completes and the OS doesn't reference any resource
// declared on this class anymore.
class TCPClientSocketWin::Core : public base::RefCounted<Core> {
 public:
  explicit Core(TCPClientSocketWin* socket);

  // Start watching for the end of a read or write operation.
  void WatchForRead();
  void WatchForWrite();

  // The TCPClientSocketWin is going away.
  void Detach() { socket_ = NULL; }

  // The separate OVERLAPPED variables for asynchronous operation.
  // |read_overlapped_| is used for both Connect() and Read().
  // |write_overlapped_| is only used for Write();
  OVERLAPPED read_overlapped_;
  OVERLAPPED write_overlapped_;

  // The buffers used in Read() and Write().
  WSABUF read_buffer_;
  WSABUF write_buffer_;
  scoped_refptr<IOBuffer> read_iobuffer_;
  scoped_refptr<IOBuffer> write_iobuffer_;
  int write_buffer_length_;

  // Throttle the read size based on our current slow start state.
  // Returns the throttled read size.
  int ThrottleReadSize(int size) {
    if (slow_start_throttle_ < kMaxSlowStartThrottle) {
      size = std::min(size, slow_start_throttle_);
      slow_start_throttle_ *= 2;
    }
    return size;
  }

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
  TCPClientSocketWin* socket_;

  // |reader_| handles the signals from |read_watcher_|.
  ReadDelegate reader_;
  // |writer_| handles the signals from |write_watcher_|.
  WriteDelegate writer_;

  // |read_watcher_| watches for events from Connect() and Read().
  base::win::ObjectWatcher read_watcher_;
  // |write_watcher_| watches for events from Write();
  base::win::ObjectWatcher write_watcher_;

  // When doing reads from the socket, we try to mirror TCP's slow start.
  // We do this because otherwise the async IO subsystem artifically delays
  // returning data to the application.
  static const int kInitialSlowStartThrottle = 1 * 1024;
  static const int kMaxSlowStartThrottle = 32 * kInitialSlowStartThrottle;
  int slow_start_throttle_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

TCPClientSocketWin::Core::Core(
    TCPClientSocketWin* socket)
    : write_buffer_length_(0),
      socket_(socket),
      ALLOW_THIS_IN_INITIALIZER_LIST(reader_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(writer_(this)),
      slow_start_throttle_(kInitialSlowStartThrottle) {
  memset(&read_overlapped_, 0, sizeof(read_overlapped_));
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));
}

TCPClientSocketWin::Core::~Core() {
  // Make sure the message loop is not watching this object anymore.
  read_watcher_.StopWatching();
  write_watcher_.StopWatching();

  WSACloseEvent(read_overlapped_.hEvent);
  memset(&read_overlapped_, 0, sizeof(read_overlapped_));
  WSACloseEvent(write_overlapped_.hEvent);
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));
}

void TCPClientSocketWin::Core::WatchForRead() {
  // We grab an extra reference because there is an IO operation in progress.
  // Balanced in ReadDelegate::OnObjectSignaled().
  AddRef();
  read_watcher_.StartWatching(read_overlapped_.hEvent, &reader_);
}

void TCPClientSocketWin::Core::WatchForWrite() {
  // We grab an extra reference because there is an IO operation in progress.
  // Balanced in WriteDelegate::OnObjectSignaled().
  AddRef();
  write_watcher_.StartWatching(write_overlapped_.hEvent, &writer_);
}

void TCPClientSocketWin::Core::ReadDelegate::OnObjectSignaled(
    HANDLE object) {
  DCHECK_EQ(object, core_->read_overlapped_.hEvent);
  if (core_->socket_) {
    if (core_->socket_->waiting_connect()) {
      core_->socket_->DidCompleteConnect();
    } else {
      core_->socket_->DidCompleteRead();
    }
  }

  core_->Release();
}

void TCPClientSocketWin::Core::WriteDelegate::OnObjectSignaled(
    HANDLE object) {
  DCHECK_EQ(object, core_->write_overlapped_.hEvent);
  if (core_->socket_)
    core_->socket_->DidCompleteWrite();

  core_->Release();
}

//-----------------------------------------------------------------------------

TCPClientSocketWin::TCPClientSocketWin(const AddressList& addresses,
                                       net::NetLog* net_log,
                                       const net::NetLog::Source& source)
    : socket_(INVALID_SOCKET),
      addresses_(addresses),
      current_ai_(NULL),
      waiting_read_(false),
      waiting_write_(false),
      read_callback_(NULL),
      write_callback_(NULL),
      next_connect_state_(CONNECT_STATE_NONE),
      connect_os_error_(0),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)),
      previously_disconnected_(false) {
  scoped_refptr<NetLog::EventParameters> params;
  if (source.is_valid())
    params = new NetLogSourceParameter("source_dependency", source);
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE, params);
  EnsureWinsockInit();
}

TCPClientSocketWin::~TCPClientSocketWin() {
  Disconnect();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE, NULL);
}

void TCPClientSocketWin::AdoptSocket(SOCKET socket) {
  DCHECK_EQ(socket_, INVALID_SOCKET);
  socket_ = socket;
  int error = SetupSocket();
  DCHECK_EQ(0, error);
  current_ai_ = addresses_.head();
  use_history_.set_was_ever_connected();
}

int TCPClientSocketWin::Connect(CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());

  // If already connected, then just return OK.
  if (socket_ != INVALID_SOCKET)
    return OK;

  static base::StatsCounter connects("tcp.connect");
  connects.Increment();

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT,
                      new AddressListNetLogParam(addresses_));

  // We will try to connect to each address in addresses_. Start with the
  // first one in the list.
  next_connect_state_ = CONNECT_STATE_CONNECT;
  current_ai_ = addresses_.head();

  int rv = DoConnectLoop(OK);
  if (rv == ERR_IO_PENDING) {
    // Synchronous operation not supported.
    DCHECK(callback);
    read_callback_ = callback;
  } else {
    LogConnectCompletion(rv);
  }

  return rv;
}

int TCPClientSocketWin::DoConnectLoop(int result) {
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
        LOG(DFATAL) << "bad state " << state;
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_connect_state_ != CONNECT_STATE_NONE);

  return rv;
}

int TCPClientSocketWin::DoConnect() {
  const struct addrinfo* ai = current_ai_;
  DCHECK(ai);
  DCHECK_EQ(0, connect_os_error_);

  if (previously_disconnected_) {
    use_history_.Reset();
    previously_disconnected_ = false;
  }

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT,
                      new NetLogStringParameter(
                          "address", NetAddressToStringWithPort(current_ai_)));

  next_connect_state_ = CONNECT_STATE_CONNECT_COMPLETE;

  connect_os_error_ = CreateSocket(ai);
  if (connect_os_error_ != 0)
    return MapWinsockError(connect_os_error_);

  DCHECK(!core_);
  core_ = new Core(this);

  // WSACreateEvent creates a manual-reset event object.
  core_->read_overlapped_.hEvent = WSACreateEvent();
  // WSAEventSelect sets the socket to non-blocking mode as a side effect.
  // Our connect() and recv() calls require that the socket be non-blocking.
  WSAEventSelect(socket_, core_->read_overlapped_.hEvent, FD_CONNECT);

  core_->write_overlapped_.hEvent = WSACreateEvent();

  if (!connect(socket_, ai->ai_addr, static_cast<int>(ai->ai_addrlen))) {
    // Connected without waiting!
    //
    // The MSDN page for connect says:
    //   With a nonblocking socket, the connection attempt cannot be completed
    //   immediately. In this case, connect will return SOCKET_ERROR, and
    //   WSAGetLastError will return WSAEWOULDBLOCK.
    // which implies that for a nonblocking socket, connect never returns 0.
    // It's not documented whether the event object will be signaled or not
    // if connect does return 0.  So the code below is essentially dead code
    // and we don't know if it's correct.
    NOTREACHED();

    if (ResetEventIfSignaled(core_->read_overlapped_.hEvent))
      return OK;
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSAEWOULDBLOCK) {
      LOG(ERROR) << "connect failed: " << os_error;
      connect_os_error_ = os_error;
      return MapConnectError(os_error);
    }
  }

  core_->WatchForRead();
  return ERR_IO_PENDING;
}

int TCPClientSocketWin::DoConnectComplete(int result) {
  // Log the end of this attempt (and any OS error it threw).
  int os_error = connect_os_error_;
  connect_os_error_ = 0;
  scoped_refptr<NetLog::EventParameters> params;
  if (result != OK)
    params = new NetLogIntegerParameter("os_error", os_error);
  net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT, params);

  if (result == OK) {
    use_history_.set_was_ever_connected();
    return OK;  // Done!
  }

  // Close whatever partially connected socket we currently have.
  DoDisconnect();

  // Try to fall back to the next address in the list.
  if (current_ai_->ai_next) {
    next_connect_state_ = CONNECT_STATE_CONNECT;
    current_ai_ = current_ai_->ai_next;
    return OK;
  }

  // Otherwise there is nothing to fall back to, so give up.
  return result;
}

void TCPClientSocketWin::Disconnect() {
  DoDisconnect();
  current_ai_ = NULL;
}

void TCPClientSocketWin::DoDisconnect() {
  DCHECK(CalledOnValidThread());

  if (socket_ == INVALID_SOCKET)
    return;

  // Note: don't use CancelIo to cancel pending IO because it doesn't work
  // when there is a Winsock layered service provider.

  // In most socket implementations, closing a socket results in a graceful
  // connection shutdown, but in Winsock we have to call shutdown explicitly.
  // See the MSDN page "Graceful Shutdown, Linger Options, and Socket Closure"
  // at http://msdn.microsoft.com/en-us/library/ms738547.aspx
  shutdown(socket_, SD_SEND);

  // This cancels any pending IO.
  closesocket(socket_);
  socket_ = INVALID_SOCKET;

  if (waiting_connect()) {
    // We closed the socket, so this notification will never come.
    // From MSDN' WSAEventSelect documentation:
    // "Closing a socket with closesocket also cancels the association and
    // selection of network events specified in WSAEventSelect for the socket".
    core_->Release();
  }

  waiting_read_ = false;
  waiting_write_ = false;

  core_->Detach();
  core_ = NULL;

  previously_disconnected_ = true;
}

bool TCPClientSocketWin::IsConnected() const {
  DCHECK(CalledOnValidThread());

  if (socket_ == INVALID_SOCKET || waiting_connect())
    return false;

  // Check if connection is alive.
  char c;
  int rv = recv(socket_, &c, 1, MSG_PEEK);
  if (rv == 0)
    return false;
  if (rv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    return false;

  return true;
}

bool TCPClientSocketWin::IsConnectedAndIdle() const {
  DCHECK(CalledOnValidThread());

  if (socket_ == INVALID_SOCKET || waiting_connect())
    return false;

  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c;
  int rv = recv(socket_, &c, 1, MSG_PEEK);
  if (rv >= 0)
    return false;
  if (WSAGetLastError() != WSAEWOULDBLOCK)
    return false;

  return true;
}

int TCPClientSocketWin::GetPeerAddress(AddressList* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  address->Copy(current_ai_, false);
  return OK;
}

void TCPClientSocketWin::SetSubresourceSpeculation() {
  use_history_.set_subresource_speculation();
}

void TCPClientSocketWin::SetOmniboxSpeculation() {
  use_history_.set_omnibox_speculation();
}

bool TCPClientSocketWin::WasEverUsed() const {
  return use_history_.was_used_to_convey_data();
}

bool TCPClientSocketWin::UsingTCPFastOpen() const {
  // Not supported on windows.
  return false;
}

int TCPClientSocketWin::Read(IOBuffer* buf,
                             int buf_len,
                             CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK(!waiting_read_);
  DCHECK(!read_callback_);
  DCHECK(!core_->read_iobuffer_);

  buf_len = core_->ThrottleReadSize(buf_len);

  core_->read_buffer_.len = buf_len;
  core_->read_buffer_.buf = buf->data();

  // TODO(wtc): Remove the assertion after enough testing.
  AssertEventNotSignaled(core_->read_overlapped_.hEvent);
  DWORD num, flags = 0;
  int rv = WSARecv(socket_, &core_->read_buffer_, 1, &num, &flags,
                   &core_->read_overlapped_, NULL);
  if (rv == 0) {
    if (ResetEventIfSignaled(core_->read_overlapped_.hEvent)) {
      // Because of how WSARecv fills memory when used asynchronously, Purify
      // isn't able to detect that it's been initialized, so it scans for 0xcd
      // in the buffer and reports UMRs (uninitialized memory reads) for those
      // individual bytes. We override that in PURIFY builds to avoid the
      // false error reports.
      // See bug 5297.
      base::MemoryDebug::MarkAsInitialized(core_->read_buffer_.buf, num);
      static base::StatsCounter read_bytes("tcp.read_bytes");
      read_bytes.Add(num);
      if (num > 0)
        use_history_.set_was_used_to_convey_data();
      LogByteTransfer(net_log_, NetLog::TYPE_SOCKET_BYTES_RECEIVED, num,
                      core_->read_buffer_.buf);
      return static_cast<int>(num);
    }
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSA_IO_PENDING)
      return MapWinsockError(os_error);
  }
  core_->WatchForRead();
  waiting_read_ = true;
  read_callback_ = callback;
  core_->read_iobuffer_ = buf;
  return ERR_IO_PENDING;
}

int TCPClientSocketWin::Write(IOBuffer* buf,
                              int buf_len,
                              CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK(!waiting_write_);
  DCHECK(!write_callback_);
  DCHECK_GT(buf_len, 0);
  DCHECK(!core_->write_iobuffer_);

  static base::StatsCounter writes("tcp.writes");
  writes.Increment();

  core_->write_buffer_.len = buf_len;
  core_->write_buffer_.buf = buf->data();
  core_->write_buffer_length_ = buf_len;

  // TODO(wtc): Remove the assertion after enough testing.
  AssertEventNotSignaled(core_->write_overlapped_.hEvent);
  DWORD num;
  int rv = WSASend(socket_, &core_->write_buffer_, 1, &num, 0,
                   &core_->write_overlapped_, NULL);
  if (rv == 0) {
    if (ResetEventIfSignaled(core_->write_overlapped_.hEvent)) {
      rv = static_cast<int>(num);
      if (rv > buf_len || rv < 0) {
        // It seems that some winsock interceptors report that more was written
        // than was available. Treat this as an error.  http://crbug.com/27870
        LOG(ERROR) << "Detected broken LSP: Asked to write " << buf_len
                   << " bytes, but " << rv << " bytes reported.";
        return ERR_WINSOCK_UNEXPECTED_WRITTEN_BYTES;
      }
      static base::StatsCounter write_bytes("tcp.write_bytes");
      write_bytes.Add(rv);
      if (rv > 0)
        use_history_.set_was_used_to_convey_data();
      LogByteTransfer(net_log_, NetLog::TYPE_SOCKET_BYTES_SENT, rv,
                      core_->write_buffer_.buf);
      return rv;
    }
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSA_IO_PENDING)
      return MapWinsockError(os_error);
  }
  core_->WatchForWrite();
  waiting_write_ = true;
  write_callback_ = callback;
  core_->write_iobuffer_ = buf;
  return ERR_IO_PENDING;
}

bool TCPClientSocketWin::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  int rv = setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size));
  DCHECK(!rv) << "Could not set socket receive buffer size: " << GetLastError();
  return rv == 0;
}

bool TCPClientSocketWin::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  int rv = setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size));
  DCHECK(!rv) << "Could not set socket send buffer size: " << GetLastError();
  return rv == 0;
}

int TCPClientSocketWin::CreateSocket(const struct addrinfo* ai) {
  socket_ = WSASocket(ai->ai_family, ai->ai_socktype, ai->ai_protocol, NULL, 0,
                      WSA_FLAG_OVERLAPPED);
  if (socket_ == INVALID_SOCKET) {
    int os_error = WSAGetLastError();
    LOG(ERROR) << "WSASocket failed: " << os_error;
    return os_error;
  }
  return SetupSocket();
}

int TCPClientSocketWin::SetupSocket() {
  // Increase the socket buffer sizes from the default sizes for WinXP.  In
  // performance testing, there is substantial benefit by increasing from 8KB
  // to 64KB.
  // See also:
  //    http://support.microsoft.com/kb/823764/EN-US
  // On Vista, if we manually set these sizes, Vista turns off its receive
  // window auto-tuning feature.
  //    http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
  // Since Vista's auto-tune is better than any static value we can could set,
  // only change these on pre-vista machines.
  int32 major_version, minor_version, fix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
    &fix_version);
  if (major_version < 6) {
    const int32 kSocketBufferSize = 64 * 1024;
    SetReceiveBufferSize(kSocketBufferSize);
    SetSendBufferSize(kSocketBufferSize);
  }

  // Disable Nagle.
  // The Nagle implementation on windows is governed by RFC 896.  The idea
  // behind Nagle is to reduce small packets on the network.  When Nagle is
  // enabled, if a partial packet has been sent, the TCP stack will disallow
  // further *partial* packets until an ACK has been received from the other
  // side.  Good applications should always strive to send as much data as
  // possible and avoid partial-packet sends.  However, in most real world
  // applications, there are edge cases where this does not happen, and two
  // partil packets may be sent back to back.  For a browser, it is NEVER
  // a benefit to delay for an RTT before the second packet is sent.
  //
  // As a practical example in Chromium today, consider the case of a small
  // POST.  I have verified this:
  //     Client writes 649 bytes of header  (partial packet #1)
  //     Client writes 50 bytes of POST data (partial packet #2)
  // In the above example, with Nagle, a RTT delay is inserted between these
  // two sends due to nagle.  RTTs can easily be 100ms or more.  The best
  // fix is to make sure that for POSTing data, we write as much data as
  // possible and minimize partial packets.  We will fix that.  But disabling
  // Nagle also ensure we don't run into this delay in other edge cases.
  // See also:
  //    http://technet.microsoft.com/en-us/library/bb726981.aspx
  const BOOL kDisableNagle = TRUE;
  int rv = setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                      reinterpret_cast<const char*>(&kDisableNagle),
                      sizeof(kDisableNagle));
  DCHECK(!rv) << "Could not disable nagle";

  // Enable TCP Keep-Alive to prevent NAT routers from timing out TCP
  // connections. See http://crbug.com/27400 for details.

  struct tcp_keepalive keepalive_vals = {
    1, // TCP keep-alive on.
    45000,  // Wait 45s until sending first TCP keep-alive packet.
    45000,  // Wait 45s between sending TCP keep-alive packets.
  };
  DWORD bytes_returned = 0xABAB;
  rv = WSAIoctl(socket_, SIO_KEEPALIVE_VALS, &keepalive_vals,
                sizeof(keepalive_vals), NULL, 0,
                &bytes_returned, NULL, NULL);
  DCHECK_EQ(0u, bytes_returned);
  DCHECK(!rv) << "Could not enable TCP Keep-Alive for socket: " << socket_
              << " [error: " << WSAGetLastError() << "].";

  // Disregard any failure in disabling nagle or enabling TCP Keep-Alive.
  return 0;
}

void TCPClientSocketWin::LogConnectCompletion(int net_error) {
  scoped_refptr<NetLog::EventParameters> params;
  if (net_error != OK)
    params = new NetLogIntegerParameter("net_error", net_error);
  net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT, params);
  if (net_error == OK)
    UpdateConnectionTypeHistograms(CONNECTION_ANY);
}

void TCPClientSocketWin::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(read_callback_);

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback* c = read_callback_;
  read_callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocketWin::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(write_callback_);

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback* c = write_callback_;
  write_callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocketWin::DidCompleteConnect() {
  DCHECK_EQ(next_connect_state_, CONNECT_STATE_CONNECT_COMPLETE);
  int result;

  WSANETWORKEVENTS events;
  int rv = WSAEnumNetworkEvents(socket_, core_->read_overlapped_.hEvent,
                                &events);
  int os_error = 0;
  if (rv == SOCKET_ERROR) {
    NOTREACHED();
    os_error = WSAGetLastError();
    result = MapWinsockError(os_error);
  } else if (events.lNetworkEvents & FD_CONNECT) {
    os_error = events.iErrorCode[FD_CONNECT_BIT];
    result = MapConnectError(os_error);
  } else {
    NOTREACHED();
    result = ERR_UNEXPECTED;
  }

  connect_os_error_ = os_error;
  rv = DoConnectLoop(result);
  if (rv != ERR_IO_PENDING) {
    LogConnectCompletion(rv);
    DoReadCallback(rv);
  }
}

void TCPClientSocketWin::DidCompleteRead() {
  DCHECK(waiting_read_);
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &core_->read_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(core_->read_overlapped_.hEvent);
  waiting_read_ = false;
  core_->read_iobuffer_ = NULL;
  if (ok) {
    static base::StatsCounter read_bytes("tcp.read_bytes");
    read_bytes.Add(num_bytes);
    if (num_bytes > 0)
      use_history_.set_was_used_to_convey_data();
    LogByteTransfer(net_log_, NetLog::TYPE_SOCKET_BYTES_RECEIVED, num_bytes,
                    core_->read_buffer_.buf);
  }
  DoReadCallback(ok ? num_bytes : MapWinsockError(WSAGetLastError()));
}

void TCPClientSocketWin::DidCompleteWrite() {
  DCHECK(waiting_write_);

  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &core_->write_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(core_->write_overlapped_.hEvent);
  waiting_write_ = false;
  int rv;
  if (!ok) {
    rv = MapWinsockError(WSAGetLastError());
  } else {
    rv = static_cast<int>(num_bytes);
    if (rv > core_->write_buffer_length_ || rv < 0) {
      // It seems that some winsock interceptors report that more was written
      // than was available. Treat this as an error.  http://crbug.com/27870
      LOG(ERROR) << "Detected broken LSP: Asked to write "
                 << core_->write_buffer_length_ << " bytes, but " << rv
                 << " bytes reported.";
      rv = ERR_WINSOCK_UNEXPECTED_WRITTEN_BYTES;
    } else {
      static base::StatsCounter write_bytes("tcp.write_bytes");
      write_bytes.Add(num_bytes);
      if (num_bytes > 0)
        use_history_.set_was_used_to_convey_data();
      LogByteTransfer(net_log_, NetLog::TYPE_SOCKET_BYTES_SENT, num_bytes,
                      core_->write_buffer_.buf);
    }
  }
  core_->write_iobuffer_ = NULL;
  DoWriteCallback(rv);
}

}  // namespace net
