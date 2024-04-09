// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

// We specifically do not include <sys/socket.h> since the define causes a loop

#include <fcntl.h>
#include <io.h>      // Needed for file-specific `_close`.
#include <unistd.h>  // Our version that declares generic `close`.
#include <winsock2.h>
#undef NO_ERROR  // http://b/302733082#comment15
#include <ws2tcpip.h>
#include <map>
#include "starboard/common/log.h"
#include "starboard/types.h"

static int gen_fd() {
  static int fd = 100;
  fd++;
  if (fd == 0x7FFFFFFF) {
    fd = 100;
  }
  return fd;
}

struct FileOrSocket {
  bool is_file;
  int file;
  SOCKET socket;
};

struct CriticalSection {
  CriticalSection() { InitializeCriticalSection(&critical_section_); }
  CRITICAL_SECTION critical_section_;
};

static std::map<int, FileOrSocket>* g_map_addr = nullptr;
static CriticalSection g_critical_section;

int handle_db_put(FileOrSocket handle) {
  EnterCriticalSection(&g_critical_section.critical_section_);
  if (g_map_addr == nullptr) {
    g_map_addr = new std::map<int, FileOrSocket>();
  }

  int fd = gen_fd();
  // Go through the map and make sure there isn't duplicated index
  // already.
  while (g_map_addr->find(fd) != g_map_addr->end()) {
    fd = gen_fd();
  }
  g_map_addr->insert({fd, handle});

  LeaveCriticalSection(&g_critical_section.critical_section_);
  return fd;
}

static FileOrSocket handle_db_get(int fd, bool erase) {
  FileOrSocket invalid_handle = {/*is_file=*/false, -1, INVALID_SOCKET};
  if (fd < 0) {
    _set_errno(EBADF);
    return invalid_handle;
  }
  EnterCriticalSection(&g_critical_section.critical_section_);
  if (g_map_addr == nullptr) {
    g_map_addr = new std::map<int, FileOrSocket>();
    _set_errno(EBADF);
    return invalid_handle;
  }

  auto itr = g_map_addr->find(fd);
  if (itr == g_map_addr->end()) {
    _set_errno(EBADF);
    return invalid_handle;
  }

  FileOrSocket handle = itr->second;
  if (erase) {
    g_map_addr->erase(fd);
  }
  LeaveCriticalSection(&g_critical_section.critical_section_);
  return handle;
}

// WSAGetLastError should be called immediately to retrieve the extended error
// code for the failing function call.
// https://learn.microsoft.com/en-us/windows/win32/winsock/error-codes-errno-h-errno-and-wsagetlasterror-2
static void set_errno() {
  int winsockError = WSAGetLastError();
  int sockError = 0;

  // The error codes returned by Windows Sockets are similar to UNIX socket
  // error code constants, but the constants are all prefixed with WSA. So in
  // Winsock applications the WSAEWOULDBLOCK error code would be returned, while
  // in UNIX applications the EWOULDBLOCK error code would be returned. The
  // errno values in a WIN32 are a subset of the values for errno in UNIX
  // systems.
  switch (winsockError) {
    case WSAEINTR:  // Interrupted function call
      sockError = EINTR;
      break;
    case WSAEBADF:  // WSAEBADF
      sockError = EBADF;
      break;
    case WSAEACCES:  // WSAEACCES
      sockError = EACCES;
      break;
    case WSAEFAULT:  // Bad address
      sockError = EFAULT;
      break;
    case WSAEINVAL:  // Invalid argument
      sockError = EINVAL;
      break;
    case WSAEMFILE:  // Too many open files
      sockError = EMFILE;
      break;
    case WSAEWOULDBLOCK:  // Operation would block
      sockError = EWOULDBLOCK;
      break;
    case WSAEINPROGRESS:  // Operation now in progress
      sockError = EINPROGRESS;
      break;
    case WSAEALREADY:  // Operation already in progress
      sockError = EALREADY;
      break;
    case WSAENOTSOCK:  // Socket operation on non-socket
      sockError = ENOTSOCK;
      break;
    case WSAEDESTADDRREQ:  // Destination address required
      sockError = EDESTADDRREQ;
      break;
    case WSAEMSGSIZE:  // Message too long
      sockError = EMSGSIZE;
      break;
    case WSAEPROTOTYPE:  // Protocol wrong type for socket
      sockError = EPROTOTYPE;
      break;
    case WSAENOPROTOOPT:  // Bad protocol option
      sockError = ENOPROTOOPT;
      break;
    case WSAEPROTONOSUPPORT:  // Protocol not supported
      sockError = EPROTONOSUPPORT;
      break;
    case WSAEOPNOTSUPP:  // Operation not supported
      sockError = EOPNOTSUPP;
      break;
    case WSAEAFNOSUPPORT:  // Address family not supported by protocol family
      sockError = EAFNOSUPPORT;
      break;
    case WSAEADDRINUSE:  // Address already in use
      sockError = EADDRINUSE;
      break;
    case WSAEADDRNOTAVAIL:  // Cannot assign requested address
      sockError = EADDRNOTAVAIL;
      break;
    case WSAENETDOWN:  // Network is down
      sockError = ENETDOWN;
      break;
    case WSAENETUNREACH:  // Network is unreachable
      sockError = ENETUNREACH;
      break;
    case WSAENETRESET:  // Network dropped connection on reset
      sockError = ENETRESET;
      break;
    case WSAECONNABORTED:  // Software caused connection abort
      sockError = ECONNABORTED;
      break;
    case WSAECONNRESET:  // Connection reset by peer
      sockError = ECONNRESET;
      break;
    case WSAENOBUFS:  // No buffer space available
      sockError = ENOBUFS;
      break;
    case WSAEISCONN:  // Socket is already connected
      sockError = EISCONN;
      break;
    case WSAENOTCONN:  // Socket is not connected
      sockError = ENOTCONN;
      break;
    case WSAETIMEDOUT:  // Connection timed out
      sockError = ETIMEDOUT;
      break;
    case WSAECONNREFUSED:  // Connection refused
      sockError = ECONNREFUSED;
      break;
    case WSAELOOP:  // WSAELOOP
      sockError = ELOOP;
      break;
    case WSAENAMETOOLONG:  // WSAENAMETOOLONG
      sockError = ENAMETOOLONG;
      break;
    case WSAEHOSTUNREACH:  // No route to host
      sockError = EHOSTUNREACH;
      break;
    case WSAENOTEMPTY:  // WSAENOTEMPTY
      sockError = ENOTEMPTY;
      break;
    case WSAHOST_NOT_FOUND:  // Host not found
      sockError = HOST_NOT_FOUND;
      break;
    case WSATRY_AGAIN:  // Non-authoritative host not found
      sockError = TRY_AGAIN;
      break;
    case WSANO_RECOVERY:  // This is a non-recoverable error
      sockError = NO_RECOVERY;
      break;
    case WSANO_DATA:  // Valid name, no data record of requested type
      sockError = NO_DATA;
      break;
    default:
      SB_DLOG(WARNING) << "Unknown socket error.";
      break;
  }

  _set_errno(0);
  _set_errno(sockError);
  SB_DLOG(INFO) << "Encounter socket error: " << sockError;
}

///////////////////////////////////////////////////////////////////////////////
// Implementations below exposed externally in pure C for emulation.
///////////////////////////////////////////////////////////////////////////////

extern "C" {

int sb_socket(int domain, int type, int protocol) {
  // Sockets on Windows do not use *nix-style file descriptors
  // socket() returns a handle to a kernel object instead
  SOCKET socket_handle = socket(domain, type, protocol);
  if (socket_handle == INVALID_SOCKET) {
    set_errno();
    return -1;
  }

  FileOrSocket handle = {/*is_file=*/false, -1, socket_handle};

  return handle_db_put(handle);
}

int open(const char* path, int oflag, ...) {
  va_list args;
  va_start(args, oflag);
  int fd;
  mode_t mode;
  if (oflag & O_CREAT) {
    mode = va_arg(args, mode_t);
    fd = _open(path, oflag, mode & MS_MODE_MASK);
  } else {
    fd = _open(path, oflag);
  }
  va_end(args);

  if (fd < 0) {
    return fd;
  }

  FileOrSocket handle = {/*is_file=*/true, fd, INVALID_SOCKET};
  return handle_db_put(handle);
}

int close(int fd) {
  FileOrSocket handle = handle_db_get(fd, true);

  if (!handle.is_file && handle.socket == INVALID_SOCKET) {
    return -1;
  } else if (!handle.is_file) {
    int result = closesocket(handle.socket);
    if (result == SOCKET_ERROR) {
      set_errno();
    }
    return result;
  }

  // This is then a file handle, so use Windows `_close` API.
  return _close(handle.file);
}

int sb_bind(int socket, const struct sockaddr* address, socklen_t address_len) {
  SOCKET socket_handle = handle_db_get(socket, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    return -1;
  }

  int result = bind(socket_handle, address, address_len);
  if (result == SOCKET_ERROR) {
    set_errno();
  }
  return result;
}

int sb_listen(int socket, int backlog) {
  SOCKET socket_handle = handle_db_get(socket, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    return -1;
  }

  int result = listen(socket_handle, backlog);
  if (result == SOCKET_ERROR) {
    set_errno();
  }
  return result;
}

int sb_accept(int socket, sockaddr* addr, int* addrlen) {
  SOCKET socket_handle = handle_db_get(socket, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    return -1;
  }

  SOCKET accept_handle = accept(socket_handle, addr, addrlen);
  if (accept_handle == INVALID_SOCKET) {
    set_errno();
    return -1;
  }

  FileOrSocket handle = {/*is_file=*/false, -1, accept_handle};
  return handle_db_put(handle);
}

int sb_connect(int socket, sockaddr* name, int namelen) {
  SOCKET socket_handle = handle_db_get(socket, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    return -1;
  }

  int result = connect(socket_handle, name, namelen);
  if (result == SOCKET_ERROR) {
    set_errno();
  }
  return result;
}

int sb_send(int sockfd, const void* buf, size_t len, int flags) {
  SOCKET socket_handle = handle_db_get(sockfd, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    return -1;
  }

  int result =
      send(socket_handle, reinterpret_cast<const char*>(buf), len, flags);
  if (result == SOCKET_ERROR) {
    set_errno();
  }
  return result;
}

int sb_recv(int sockfd, void* buf, size_t len, int flags) {
  SOCKET socket_handle = handle_db_get(sockfd, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    return -1;
  }

  int result = recv(socket_handle, reinterpret_cast<char*>(buf), len, flags);
  if (result == SOCKET_ERROR) {
    set_errno();
  }
  return result;
}

int sb_sendto(int sockfd,
              const void* buf,
              size_t len,
              int flags,
              const struct sockaddr* dest_addr,
              socklen_t dest_len) {
  SOCKET socket_handle = handle_db_get(sockfd, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    return -1;
  }

  int result = sendto(socket_handle, reinterpret_cast<const char*>(buf), len,
                      flags, dest_addr, dest_len);
  if (result == SOCKET_ERROR) {
    set_errno();
  }
  return result;
}

int sb_recvfrom(int sockfd,
                void* buf,
                size_t len,
                int flags,
                struct sockaddr* address,
                socklen_t* address_len) {
  SOCKET socket_handle = handle_db_get(sockfd, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    return -1;
  }

  int result = recvfrom(socket_handle, reinterpret_cast<char*>(buf), len, flags,
                        address, address_len);
  if (result == SOCKET_ERROR) {
    set_errno();
  }
  return result;
}

int sb_setsockopt(int socket,
                  int level,
                  int option_name,
                  const void* option_value,
                  int option_len) {
  FileOrSocket handle = handle_db_get(socket, false);

  if (handle.is_file || handle.socket == INVALID_SOCKET) {
    return -1;
  }

  int result =
      setsockopt(handle.socket, level, option_name,
                 reinterpret_cast<const char*>(option_value), option_len);
  if (result == SOCKET_ERROR) {
    set_errno();
  }
  return result;
}

int sb_fcntl(int fd, int cmd, ... /*arg*/) {
  SOCKET socket_handle = handle_db_get(fd, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    return -1;
  }

  if (cmd == F_SETFL) {
    va_list ap;
    int arg;
    va_start(ap, cmd);
    arg = va_arg(ap, int);
    va_end(ap);
    if ((arg & O_NONBLOCK) == O_NONBLOCK) {
      int opt = 1;
      ioctlsocket(socket_handle, FIONBIO, reinterpret_cast<u_long*>(&opt));
    }
  }
  return 0;
}

int* sb_errno_location() {
  return &errno;
}

}  // extern "C"
