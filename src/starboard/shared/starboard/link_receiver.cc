// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <string>
#include <unordered_map>

#include "starboard/atomic.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/common/semaphore.h"
#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/string.h"
#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace starboard {

namespace {
// Returns an address that means bind to any interface on the given |port|. When
// |port| is zero, it means the system should choose the port.
SbSocketAddress GetUnspecifiedAddress(SbSocketAddressType address_type,
                                      int port) {
  SbSocketAddress address = {0};
  address.type = address_type;
  address.port = port;
  return address;
}

// Returns an address that means bind to the loopback interface on the given
// |port|. When |port| is zero, it means the system should choose the port.
SbSocketAddress GetLocalhostAddress(SbSocketAddressType address_type,
                                    int port) {
  SbSocketAddress address = GetUnspecifiedAddress(address_type, port);
  switch (address_type) {
    case kSbSocketAddressTypeIpv4: {
      address.address[0] = 127;
      address.address[3] = 1;
      return address;
    }
    case kSbSocketAddressTypeIpv6: {
      address.address[15] = 1;
      return address;
    }
  }
  SB_LOG(ERROR) << __FUNCTION__ << ": unknown address type: " << address_type;
  return address;
}

// Creates a socket that is appropriate for binding and listening, but is not
// bound and hasn't started listening yet.
scoped_ptr<Socket> CreateServerSocket(SbSocketAddressType address_type) {
  scoped_ptr<Socket> socket(new Socket(address_type));
  if (!socket->IsValid()) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "SbSocketCreate failed";
    return scoped_ptr<Socket>().Pass();
  }

  if (!socket->SetReuseAddress(true)) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "SbSocketSetReuseAddress failed";
    return scoped_ptr<Socket>().Pass();
  }

  return socket.Pass();
}

// Creates a server socket that is bound to the loopback interface.
scoped_ptr<Socket> CreateLocallyBoundSocket(SbSocketAddressType address_type,
                                            int port) {
  scoped_ptr<Socket> socket = CreateServerSocket(address_type);
  if (!socket) {
    return scoped_ptr<Socket>().Pass();
  }

  SbSocketAddress address = GetLocalhostAddress(address_type, port);
  SbSocketError result = socket->Bind(&address);
  if (result != kSbSocketOk) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "SbSocketBind to " << port << " failed: " << result;
    return scoped_ptr<Socket>().Pass();
  }

  return socket.Pass();
}

// Creates a server socket that is bound and listening to the loopback interface
// on the given port.
scoped_ptr<Socket> CreateListeningSocket(SbSocketAddressType address_type,
                                         int port) {
  scoped_ptr<Socket> socket = CreateLocallyBoundSocket(address_type, port);
  if (!socket) {
    return scoped_ptr<Socket>().Pass();
  }

  SbSocketError result = socket->Listen();
  if (result != kSbSocketOk) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "SbSocketListen failed: " << result;
    return scoped_ptr<Socket>().Pass();
  }

  return socket.Pass();
}

// Gets the port socket is bound to.
bool GetBoundPort(Socket* socket, int* out_port) {
  SB_DCHECK(out_port);
  SB_DCHECK(socket);

  SbSocketAddress socket_address = {0};
  bool result = socket->GetLocalAddress(&socket_address);
  if (!result) {
    return false;
  }

  *out_port = socket_address.port;
  return true;
}

std::string GetTemporaryDirectory() {
  const int kMaxPathLength = SB_FILE_MAX_PATH;
  scoped_array<char> temp_path(new char[kMaxPathLength]);
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

  path += SB_FILE_SEP_STRING;
  path += name;
  ScopedFile file(path.c_str(), kSbFileCreateAlways | kSbFileWrite);
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
    explicit Connection(scoped_ptr<Socket> socket) : socket(socket.Pass()) {}
    ~Connection() {}
    void FlushLink(Application* application) {
      if (!data.empty()) {
        application->Link(data.c_str());
        data.clear();
      }
    }

    scoped_ptr<Socket> socket;
    std::string data;
  };

  // Runs the server, waiting on an SbSocketWaiter, and blocking until shut
  // down.
  void Run();

  // Adds |socket| to the SbSocketWaiter to wait until ready for accepting a new
  // connection.
  bool AddForAccept(Socket* socket);

  // Adds the |connection| to the SbSocketWaiter to wait until ready to read
  // more data.
  bool AddForRead(Connection* connection);

  // Called when the listening socket has a connection available to accept.
  void OnAcceptReady();

  // Called when the waiter reports that a socket has more data to read.
  void OnReadReady(SbSocket sb_socket);

  // Called when the waiter reports that a connection has more data to read.
  void OnReadReady(Connection* connection);

  // Thread entry point.
  static void* RunThread(void* context);

  // SbSocketWaiter entry points.
  static void HandleAccept(SbSocketWaiter waiter,
                           SbSocket socket,
                           void* context,
                           int ready_interests);
  static void HandleRead(SbSocketWaiter waiter,
                         SbSocket socket,
                         void* context,
                         int ready_interests);

  // The application to dispatch Link() calls to.
  Application* application_;

  // The port that was specified by the constructor.
  const int specified_port_;

  // The port that was queried off of the bound socket.
  int actual_port_;

  // The thread owned by this server.
  SbThread thread_;

  // An atomic flag that indicates whether to quit to the server thread.
  atomic_bool quit_;

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
  scoped_ptr<Socket> listen_socket_;

  // A map of raw SbSockets to Connection objects.
  std::unordered_map<SbSocket, Connection*> connections_;
};

LinkReceiver::Impl::Impl(Application* application, int port)
    : application_(application),
      specified_port_(port),
      thread_(kSbThreadInvalid),
      waiter_(kSbSocketWaiterInvalid) {
  thread_ =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     "LinkReceiver", &LinkReceiver::Impl::RunThread, this);

  // Block until waiter is set.
  waiter_initialized_.Take();
}

LinkReceiver::Impl::~Impl() {
  SB_DCHECK(!SbThreadIsEqual(thread_, SbThreadGetCurrent()));
  quit_.store(true);
  if (SbSocketWaiterIsValid(waiter_)) {
    SbSocketWaiterWakeUp(waiter_);
  }
  destroy_waiter_.Put();
  SbThreadJoin(thread_, NULL);
}

void LinkReceiver::Impl::Run() {
  waiter_ = SbSocketWaiterCreate();
  if (!SbSocketWaiterIsValid(waiter_)) {
    SB_LOG(WARNING) << "Unable to create SbSocketWaiter.";
    waiter_initialized_.Put();
    return;
  }

  listen_socket_ =
      CreateListeningSocket(kSbSocketAddressTypeIpv4, specified_port_);
  if (!listen_socket_ || !listen_socket_->IsValid()) {
      listen_socket_ = CreateListeningSocket(kSbSocketAddressTypeIpv6,
                                             specified_port_);
  }
  if (!listen_socket_ || !listen_socket_->IsValid()) {
    SB_LOG(WARNING) << "Unable to start LinkReceiver on port "
                    << specified_port_ << ".";
    SbSocketWaiterDestroy(waiter_);
    waiter_ = kSbSocketWaiterInvalid;
    waiter_initialized_.Put();
    return;
  }

  actual_port_ = 0;
  bool result = GetBoundPort(listen_socket_.get(), &actual_port_);
  if (!result) {
    SB_LOG(WARNING) << "Unable to get LinkReceiver bound port.";
    SbSocketWaiterDestroy(waiter_);
    waiter_ = kSbSocketWaiterInvalid;
    waiter_initialized_.Put();
    return;
  }

  char port_string[32] = {0};
  SbStringFormatF(port_string, SB_ARRAY_SIZE(port_string), "%d", actual_port_);
  CreateTemporaryFile("link_receiver_port", port_string,
                      SbStringGetLength(port_string));

  if (!AddForAccept(listen_socket_.get())) {
    quit_.store(true);
  }

  waiter_initialized_.Put();
  while (!quit_.load()) {
    SbSocketWaiterWait(waiter_);
  }

  for (auto& entry : connections_) {
    SbSocketWaiterRemove(waiter_, entry.first);
    delete entry.second;
  }
  connections_.clear();

  SbSocketWaiterRemove(waiter_, listen_socket_->socket());

  // Block until destroying thread will no longer reference waiter.
  destroy_waiter_.Take();
  SbSocketWaiterDestroy(waiter_);
}

bool LinkReceiver::Impl::AddForAccept(Socket* socket) {
  if (!SbSocketWaiterAdd(waiter_, socket->socket(), this,
                         &LinkReceiver::Impl::HandleAccept,
                         kSbSocketWaiterInterestRead, true)) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "SbSocketWaiterAdd failed.";
    return false;
  }
  return true;
}

bool LinkReceiver::Impl::AddForRead(Connection* connection) {
  if (!SbSocketWaiterAdd(waiter_, connection->socket->socket(), this,
                         &LinkReceiver::Impl::HandleRead,
                         kSbSocketWaiterInterestRead, false)) {
    SB_LOG(ERROR) << __FUNCTION__ << ": "
                  << "SbSocketWaiterAdd failed.";
    return false;
  }
  return true;
}

void LinkReceiver::Impl::OnAcceptReady() {
  scoped_ptr<Socket> accepted_socket =
      make_scoped_ptr(listen_socket_->Accept());
  SB_DCHECK(accepted_socket);
  Connection* connection = new Connection(accepted_socket.Pass());
  connections_.emplace(connection->socket->socket(), connection);
  AddForRead(connection);
}

void LinkReceiver::Impl::OnReadReady(SbSocket sb_socket) {
  auto iter = connections_.find(sb_socket);
  SB_DCHECK(iter != connections_.end());
  OnReadReady(iter->second);
}

void LinkReceiver::Impl::OnReadReady(Connection* connection) {
  auto socket = connection->socket.get();

  char data[64] = {0};
  int read = socket->ReceiveFrom(data, SB_ARRAY_SIZE_INT(data), NULL);
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
    connections_.erase(socket->socket());
    delete connection;
    return;
  }

  AddForRead(connection);
}

// static
void* LinkReceiver::Impl::RunThread(void* context) {
  SB_DCHECK(context);
  reinterpret_cast<LinkReceiver::Impl*>(context)->Run();
  return NULL;
}

// static
void LinkReceiver::Impl::HandleAccept(SbSocketWaiter /*waiter*/,
                                      SbSocket /*socket*/,
                                      void* context,
                                      int ready_interests) {
  SB_DCHECK(context);
  reinterpret_cast<LinkReceiver::Impl*>(context)->OnAcceptReady();
}

// static
void LinkReceiver::Impl::HandleRead(SbSocketWaiter /*waiter*/,
                                    SbSocket socket,
                                    void* context,
                                    int /*ready_interests*/) {
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
