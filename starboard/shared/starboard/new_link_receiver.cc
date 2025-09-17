// Standalone version of Starboard's LinkReceiver.
// This implementation uses Linux-specific APIs (epoll, eventfd) and C++ STL,
// and has no dependency on the Starboard library.

#include "starboard/shared/starboard/new_link_receiver.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// A simple logging macro to replace SB_LOG.
#define LOG_INFO(msg) std::cout << "INFO: " << msg << std::endl
#define LOG_WARNING(msg) std::cerr << "WARNING: " << msg << std::endl
#define LOG_ERROR(msg) \
  std::cerr << "ERROR: " << msg << " (errno: " << errno << ")" << std::endl

// A simple assertion macro to replace SB_DCHECK.
#define ASSERT(condition) assert(condition)

// A wrapper to handle EINTR for syscalls.
template <typename Func>
int HANDLE_EINTR(Func func) {
  int result;
  do {
    result = func();
  } while (result == -1 && errno == EINTR);
  return result;
}

class LinkReceiverImpl {
 public:
  LinkReceiverImpl(LinkCallback link_callback, int port);
  ~LinkReceiverImpl();

 private:
  // Encapsulates connection state.
  struct Connection {
    explicit Connection(int socket_fd) : socket(socket_fd) {}
    ~Connection() {
      if (socket >= 0) {
        close(socket);
      }
    }

    void FlushLink(LinkCallback link_callback) {
      if (!data.empty()) {
        link_callback(data.c_str());
        data.clear();
      }
    }

    int socket;
    std::string data;
  };

  void Run();
  static void* RunThread(void* context);

  void OnAcceptReady();
  void OnReadReady(Connection* connection);
  void AddToEpoll(int fd, void* data_ptr);
  void RemoveFromEpoll(int fd);

  // Helper functions
  static bool SetNonBlocking(int fd);
  static int CreateListeningSocket(int port, int address_family);
  static bool GetBoundPort(int socket, int* out_port);
  static void CreateTemporaryFile(const char* name,
                                  const char* contents,
                                  int size);
  static std::string GetTemporaryDirectory();

  LinkCallback link_callback_;
  const int specified_port_;
  int actual_port_;
  pthread_t thread_;
  std::atomic_bool quit_{false};

  int listen_socket_ = -1;
  int epoll_fd_ = -1;
  int event_fd_ = -1;  // Used to wake up the epoll_wait() call

  // Map of file descriptors to Connection objects.
  std::unordered_map<int, std::unique_ptr<Connection>> connections_;

  sem_t server_started_sem_;
};

// --- LinkReceiver Implementation ---

LinkReceiver::LinkReceiver(LinkCallback link_callback)
    : impl_(new LinkReceiverImpl(link_callback, 0)) {}

LinkReceiver::LinkReceiver(LinkCallback link_callback, int port)
    : impl_(new LinkReceiverImpl(link_callback, port)) {}

LinkReceiver::~LinkReceiver() {
  // The unique_ptr will handle deletion of impl_.
}

// --- LinkReceiverImpl Implementation ---

LinkReceiverImpl::LinkReceiverImpl(LinkCallback link_callback, int port)
    : link_callback_(link_callback), specified_port_(port), thread_(0) {
  sem_init(&server_started_sem_, 0, 0);
  pthread_create(&thread_, nullptr, &LinkReceiverImpl::RunThread, this);
  // Block until the server thread is initialized.
  sem_wait(&server_started_sem_);
}

LinkReceiverImpl::~LinkReceiverImpl() {
  ASSERT(thread_ != pthread_self());
  quit_.store(true);

  // Wake up the epoll_wait() call by writing to the eventfd.
  if (event_fd_ >= 0) {
    uint64_t val = 1;
    HANDLE_EINTR([&]() { return write(event_fd_, &val, sizeof(val)); });
  }

  pthread_join(thread_, NULL);
  sem_destroy(&server_started_sem_);
}

void* LinkReceiverImpl::RunThread(void* context) {
  pthread_setname_np(pthread_self(), "LinkReceiver");
  reinterpret_cast<LinkReceiverImpl*>(context)->Run();
  return NULL;
}

void LinkReceiverImpl::Run() {
  // Try IPv4 first, then IPv6.
  listen_socket_ = CreateListeningSocket(specified_port_, AF_INET);
  if (listen_socket_ < 0) {
    listen_socket_ = CreateListeningSocket(specified_port_, AF_INET6);
  }

  if (listen_socket_ < 0) {
    LOG_WARNING("Unable to start LinkReceiver on port " << specified_port_);
    sem_post(&server_started_sem_);  // Unblock constructor
    return;
  }

  if (!GetBoundPort(listen_socket_, &actual_port_)) {
    LOG_WARNING("Unable to get LinkReceiver bound port.");
    close(listen_socket_);
    listen_socket_ = -1;
    sem_post(&server_started_sem_);  // Unblock constructor
    return;
  }

  char port_string[32] = {0};
  snprintf(port_string, sizeof(port_string), "%d", actual_port_);
  CreateTemporaryFile("link_receiver_port", port_string, strlen(port_string));

  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) {
    LOG_ERROR("epoll_create1() failed");
    close(listen_socket_);
    listen_socket_ = -1;
    sem_post(&server_started_sem_);
    return;
  }

  event_fd_ = eventfd(0, EFD_NONBLOCK);
  if (event_fd_ < 0) {
    LOG_ERROR("eventfd() failed");
    close(listen_socket_);
    close(epoll_fd_);
    listen_socket_ = -1;
    epoll_fd_ = -1;
    sem_post(&server_started_sem_);
    return;
  }

  // Add listen socket and eventfd to epoll.
  // We use the listen_socket_ itself as the data pointer for identification.
  AddToEpoll(listen_socket_, &listen_socket_);
  AddToEpoll(event_fd_, &event_fd_);

  sem_post(&server_started_sem_);  // Unblock constructor, server is ready.

  const int kMaxEvents = 16;
  struct epoll_event events[kMaxEvents];

  while (!quit_.load()) {
    int num_events = HANDLE_EINTR(
        [&]() { return epoll_wait(epoll_fd_, events, kMaxEvents, -1); });

    if (num_events < 0) {
      LOG_ERROR("epoll_wait() failed");
      break;
    }

    if (quit_.load()) {
      break;
    }

    for (int i = 0; i < num_events; ++i) {
      void* event_ptr = events[i].data.ptr;

      if (event_ptr == &listen_socket_) {
        // New connection
        OnAcceptReady();
      } else if (event_ptr == &event_fd_) {
        // Wakeup signal
        uint64_t val;
        HANDLE_EINTR([&]() { return read(event_fd_, &val, sizeof(val)); });
        // Break from the loop to check quit_ flag.
        goto shutdown;
      } else {
        // Data on existing connection
        Connection* connection = static_cast<Connection*>(event_ptr);
        if (events[i].events & (EPOLLHUP | EPOLLERR)) {
          RemoveFromEpoll(connection->socket);
          connections_.erase(connection->socket);
        } else if (events[i].events & EPOLLIN) {
          OnReadReady(connection);
          // OnReadReady might close the connection.
          if (connections_.find(connection->socket) == connections_.end()) {
            // The connection was removed inside OnReadReady.
          }
        }
      }
    }
  }

shutdown:
  // Cleanup
  connections_.clear();  // Destructors will close sockets.
  if (listen_socket_ >= 0) {
    close(listen_socket_);
  }
  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
  }
  if (event_fd_ >= 0) {
    close(event_fd_);
  }
}

void LinkReceiverImpl::AddToEpoll(int fd, void* data_ptr) {
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.ptr = data_ptr;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
    LOG_ERROR("epoll_ctl(ADD) failed");
  }
}

void LinkReceiverImpl::RemoveFromEpoll(int fd) {
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, NULL) < 0) {
    LOG_ERROR("epoll_ctl(DEL) failed");
  }
}

void LinkReceiverImpl::OnAcceptReady() {
  while (true) {
    int accepted_socket =
        HANDLE_EINTR([&]() { return accept(listen_socket_, NULL, NULL); });
    if (accepted_socket < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No more pending connections.
        break;
      }
      LOG_ERROR("accept() failed");
      break;
    }

    if (!SetNonBlocking(accepted_socket)) {
      close(accepted_socket);
      continue;
    }

    auto connection = std::make_unique<Connection>(accepted_socket);
    // Store the raw pointer in the epoll event data.
    // The unique_ptr in the map owns the memory.
    void* conn_ptr = connection.get();
    connections_.emplace(accepted_socket, std::move(connection));
    AddToEpoll(accepted_socket, conn_ptr);
  }
}

void LinkReceiverImpl::OnReadReady(Connection* connection) {
  char buffer[1024];
  bool connection_closed = false;

  while (true) {
    ssize_t bytes_read = HANDLE_EINTR(
        [&]() { return recv(connection->socket, buffer, sizeof(buffer), 0); });

    if (bytes_read < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        LOG_ERROR("recv() failed");
        connection_closed = true;
      }
      break;  // No more data to read now.
    }

    if (bytes_read == 0) {
      // Connection closed by peer.
      connection_closed = true;
      break;
    }

    // Process the received data.
    int last_null = 0;
    for (int i = 0; i < bytes_read; ++i) {
      if (buffer[i] == '\0' || buffer[i] == '\n' || buffer[i] == '\r') {
        int length = i - last_null;
        if (length > 0) {
          connection->data.append(&buffer[last_null], length);
        }
        connection->FlushLink(link_callback_);
        last_null = i + 1;
      }
    }

    int remainder = bytes_read - last_null;
    if (remainder > 0) {
      connection->data.append(&buffer[last_null], remainder);
    }
  }

  if (connection_closed) {
    connection->FlushLink(link_callback_);
    RemoveFromEpoll(connection->socket);
    connections_.erase(connection->socket);
  }
}

// --- Static Helper Functions ---

bool LinkReceiverImpl::SetNonBlocking(int fd) {
  int flags = HANDLE_EINTR([&]() { return fcntl(fd, F_GETFL, 0); });
  if (flags == -1) {
    LOG_ERROR("fcntl(F_GETFL) failed");
    return false;
  }
  if (HANDLE_EINTR([&]() { return fcntl(fd, F_SETFL, flags | O_NONBLOCK); }) ==
      -1) {
    LOG_ERROR("fcntl(F_SETFL, O_NONBLOCK) failed");
    return false;
  }
  return true;
}

int LinkReceiverImpl::CreateListeningSocket(int port, int address_family) {
  int sock_fd = socket(address_family, SOCK_STREAM, IPPROTO_TCP);
  if (sock_fd < 0) {
    LOG_ERROR("socket() failed");
    return -1;
  }

  if (!SetNonBlocking(sock_fd)) {
    close(sock_fd);
    return -1;
  }

  int on = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
    LOG_ERROR("setsockopt(SO_REUSEADDR) failed");
    close(sock_fd);
    return -1;
  }

#if defined(SO_NOSIGPIPE)
  // Use SO_NOSIGPIPE to mute SIGPIPE on Darwin/BSD systems.
  // On Linux, we can use MSG_NOSIGNAL flag with send/recv.
  setsockopt(sock_fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif

  // Bind to localhost.
  if (address_family == AF_INET) {
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
      LOG_ERROR("bind() failed for IPv4");
      close(sock_fd);
      return -1;
    }
  } else if (address_family == AF_INET6) {
    struct sockaddr_in6 addr = {};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_loopback;
    // Also accept IPv4 connections on this IPv6 socket.
    int off = 0;
    setsockopt(sock_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
      LOG_ERROR("bind() failed for IPv6");
      close(sock_fd);
      return -1;
    }
  } else {
    LOG_ERROR("Unsupported address family");
    close(sock_fd);
    return -1;
  }

  const int kMaxConn = 128;
  if (listen(sock_fd, kMaxConn) != 0) {
    LOG_ERROR("listen() failed");
    close(sock_fd);
    return -1;
  }

  return sock_fd;
}

bool LinkReceiverImpl::GetBoundPort(int socket, int* out_port) {
  ASSERT(out_port);
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (getsockname(socket, (struct sockaddr*)&addr, &len) < 0) {
    LOG_ERROR("getsockname() failed");
    return false;
  }

  if (addr.ss_family == AF_INET) {
    *out_port = ntohs(((struct sockaddr_in*)&addr)->sin_port);
  } else if (addr.ss_family == AF_INET6) {
    *out_port = ntohs(((struct sockaddr_in6*)&addr)->sin6_port);
  } else {
    return false;
  }
  return true;
}

std::string LinkReceiverImpl::GetTemporaryDirectory() {
  const char* tmpdir = getenv("TMPDIR");
  if (tmpdir && tmpdir[0]) {
    return tmpdir;
  }
  return "/tmp";
}

void LinkReceiverImpl::CreateTemporaryFile(const char* name,
                                           const char* contents,
                                           int size) {
  std::string path = GetTemporaryDirectory();
  if (path.empty()) {
    return;
  }
  path += "/";
  path += name;

  FILE* file = fopen(path.c_str(), "w");
  if (!file) {
    LOG_ERROR("Unable to create temporary file: " << path);
    return;
  }

  fwrite(contents, 1, size, file);
  fclose(file);
}

// --- Example Usage ---
/*
void MyLinkCallback(const char* link_data) {
    printf("Received link: %s\n", link_data);
}

int main() {
    printf("Starting link receiver...\n");
    LinkReceiver receiver(MyLinkCallback, 45678);

    printf("Link receiver running. Press Enter to exit.\n");
    getchar();

    printf("Shutting down link receiver...\n");
    // Destructor of 'receiver' will be called here, shutting down the server.
    return 0;
}
*/
