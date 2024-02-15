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

#include <io.h>
#include <time.h>
#include <winsock2.h>
#undef NO_ERROR  // http://b/302733082#comment15
#include <ws2tcpip.h>
#include <map>
#include "starboard/types.h"

int gen_fd() {
  static int fd = 100;
  fd++;
  if (fd == 0x7FFFFFFF) {
    fd = 100;
  }
  return fd;
}

struct CriticalSection {
  CriticalSection() { InitializeCriticalSection(&critical_section_); }
  CRITICAL_SECTION critical_section_;
};

static std::map<int, SOCKET>* g_map_addr = nullptr;
static CriticalSection g_critical_section;

static int handle_db_put(SOCKET socket_handle) {
  EnterCriticalSection(&g_critical_section.critical_section_);
  if (g_map_addr == nullptr) {
    g_map_addr = new std::map<int, SOCKET>();
  }

  int fd = gen_fd();
  // Go through the map and make sure there isn't duplicated index
  // already.
  while (g_map_addr->find(fd) != g_map_addr->end()) {
    fd = gen_fd();
  }

  g_map_addr->insert({fd, socket_handle});
  LeaveCriticalSection(&g_critical_section.critical_section_);
  return fd;
}

static SOCKET handle_db_get(int fd, bool erase) {
  if (fd < 0) {
    return INVALID_SOCKET;
  }
  EnterCriticalSection(&g_critical_section.critical_section_);
  if (g_map_addr == nullptr) {
    g_map_addr = new std::map<int, SOCKET>();
    return INVALID_SOCKET;
  }

  auto itr = g_map_addr->find(fd);
  if (itr == g_map_addr->end()) {
    return INVALID_SOCKET;
  }

  SOCKET socket_handle = itr->second;
  if (erase) {
    g_map_addr->erase(fd);
  }
  LeaveCriticalSection(&g_critical_section.critical_section_);
  return socket_handle;
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

  return handle_db_put(socket_handle);
}

int close(int fd) {
  SOCKET socket_handle = handle_db_get(fd, true);

  if (socket_handle != INVALID_SOCKET) {
    return closesocket(socket_handle);
  }

  return _close(fd);
}

int sb_bind(int socket, const struct sockaddr* address, socklen_t address_len) {
  SOCKET socket_handle = handle_db_get(socket, false);
  if (socket_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }

  return bind(socket_handle, address, address_len);
}

int sb_listen(int socket, int backlog) {
  SOCKET socket_handle = handle_db_get(socket, false);
  if (socket_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }

  return listen(socket_handle, backlog);
}

int sb_accept(int socket, sockaddr* addr, int* addrlen) {
  SOCKET socket_handle = handle_db_get(socket, false);
  if (socket_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }

  SOCKET accept_handle = accept(socket_handle, addr, addrlen);
  if (accept_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }
  return handle_db_put(accept_handle);
}

int sb_connect(int socket, sockaddr* name, int namelen) {
  SOCKET socket_handle = handle_db_get(socket, false);
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
                  socklen_t option_len) {
  SOCKET socket_handle = handle_db_get(socket, false);
  if (socket_handle == INVALID_SOCKET) {
    // TODO: update errno with file operation error
    return -1;
  }
  return setsockopt(socket_handle, level, option_name,
                    reinterpret_cast<const char*>(option_value),
                    static_cast<int>(option_len));
}

}  // extern "C"
