// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/socket.h"

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>

#include "starboard/common/log.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

SbSocketResolution* SbSocketResolve(const char* hostname, int filters) {
  struct addrinfo* ai = NULL;
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
  int err = getaddrinfo(hostname, NULL, &hints, &ai);
  if (err != 0 || ai == NULL) {
    return NULL;
  }

  int address_count = 0;
  for (const struct addrinfo* i = ai; i != NULL; i = i->ai_next) {
    ++address_count;
  }

  SbSocketResolution* result = new SbSocketResolution();

  // Translate all the sockaddrs.
  sbposix::SockAddr* sock_addrs = new sbposix::SockAddr[address_count];
  bool* parsed = new bool[address_count];
  int index = 0;
  int skip = 0;
  for (const struct addrinfo *i = ai; i != NULL; i = i->ai_next, ++index) {
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

  delete[] parsed;
  delete[] sock_addrs;
  freeaddrinfo(ai);
  return result;
}
