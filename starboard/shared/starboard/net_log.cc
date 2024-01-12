// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/net_log.h"

#include <windows.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <string>

#include "starboard/common/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/socket.h"
#include "starboard/common/string.h"
#include "starboard/common/thread.h"
#include "starboard/common/time.h"
#include "starboard/once.h"
#include "starboard/socket_waiter.h"

#ifndef NET_LOG_PORT
// Default port was generated from a random number generator.
#define NET_LOG_PORT 49353
#endif

// Controls whether using IPv4 or IPv6.
#ifndef NET_LOG_IP_VERSION
#define NET_LOG_IP_VERSION kSbSocketAddressTypeIpv4
#endif

// Default Socket write buffer is 100k.
#ifndef NET_LOG_SOCKET_BUFFER_SIZE
#define NET_LOG_SOCKET_BUFFER_SIZE (1024 * 100)
#endif

// Default in memory write buffer is 1mb.
#ifndef NET_LOG_MAX_IN_MEMORY_BUFFER
#define NET_LOG_MAX_IN_MEMORY_BUFFER (1024 * 1024)
#endif

// Default block to send to the socket is 1k.
#ifndef NET_LOG_SOCKET_SEND_SIZE
#define NET_LOG_SOCKET_SEND_SIZE 1024
#endif

namespace starboard {
namespace shared {
namespace starboard {
namespace {

using RunFunction = std::function<void(Semaphore*)>;

class FunctionThread : public Thread {
 public:
  static scoped_ptr<Thread> Create(const std::string& thread_name,
                                   RunFunction run_function) {
    scoped_ptr<Thread> out(new FunctionThread(thread_name, run_function));
    return out.Pass();
  }

  FunctionThread(const std::string& name, RunFunction run_function)
      : Thread(name), run_function_(run_function) {}

  void Run() override { run_function_(join_sema()); }

 private:
  RunFunction run_function_;
};

std::string ToString(SbSocketError error) {
  switch (error) {
    case kSbSocketOk: {
      return "kSbSocketOk";
    }
    case kSbSocketPending: {
      return "kSbSocketErrorConnectionReset";
    }
    case kSbSocketErrorFailed: {
      return "kSbSocketErrorFailed";
    }

    case kSbSocketErrorConnectionReset: {
      return "kSbSocketErrorConnectionReset";
    }
  }
  SB_NOTREACHED() << "Unexpected case " << error;
  std::stringstream ss;
  ss << "Unknown-" << error;
  return ss.str();
}

scoped_ptr<Socket> CreateListenSocket() {
  scoped_ptr<Socket> socket(
      new Socket(NET_LOG_IP_VERSION, kSbSocketProtocolTcp));
  socket->SetReuseAddress(true);
  SbSocketAddress sock_addr;
  // Ip address will be set to 0.0.0.0 so that it will bind to all sockets.
  memset(&sock_addr, 0, sizeof(SbSocketAddress));
  sock_addr.type = NET_LOG_IP_VERSION;
  sock_addr.port = NET_LOG_PORT;
  SbSocketError sock_err = socket->Bind(&sock_addr);

  const char kErrFmt[] = "Socket error while attempting to bind, error = %d\n";
  // Can't use SB_LOG_IF() because SB_LOG_IF() might have triggered this call,
  // and therefore would triggered reentrant behavior and then deadlock.
  // SbLogRawFormat() is assumed to be safe to call. Note that NetLogWrite()
  // ignores reentrant calls.
  if (sock_err != kSbSocketOk) {
    SbLogRawFormatF(kErrFmt, sock_err);
  }
  sock_err = socket->Listen();
  if (sock_err != kSbSocketOk) {
    SbLogRawFormatF(kErrFmt, sock_err);
  }
  return socket.Pass();
}

class BufferedSocketWriter {
 public:
  BufferedSocketWriter(int in_memory_buffer_size, int chunk_size)
      : max_memory_buffer_size_(in_memory_buffer_size),
        chunk_size_(chunk_size) {}

  void Append(const char* data, size_t data_n) {
    bool overflow = false;
    log_mutex_.Acquire();
    for (const char* curr = data; curr != data + data_n; ++curr) {
      log_.push_back(*curr);
      if (log_.size() > max_memory_buffer_size_) {
        overflow = true;
        log_.pop_front();
      }
    }
    log_mutex_.Release();

    if (overflow) {
      // Can't use SB_LOG_IF() because SB_LOG_IF() might have triggered this
      // call, and therefore would triggered reentrant behavior and then
      // deadlock. SbLogRawFormat() is assumed to be safe to call. Note that
      // NetLogWrite() ignores reentrant calls.
      SbLogRaw("Net log dropped buffer data.");
    }
  }

  void WaitUntilWritableOrConnectionReset(SbSocket sock) {
    SbSocketWaiter waiter = SbSocketWaiterCreate();

    struct F {
      static void WakeUp(SbSocketWaiter waiter, SbSocket, void*, int) {
        SbSocketWaiterWakeUp(waiter);
      }
    };

    SbSocketWaiterAdd(waiter, sock, NULL, &F::WakeUp,
                      kSbSocketWaiterInterestWrite,
                      false);  // false means one shot.

    SbSocketWaiterWait(waiter);
    SbSocketWaiterRemove(waiter, sock);
    SbSocketWaiterDestroy(waiter);
  }

  bool IsConnectionReset(SbSocketError err) {
    return err == kSbSocketErrorConnectionReset;
  }

  // Will flush data through to the dest_socket. Returns |true| if
  // flushed, else connection was dropped or an error occurred.
  bool Flush(SbSocket dest_socket) {
    std::string curr_write_block;
    while (TransferData(chunk_size_, &curr_write_block)) {
      while (!curr_write_block.empty()) {
        int bytes_to_write = static_cast<int>(curr_write_block.size());
        int result = SbSocketSendTo(dest_socket, curr_write_block.c_str(),
                                    bytes_to_write, NULL);

        if (result < 0) {
          SbSocketError err = SbSocketGetLastError(dest_socket);
          SbSocketClearLastError(dest_socket);
          if (err == kSbSocketPending) {
            blocked_counts_.increment();
            WaitUntilWritableOrConnectionReset(dest_socket);
            continue;
          } else if (IsConnectionReset(err)) {
            return false;
          } else {
            SB_LOG(ERROR) << "An error happened while writing to socket: "
                          << ToString(err);
            return false;
          }
          break;
        } else if (result == 0) {
          // Socket has closed.
          return false;
        } else {
          // Expected condition. Partial or full write was successful.
          size_t bytes_written = static_cast<size_t>(result);
          SB_DCHECK(bytes_written <= bytes_to_write);
          curr_write_block.erase(0, bytes_written);
        }
      }
    }
    return true;
  }

  int32_t blocked_counts() const { return blocked_counts_.load(); }

 private:
  bool TransferData(size_t max_size, std::string* destination) {
    ScopedLock lock_log(log_mutex_);
    size_t log_size = log_.size();
    if (log_size == 0) {
      return false;
    }

    size_t size = std::min<size_t>(max_size, log_size);
    std::deque<char>::iterator begin_it = log_.begin();
    std::deque<char>::iterator end_it = begin_it;
    std::advance(end_it, size);

    destination->assign(begin_it, end_it);
    log_.erase(begin_it, end_it);
    return true;
  }

  void PrependData(const std::string& curr_write_block) {
    ScopedLock lock_log(log_mutex_);
    log_.insert(log_.begin(), curr_write_block.begin(), curr_write_block.end());
  }

  int max_memory_buffer_size_;
  int chunk_size_;
  Mutex log_mutex_;
  std::deque<char> log_;
  atomic_int32_t blocked_counts_;
};

// This class will listen to the provided socket for a client
// connection. When a client connection is established, a
// callback will be invoked.
class SocketListener {
 public:
  typedef std::function<void(scoped_ptr<Socket>)> Callback;

  SocketListener(Socket* listen_socket, Callback cb)
      : listen_socket_(listen_socket), callback_(cb) {
    auto run_cb = [this](Semaphore* sema) { this->Run(sema); };
    thread_ = FunctionThread::Create("NetLogSocketListener", run_cb);
    thread_->Start();
  }

  ~SocketListener() { thread_->Join(); }

 private:
  void Run(Semaphore* joined_sema) {
    while (!joined_sema->TakeWait(100'000)) {
      scoped_ptr<Socket> client_connection(listen_socket_->Accept());

      if (client_connection) {
        callback_(client_connection.Pass());
        break;
      }
    }
  }

  Socket* listen_socket_;
  Callback callback_;
  scoped_ptr<Thread> thread_;
};

class NetLogServer {
 public:
  static NetLogServer* Instance();
  NetLogServer()
      : buffered_socket_writer_(NET_LOG_MAX_IN_MEMORY_BUFFER,
                                NET_LOG_SOCKET_SEND_SIZE) {
    ScopedLock lock(socket_mutex_);
    listen_socket_ = CreateListenSocket();
    ListenForClient();

    writer_thread_ = FunctionThread::Create(
        "NetLogSocketWriter",
        [this](Semaphore* sema) { this->WriterTask(sema); });
    writer_thread_->Start();
  }

  ~NetLogServer() {
    SB_NOTREACHED();  // Should never reach here because of singleton use.
    Close();
  }

  void ListenForClient() {
    SocketListener::Callback cb =
        std::bind(&NetLogServer::OnClientConnect, this, std::placeholders::_1);
    socket_listener_.reset();
    socket_listener_.reset(new SocketListener(listen_socket_.get(), cb));
  }

  void OnClientConnect(scoped_ptr<Socket> client_socket) {
    ScopedLock lock(socket_mutex_);
    client_socket_ = client_socket.Pass();
    client_socket_->SetSendBufferSize(NET_LOG_SOCKET_BUFFER_SIZE);
    client_socket_->SetTcpKeepAlive(true, 1'000'000);
  }

  // Has a client ever connected?
  bool HasClientConnected() {
    ScopedLock lock(socket_mutex_);
    return client_socket_;
  }

  void OnLog(const char* msg) {
    buffered_socket_writer_.Append(msg, strlen(msg));
    writer_thread_sema_.Put();
  }

  void Close() {
    socket_listener_.reset();
    if (writer_thread_) {
      is_joined_.store(true);
      writer_thread_sema_.Put();
      writer_thread_->Join();
      writer_thread_.reset(nullptr);
      Flush();  // One last flush to the socket.
    }
    ScopedLock lock(socket_mutex_);
    client_socket_.reset();
    listen_socket_.reset();
  }

  // Return |true| if the data was written out to a connected socket,
  // else |false| if
  // 1. There was no connected client.
  // 2. The connection was dropped.
  // 3. Some other connection error happened.
  bool Flush() {
    ScopedLock lock(socket_mutex_);
    if (!client_socket_) {
      return false;
    }
    return buffered_socket_writer_.Flush(client_socket_->socket());
  }

 private:
  void WriterTask(Semaphore* joined_sema) {
    while (true) {
      writer_thread_sema_.Take();

      if (HasClientConnected()) {
        if (!Flush()) {
          break;  // Connection dropped.
        }
      }
      if (is_joined_.load()) {
        break;
      }
    }
  }

  scoped_ptr<Socket> listen_socket_;
  scoped_ptr<Socket> client_socket_;
  Mutex socket_mutex_;

  scoped_ptr<SocketListener> socket_listener_;
  scoped_ptr<Thread> writer_thread_;
  Semaphore writer_thread_sema_;
  atomic_bool is_joined_;

  BufferedSocketWriter buffered_socket_writer_;
};

class ThreadLocalBoolean {
 public:
  ThreadLocalBoolean() : slot_(SbThreadCreateLocalKey(NULL)) {}
  ~ThreadLocalBoolean() { SbThreadDestroyLocalKey(slot_); }

  void Set(bool val) {
    void* ptr = val ? reinterpret_cast<void*>(0x1) : nullptr;
    SbThreadSetLocalValue(slot_, ptr);
  }

  bool Get() const {
    void* ptr = SbThreadGetLocalValue(slot_);
    return ptr != nullptr;
  }

 private:
  SbThreadLocalKey slot_;

  ThreadLocalBoolean(const ThreadLocalBoolean&) = delete;
  void operator=(const ThreadLocalBoolean&) = delete;
};

SB_ONCE_INITIALIZE_FUNCTION(NetLogServer, NetLogServer::Instance);
SB_ONCE_INITIALIZE_FUNCTION(ThreadLocalBoolean, ScopeGuardTLB);

// Prevents re-entrant behavior for sending logs. This is useful if/when
// sub-routines will invoke logging on an error condition.
class ScopeGuard {
 public:
  ScopeGuard() : disabled_(false), tlb_(ScopeGuardTLB()) {
    disabled_ = tlb_->Get();
    tlb_->Set(true);
  }

  ~ScopeGuard() { tlb_->Set(disabled_); }

  bool IsEnabled() { return !disabled_; }

 private:
  bool disabled_;
  ThreadLocalBoolean* tlb_;
};

}  // namespace.

const char kNetLogCommandSwitchWait[] = "net_log_wait_for_connection";

void NetLogWaitForClientConnected(int64_t timeout) {
#if !SB_LOGGING_IS_OFFICIAL_BUILD
  ScopeGuard guard;
  if (guard.IsEnabled()) {
    int64_t expire_time = (timeout >= 0) && (timeout < kSbInt64Max)
                              ? CurrentMonotonicTime() + timeout
                              : kSbInt64Max;
    while (true) {
      if (NetLogServer::Instance()->HasClientConnected()) {
        break;
      }
      if (CurrentMonotonicTime() > expire_time) {
        break;
      }
      SbThreadSleep(1000);
    }
  }
#endif
}

void NetLogWrite(const char* data) {
#if !SB_LOGGING_IS_OFFICIAL_BUILD
  ScopeGuard guard;
  if (guard.IsEnabled()) {
    NetLogServer::Instance()->OnLog(data);
  }
#endif
}

void NetLogFlush() {
#if !SB_LOGGING_IS_OFFICIAL_BUILD
  ScopeGuard guard;
  if (guard.IsEnabled()) {
    NetLogServer::Instance()->Flush();
  }
#endif
}

void NetLogFlushThenClose() {
#if !SB_LOGGING_IS_OFFICIAL_BUILD
  ScopeGuard guard;
  if (guard.IsEnabled()) {
    NetLogServer* net_log = NetLogServer::Instance();
    net_log->OnLog("Netlog is closing down\n");
    net_log->Close();
  }
#endif
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
