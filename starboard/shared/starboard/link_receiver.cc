// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/link_receiver.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/socket.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/posix/handle_eintr.h"
#include "starboard/shared/posix/set_non_blocking_internal.h"
#include "starboard/shared/posix/socket_internal.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/socket_waiter.h"
#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace starboard {

namespace {

// Creates a socket that is appropriate for binding and listening, but is not
// bound and hasn't started listening yet.
int CreateServerSocket(SbSocketAddressType address_type) {
  int socket_fd;
  switch (address_type) {
    case kSbSocketAddressTypeIpv4:
      socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      break;
    case kSbSocketAddressTypeIpv6:
      socket_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
      break;
    default:
      SB_NOTREACHED();
      errno = EAFNOSUPPORT;
      return -1;
  }

  if (socket_fd < 0) {
    return -1;
  }

  // All Starboard sockets are non-blocking, so let's ensure it.
  if (!posix::SetNonBlocking(socket_fd)) {
    // Something went wrong, we'll clean up (preserving errno) and return
    // failure.
    HANDLE_EINTR(close(socket_fd));
    return errno;
  }

#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
  // Use SO_NOSIGPIPE to mute SIGPIPE on darwin systems.
  int optval_set = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE,
             reinterpret_cast<void*>(&optval_set), sizeof(int));
#endif

  if (socket_fd < 0) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "Socket create failed, errno: " << errno;
    return -1;
  }

  int on = 1;
  int result = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if (result != 0) {
    SB_LOG(ERROR) << "Failed to set SO_REUSEADDR on socket " << socket_fd
                  << ", errno = " << errno;
    return -1;
  }

  return socket_fd;
}

// Creates a server socket that is bound to the loopback interface.
int CreateLocallyBoundSocket(SbSocketAddressType address_type, int port) {
  int socket_fd = CreateServerSocket(address_type);
  if (socket_fd < 0) {
    return -1;
  }

  SbSocketAddress address = {};
  bool success = GetLocalhostAddress(address_type, port, &address);
  if (!success) {
    SB_LOG(ERROR) << "GetLocalhostAddress failed";
    return -1;
  }

  posix::SockAddr sock_addr;
  if (!sock_addr.FromSbSocketAddress(&address)) {
    SB_LOG(ERROR) << __FUNCTION__ << ": Invalid address";
    return -1;
  }

  SB_DCHECK(socket_fd >= 0);
  SbSocketAddress* local_address = &address;
  // When binding to the IPV6 any address, ensure that the IPV6_V6ONLY flag is
  // off to allow incoming IPV4 connections on the same socket.
  // See https://www.ietf.org/rfc/rfc3493.txt for details.
  if (local_address && (local_address->type == kSbSocketAddressTypeIpv6) &&
      common::MemoryIsZero(local_address->address, 16)) {
    int on = 0;
    if (setsockopt(socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) !=
        0) {
      // Silently ignore errors, assume the default behavior is as expected.
      errno = 0;
    }
  }

  int result =
      HANDLE_EINTR(bind(socket_fd, sock_addr.sockaddr(), sock_addr.length));
  if (result != 0) {
    SB_LOG(ERROR) << __FUNCTION__ << ": Bind failed. errno=" << errno;
    return -1;
  }
  return socket_fd;
}

// Creates a server socket that is bound and listening to the loopback interface
// on the given port.
int CreateListeningSocket(SbSocketAddressType address_type, int port) {
  int socket_fd = CreateLocallyBoundSocket(address_type, port);
  if (socket_fd < 0) {
    return -1;
  }

#if defined(SOMAXCONN)
  const int kMaxConn = SOMAXCONN;
#else
  const int kMaxConn = 128;
#endif
  int result = listen(socket_fd, kMaxConn);
  if (result != 0) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "SbSocketListen failed: " << result;
    return -1;
  }

  return socket_fd;
}

// Gets the port socket is bound to.
bool GetBoundPort(int socket, int* out_port) {
  SB_DCHECK(out_port);
  SB_DCHECK(socket);

  SbSocketAddress socket_address = {0};
  posix::SockAddr sock_addr;
  int result = getsockname(socket, sock_addr.sockaddr(), &sock_addr.length);
  if (result < 0) {
    return false;
  }
  if (!sock_addr.ToSbSocketAddress(&socket_address)) {
    return false;
  }

  *out_port = socket_address.port;
  return true;
}

std::string GetTemporaryDirectory() {
  const int kMaxPathLength = kSbFileMaxPath;
  std::unique_ptr<char[]> temp_path(new char[kMaxPathLength]);
  bool has_temp = SbSystemGetPath(kSbSystemPathTempDirectory, temp_path.get(),
                                  kMaxPathLength);
  if (!has_temp) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "No temporary directory.";
    return "";
  }

  return std::string(temp_path.get());
}

// Writes |size| bytes of |contents| to the file at |name|.
void CreateTemporaryFile(const char* name, const char* contents, int size) {
  std::string path = GetTemporaryDirectory();
  if (path.empty()) {
    return;
  }

  path += kSbFileSepString;
  path += name;
  ScopedFile file(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY);
  if (!file.IsValid()) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "Unable to create: " << path;
    return;
  }

  file.WriteAll(contents, size);
  file.Flush();
}
}  // namespace

// PImpl of LinkReceiver.
class LinkReceiver::Impl {
 public:
  Impl(Application* application, int port);
  ~Impl();

 private:
  // Encapsulates connection state.
  struct Connection {
    explicit Connection(int socket_fd) { socket = std::move(socket_fd); }
    ~Connection() {}
    void FlushLink(Application* application) {
      if (!data.empty()) {
        application->Link(data.c_str());
        data.clear();
      }
    }

    int socket;
    std::string data;
  };

  // Runs the server, waiting on an SbSocketWaiter, and blocking until shut
  // down.
  void Run();

  // Adds |socket| to the SbSocketWaiter to wait until ready for accepting a new
  // connection.
  bool AddForAccept(int socket);

  // Adds the |connection| to the SbSocketWaiter to wait until ready to read
  // more data.
  bool AddForRead(Connection* connection);

  // Called when the listening socket has a connection available to accept.
  void OnAcceptReady();

  // Called when the waiter reports that a socket has more data to read.
  void OnReadReady(int socket_fd);

  // Called when the waiter reports that a connection has more data to read.
  void OnReadReady(Connection* connection);

  // Thread entry point.
  static void* RunThread(void* context);

  // SbSocketWaiter entry points.
  static void HandleAccept(SbSocketWaiter waiter,
                           int socket,
                           void* context,
                           int ready_interests);
  static void HandleRead(SbSocketWaiter waiter,
                         int socket,
                         void* context,
                         int ready_interests);

  // The application to dispatch Link() calls to.
  Application* application_;

  // The port that was specified by the constructor.
  const int specified_port_;

  // The port that was queried off of the bound socket.
  int actual_port_;

  // The thread owned by this server.
  pthread_t thread_;

  // An atomic flag that indicates whether to quit to the server thread.
  std::atomic_bool quit_{false};

  // The waiter to register sockets with and block on.
  SbSocketWaiter waiter_;

  // A semaphore that will be signaled by the internal thread once the waiter
  // has been initialized, so the external thread can safely wake up the waiter.
  Semaphore waiter_initialized_;

  // A semaphore that will be signaled by the external thread indicating that it
  // will no longer reference the waiter, so that the internal thread can safely
  // destroy the waiter.
  Semaphore destroy_waiter_;

  // The server socket listening for new connections.
  int listen_socket_;

  // A map of raw SbSockets to Connection objects.
  std::unordered_map<int, Connection*> connections_;
};

LinkReceiver::Impl::Impl(Application* application, int port)
    : application_(application),
      specified_port_(port),
      thread_(0),
      waiter_(kSbSocketWaiterInvalid) {
  pthread_create(&thread_, nullptr, &LinkReceiver::Impl::RunThread, this);

  // Block until waiter is set.
  waiter_initialized_.Take();
}

LinkReceiver::Impl::~Impl() {
  SB_DCHECK(!pthread_equal(thread_, pthread_self()));
  quit_.store(true);
  if (SbSocketWaiterIsValid(waiter_)) {
    SbSocketWaiterWakeUp(waiter_);
  }
  destroy_waiter_.Put();
  pthread_join(thread_, NULL);
}

void LinkReceiver::Impl::Run() {
  waiter_ = SbSocketWaiterCreate();
  if (!SbSocketWaiterIsValid(waiter_)) {
    SB_LOG(WARNING) << "Unable to create SbSocketWaiter.";
    waiter_initialized_.Put();
    return;
  }
  listen_socket_ = -1;

  listen_socket_ =
      CreateListeningSocket(kSbSocketAddressTypeIpv4, specified_port_);
  if (listen_socket_ < 0) {
    listen_socket_ =
        CreateListeningSocket(kSbSocketAddressTypeIpv6, specified_port_);
  }
  if (listen_socket_ < 0) {
    SB_LOG(WARNING) << "Unable to start LinkReceiver on port "
                    << specified_port_ << ".";
    SbSocketWaiterDestroy(waiter_);
    waiter_ = kSbSocketWaiterInvalid;
    waiter_initialized_.Put();
    return;
  }

  actual_port_ = 0;
  bool result = GetBoundPort(listen_socket_, &actual_port_);
  if (!result) {
    SB_LOG(WARNING) << "Unable to get LinkReceiver bound port.";
    SbSocketWaiterDestroy(waiter_);
    waiter_ = kSbSocketWaiterInvalid;
    waiter_initialized_.Put();
    return;
  }

  char port_string[32] = {0};
  snprintf(port_string, SB_ARRAY_SIZE(port_string), "%d", actual_port_);
  CreateTemporaryFile("link_receiver_port", port_string, strlen(port_string));

  if (!AddForAccept(listen_socket_)) {
    quit_.store(true);
  }

  waiter_initialized_.Put();
  while (!quit_.load()) {
    SbSocketWaiterWait(waiter_);
  }

  for (auto& entry : connections_) {
    SbPosixSocketWaiterRemove(waiter_, entry.first);
    delete entry.second;
  }
  connections_.clear();

  SbPosixSocketWaiterRemove(waiter_, listen_socket_);

  // Block until destroying thread will no longer reference waiter.
  destroy_waiter_.Take();
  SbSocketWaiterDestroy(waiter_);
}

bool LinkReceiver::Impl::AddForAccept(int socket) {
  if (!SbPosixSocketWaiterAdd(waiter_, socket, this,
                              &LinkReceiver::Impl::HandleAccept,
                              kSbSocketWaiterInterestRead, true)) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "SbSocketWaiterAdd failed.";
    return false;
  }
  return true;
}

bool LinkReceiver::Impl::AddForRead(Connection* connection) {
  if (!SbPosixSocketWaiterAdd(waiter_, connection->socket, this,
                              &LinkReceiver::Impl::HandleRead,
                              kSbSocketWaiterInterestRead, false)) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "SbSocketWaiterAdd failed.";
    return false;
  }
  return true;
}

void LinkReceiver::Impl::OnAcceptReady() {
  SB_DCHECK(listen_socket_ >= 0);

  int accepted_socket = HANDLE_EINTR(accept(listen_socket_, NULL, NULL));
  if (accepted_socket < 0) {
    return;
  }

  // All Starboard sockets are non-blocking, so let's ensure it.
  if (!posix::SetNonBlocking(accepted_socket)) {
    // Something went wrong, we'll clean up and return failure.
    HANDLE_EINTR(close(accepted_socket));
    return;
  }

  SB_DCHECK(accepted_socket);
  Connection* connection = new Connection(std::move(accepted_socket));
  connections_.emplace(connection->socket, connection);
  AddForRead(connection);
}

void LinkReceiver::Impl::OnReadReady(int socket_fd) {
  auto iter = connections_.find(socket_fd);
  SB_DCHECK(iter != connections_.end());
  OnReadReady(iter->second);
}

void LinkReceiver::Impl::OnReadReady(Connection* connection) {
  int socket = connection->socket;

  char data[64] = {0};
  int read = -1;

  const int kRecvFlags = 0;

  SB_DCHECK(socket >= 0);

  ssize_t bytes_read = recv(socket, data, SB_ARRAY_SIZE_INT(data), kRecvFlags);
  if (bytes_read >= 0) {
    read = static_cast<int>(bytes_read);
  }

  int last_null = 0;
  for (int position = 0; position < read; ++position) {
    if (data[position] == '\0' || data[position] == '\n' ||
        data[position] == '\r') {
      int length = position - last_null;
      if (length) {
        connection->data.append(&data[last_null], length);
        connection->FlushLink(application_);
      }
      last_null = position + 1;
      continue;
    }
  }

  int remainder = read - last_null;
  if (remainder > 0) {
    connection->data.append(&data[last_null], remainder);
  }

  if (read == 0) {
    // Terminate connection.
    connection->FlushLink(application_);
    connections_.erase(socket);
    delete connection;
    return;
  }

  AddForRead(connection);
}

// static
void* LinkReceiver::Impl::RunThread(void* context) {
  SB_DCHECK(context);
  pthread_setname_np(pthread_self(), "LinkReceiver");
  reinterpret_cast<LinkReceiver::Impl*>(context)->Run();
  return NULL;
}

// static
void LinkReceiver::Impl::HandleAccept(SbSocketWaiter waiter,
                                      int socket,
                                      void* context,
                                      int ready_interests) {
  SB_DCHECK(context);
  reinterpret_cast<LinkReceiver::Impl*>(context)->OnAcceptReady();
}

// static
void LinkReceiver::Impl::HandleRead(SbSocketWaiter waiter,
                                    int socket,
                                    void* context,
                                    int ready_interests) {
  SB_DCHECK(context);
  reinterpret_cast<LinkReceiver::Impl*>(context)->OnReadReady(socket);
}

LinkReceiver::LinkReceiver(Application* application)
    : impl_(new Impl(application, 0)) {}

LinkReceiver::LinkReceiver(Application* application, int port)
    : impl_(new Impl(application, port)) {}

LinkReceiver::~LinkReceiver() {
  delete impl_;
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
