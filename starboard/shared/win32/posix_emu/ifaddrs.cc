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

#include <ifaddrs.h>
#include <io.h>
#include <stdio.h>
#include <time.h>
#include <winsock2.h>

void freeifaddrs(struct ifaddrs* ifa) {
  struct ifaddrs* ptr = ifa;
  struct ifaddrs* last_ptr = ifa;
  while (ptr != nullptr) {
    if (ptr->ifa_addr != nullptr) {
      delete ptr->ifa_addr;
      ptr->ifa_addr = nullptr;
    }
    ptr = ptr->ifa_next;
    delete last_ptr;
    last_ptr = ptr;
  }
}

int getifaddrs(struct ifaddrs** ifap) {
  char ac[80];
  struct ifaddrs* ifaddr_ptr = nullptr;
  struct ifaddrs* last_ifaddr_ptr = nullptr;

  if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
    return -1;
  }

  struct hostent* phe = gethostbyname(ac);
  if (phe == 0) {
    return -1;
  }

  for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
    ifaddr_ptr = new struct ifaddrs;
    memset(ifaddr_ptr, 0, sizeof(struct ifaddrs));
    if (i == 0) {
      *ifap = ifaddr_ptr;
    }
    if (last_ifaddr_ptr != nullptr) {
      last_ifaddr_ptr->ifa_next = ifaddr_ptr;
    }
    last_ifaddr_ptr = ifaddr_ptr;

    ifaddr_ptr->ifa_addr = new struct sockaddr;
    struct sockaddr_in in_addr = {0};
    memcpy(&in_addr.sin_addr, phe->h_addr_list[i], sizeof(in_addr.sin_addr));
    in_addr.sin_family = AF_INET;
    memcpy(ifaddr_ptr->ifa_addr, &in_addr, sizeof(sockaddr));

    ifaddr_ptr->ifa_next = nullptr;
  }

  return 0;
}
