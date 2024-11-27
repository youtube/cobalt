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

#include <stdlib.h>
#include <sys/socket.h>

extern "C" {

int __abi_wrap_accept(int sockfd,
                      struct sockaddr* addr,
                      socklen_t* addrlen_ptr);

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen_ptr) {
  return __abi_wrap_accept(sockfd, addr, addrlen_ptr);
}

int __abi_wrap_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  return __abi_wrap_bind(sockfd, addr, addrlen);
}

int __abi_wrap_connect(int sockfd,
                       const struct sockaddr* addr,
                       socklen_t addrlen);
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  return __abi_wrap_connect(sockfd, addr, addrlen);
}

int __abi_wrap_getaddrinfo(const char* node,
                           const char* service,
                           const struct addrinfo* hints,
                           struct addrinfo** res);
int getaddrinfo(const char* node,
                const char* service,
                const struct addrinfo* hints,
                struct addrinfo** res) {
  return __abi_wrap_getaddrinfo(node, service, hints, res);
}

void __abi_wrap_freeaddrinfo(struct addrinfo* ai);
void freeaddrinfo(struct addrinfo* ai) {
  return __abi_wrap_freeaddrinfo(ai);
}

int __abi_wrap_getifaddrs(struct ifaddrs** ifap);
int getifaddrs(struct ifaddrs** ifap) {
  return __abi_wrap_getifaddrs(ifap);
}

int __abi_wrap_setsockopt(int socket,
                          int level,
                          int option_name,
                          const void* option_value,
                          socklen_t option_len);
int setsockopt(int socket,
               int level,
               int option_name,
               const void* option_value,
               socklen_t option_len) {
  return __abi_wrap_setsockopt(socket, level, option_name, option_value,
                               option_len);
}

}  // extern "C"
