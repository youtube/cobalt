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

#include "starboard/socket.h"

#include <winsock2.h>

#include <memory>
#include <vector>

#include "starboard/log.h"
#include "starboard/shared/win32/socket_internal.h"

namespace sbwin32 = starboard::shared::win32;

SbSocketResolution* SbSocketResolve(const char* hostname, int filters) {
  struct addrinfo* ai = nullptr;
  struct addrinfo hints = {0};

  if (filters & kSbSocketResolveFilterIpv4) {
    if (filters & kSbSocketResolveFilterIpv6) {
      hints.ai_family = AF_UNSPEC;
    } else {
      hints.ai_family = AF_INET;
    }
  } else if (filters & kSbSocketResolveFilterIpv6) {
    hints.ai_family = AF_INET6;
  } else {
    hints.ai_family = AF_UNSPEC;
  }

  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_socktype = SOCK_STREAM;

  // Actually make the call to get the data.
  int err = getaddrinfo(hostname, nullptr, &hints, &ai);
  if (err != 0 || ai == nullptr) {
    SB_DLOG(ERROR) << "getaddrinfo failed.  last_error: " << WSAGetLastError();
    return nullptr;
  }

  int address_count = 0;
  for (const auto* i = ai; i != nullptr; i = i->ai_next) {
    ++address_count;
  }

  SbSocketResolution* result = new SbSocketResolution();

  // Translate all the sockaddrs.
  std::vector<sbwin32::SockAddr> sock_addrs;
  sock_addrs.resize(address_count);

  std::vector<bool> parsed;
  parsed.resize(address_count);

  int index = 0;
  int skip = 0;
  for (const auto *i = ai; i != nullptr; i = i->ai_next, ++index) {
    // Skip over any addresses we can't parse.
    parsed[index] = sock_addrs[index].FromSockaddr(i->ai_addr);
    if (!parsed[index]) {
      ++skip;
    }
  }

  result->address_count = address_count - skip;
  result->addresses = new SbSocketAddress[result->address_count];

  int result_index = 0;
  for (int i = 0; i < address_count; ++i) {
    if (parsed[i] &&
        sock_addrs[i].ToSbSocketAddress(&result->addresses[result_index])) {
      ++result_index;
    }
  }

  freeaddrinfo(ai);
  return result;
}
