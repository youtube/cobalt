// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_socketpair_abi_wrappers.h"

#include <errno.h>
#include <sys/socket.h>

#include "starboard/common/log.h"
#include "starboard/shared/modular/starboard_layer_posix_socket_abi_wrappers.h"

namespace {

int musl_domain_to_platform_domain(int domain) {
  switch (domain) {
    case MUSL_AF_UNIX:
      return AF_UNIX;
    // Report EOPNOTSUPP for the unsupported domains that are more likely to be
    // tried.
    case MUSL_AF_INET:
    case MUSL_AF_INET6:
      errno = EOPNOTSUPP;
      break;
    default:
      errno = EAFNOSUPPORT;
      break;
  }

  SB_LOG(WARNING) << "Only AF_UNIX is supported for domain, got: " << domain;
  return -1;
}

// Deconstructs, converts, and reconstructs the type, which contains a socket
// type and optionally a bitwise-inclusive OR of flags.
// Returns -1, signifying an error, if type contains unknown bits or does not
// contain a supported socket type.
int musl_type_to_platform_type(int type) {
  int platform_flags = 0;
  if (type & MUSL_SOCK_CLOEXEC) {
    platform_flags |= SOCK_CLOEXEC;
  }
  if (type & MUSL_SOCK_NONBLOCK) {
    platform_flags |= SOCK_NONBLOCK;
  }

  int known_flags_mask = MUSL_SOCK_CLOEXEC | MUSL_SOCK_NONBLOCK;
  int musl_socket_type = type & ~known_flags_mask;
  int platform_socket_type = 0;

  switch (musl_socket_type) {
    case MUSL_SOCK_STREAM:
      platform_socket_type = SOCK_STREAM;
      break;
    case MUSL_SOCK_SEQPACKET:
      platform_socket_type = SOCK_SEQPACKET;
      break;
    default:
      errno = EPROTOTYPE;
      SB_LOG(WARNING) << "Unable to convert musl socket type to platform "
                      << "socket type, or unknown flags are present. Value: "
                      << musl_socket_type;
      return -1;
  }

  return platform_socket_type | platform_flags;
}

int musl_protocol_to_platform_protocol(int protocol) {
  if (protocol) {
    errno = EPROTONOSUPPORT;
    SB_LOG(WARNING) << "Only the default protocol is supported, got "
                    << protocol;
    return -1;
  }

  return 0;
}

}  // namespace

SB_EXPORT int __abi_wrap_socketpair(int domain,
                                    int type,
                                    int protocol,
                                    int socket_vector[2]) {
  // errno reporting is handled by the conversion helper functions.
  int platform_domain = musl_domain_to_platform_domain(domain);
  if (platform_domain == -1) {
    return -1;
  }
  int platform_type = musl_type_to_platform_type(type);
  if (platform_type == -1) {
    return -1;
  }
  int platform_protocol = musl_protocol_to_platform_protocol(protocol);
  if (platform_protocol == -1) {
    return -1;
  }

  return socketpair(platform_domain, platform_type, platform_protocol,
                    socket_vector);
}
