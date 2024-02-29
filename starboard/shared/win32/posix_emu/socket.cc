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
    return invalid_handle;
  }
  EnterCriticalSection(&g_critical_section.critical_section_);
  if (g_map_addr == nullptr) {
    g_map_addr = new std::map<int, FileOrSocket>();
    return invalid_handle;
  }

  auto itr = g_map_addr->find(fd);
  if (itr == g_map_addr->end()) {
    return invalid_handle;
  }

  FileOrSocket handle = itr->second;
  if (erase) {
    g_map_addr->erase(fd);
  }
  LeaveCriticalSection(&g_critical_section.critical_section_);
  return handle;
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
    // TODO: update errno with file operation error
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

  if (!handle.is_file) {
    return closesocket(handle.socket);
  }

  // This is then a file handle, so use Windows `_close` API.
  return _close(handle.file);
}

int sb_bind(int socket, const struct sockaddr* address, socklen_t address_len) {
  SOCKET socket_handle = handle_db_get(socket, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }

  return bind(socket_handle, address, address_len);
}

int sb_listen(int socket, int backlog) {
  SOCKET socket_handle = handle_db_get(socket, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }

  return listen(socket_handle, backlog);
}

int sb_accept(int socket, sockaddr* addr, int* addrlen) {
  SOCKET socket_handle = handle_db_get(socket, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }

  SOCKET accept_handle = accept(socket_handle, addr, addrlen);
  if (accept_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }

  FileOrSocket handle = {/*is_file=*/false, -1, accept_handle};
  return handle_db_put(handle);
}

int sb_connect(int socket, sockaddr* name, int namelen) {
  SOCKET socket_handle = handle_db_get(socket, false).socket;
  if (socket_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }

  return connect(socket_handle, name, namelen);
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
  // TODO(b/321999529): Windows returns SOCKET_ERROR on failure. The specific
  // error code can be retrieved by calling WSAGetLastError(), and Posix returns
  // -1 on failure and sets errno to the error’s value.
  if (result == SOCKET_ERROR) {
    return -1;
  }
  return 0;
}
}  // extern "C"
