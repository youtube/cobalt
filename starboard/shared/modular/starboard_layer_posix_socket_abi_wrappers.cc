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

#include "starboard/shared/modular/starboard_layer_posix_socket_abi_wrappers.h"
#include <stdlib.h>
#include <string.h>

#include "starboard/common/log.h"

namespace {
// Corresponding arrays to for musl<->platform translation.
int MUSL_AI_ORDERED[] = {
    MUSL_AI_PASSIVE, MUSL_AI_CANONNAME,  MUSL_AI_NUMERICHOST, MUSL_AI_V4MAPPED,
    MUSL_AI_ALL,     MUSL_AI_ADDRCONFIG, MUSL_AI_NUMERICSERV,
};
int PLATFORM_AI_ORDERED[] = {
    AI_PASSIVE, AI_CANONNAME,  AI_NUMERICHOST, AI_V4MAPPED,
    AI_ALL,     AI_ADDRCONFIG, AI_NUMERICSERV,
};
// Corresponding arrays to for musl<->platform translation.
int MUSL_SOCK_ORDERED[] = {
    MUSL_SOCK_STREAM,
    MUSL_SOCK_DGRAM,
    MUSL_SOCK_RAW,
};
int PLATFORM_SOCK_ORDERED[] = {
    SOCK_STREAM,
    SOCK_DGRAM,
    SOCK_RAW,
};
// Corresponding arrays to for musl<->platform translation.
int MUSL_IPPROTO_ORDERED[] = {
    MUSL_IPPROTO_TCP,
    MUSL_IPPROTO_UDP,
};
int PLATFORM_IPPROTO_ORDERED[] = {
    IPPROTO_TCP,
    IPPROTO_UDP,
};
// Corresponding arrays to for musl<->platform translation.
int MUSL_AF_ORDERED[] = {
    MUSL_AF_INET,
    MUSL_AF_INET6,
};
int PLATFORM_AF_ORDERED[] = {
    AF_INET,
    AF_INET6,
};

int musl_shuts_to_platform_shuts(int how) {
  switch (how) {
    case MUSL_SHUT_RD:
      return SHUT_RD;
    case MUSL_SHUT_WR:
      return SHUT_WR;
    case MUSL_SHUT_RDWR:
      return SHUT_RDWR;
    default:
      SB_LOG(WARNING) << "Unable to convert musl flag to platform flag, "
                      << "using value as-is.";
      return how;
  }
}

int musl_errcodes_to_platform_errcodes(int ecode) {
  switch (ecode) {
    case MUSL_EAI_BADFLAGS:
      return EAI_BADFLAGS;
    case MUSL_EAI_NONAME:
      return EAI_NONAME;
    case MUSL_EAI_AGAIN:
      return EAI_AGAIN;
    case MUSL_EAI_FAIL:
      return EAI_FAIL;
    case MUSL_EAI_NODATA:
      return EAI_NODATA;
    case MUSL_EAI_FAMILY:
      return EAI_FAMILY;
    case MUSL_EAI_SOCKTYPE:
      return EAI_SOCKTYPE;
    case MUSL_EAI_SERVICE:
      return EAI_SERVICE;
    case MUSL_EAI_MEMORY:
      return EAI_MEMORY;
    case MUSL_EAI_SYSTEM:
      return EAI_SYSTEM;
    case MUSL_EAI_OVERFLOW:
      return EAI_OVERFLOW;
    default:
      // If the errcode cannot be converted, we can leave the ecode as is,
      // as gai_strerror() will just return a string stating that an unknown
      // ecode was given. A warning log is left to let the user know
      // a conversion was not possible.
      SB_LOG(WARNING) << "Unable to convert musl errcode to platform errcode, "
                         "using value as-is.";
      return ecode;
  }
}

int musl_hints_to_platform_hints(const struct musl_addrinfo* hints,
                                 struct addrinfo* platform_hints) {
  int ai_left = hints->ai_flags;
  for (int i = 0; i < sizeof(MUSL_AI_ORDERED) / sizeof(int); i++) {
    if (hints->ai_flags & MUSL_AI_ORDERED[i]) {
      platform_hints->ai_flags |= PLATFORM_AI_ORDERED[i];
      ai_left &= ~MUSL_AI_ORDERED[i];
    }
  }
  for (int i = 0; i < sizeof(MUSL_SOCK_ORDERED) / sizeof(int); i++) {
    if (hints->ai_socktype == MUSL_SOCK_ORDERED[i]) {
      platform_hints->ai_socktype = PLATFORM_SOCK_ORDERED[i];
    }
  }
  for (int i = 0; i < sizeof(MUSL_IPPROTO_ORDERED) / sizeof(int); i++) {
    if (hints->ai_protocol == MUSL_IPPROTO_ORDERED[i]) {
      platform_hints->ai_protocol = PLATFORM_IPPROTO_ORDERED[i];
    }
  }
  for (int i = 0; i < sizeof(MUSL_AF_ORDERED) / sizeof(int); i++) {
    if (hints->ai_family == MUSL_AF_ORDERED[i]) {
      platform_hints->ai_family = PLATFORM_AF_ORDERED[i];
    }
  }
  if (ai_left != 0 ||
      (hints->ai_socktype != 0 && platform_hints->ai_socktype == 0) ||
      (hints->ai_protocol != 0 && platform_hints->ai_protocol == 0) ||
      (hints->ai_family != 0 && platform_hints->ai_family == 0)) {
    return -1;
  }
  return 0;
}

int platform_hints_to_musl_hints(const struct addrinfo* hints,
                                 struct musl_addrinfo* musl_hints) {
  int ai_left = hints->ai_flags;
  for (int i = 0; i < sizeof(PLATFORM_AI_ORDERED) / sizeof(int); i++) {
    if (hints->ai_flags & PLATFORM_AI_ORDERED[i]) {
      musl_hints->ai_flags |= MUSL_AI_ORDERED[i];
      ai_left &= ~PLATFORM_AI_ORDERED[i];
    }
  }
  for (int i = 0; i < sizeof(PLATFORM_SOCK_ORDERED) / sizeof(int); i++) {
    if (hints->ai_socktype == PLATFORM_SOCK_ORDERED[i]) {
      musl_hints->ai_socktype = MUSL_SOCK_ORDERED[i];
    }
  }
  for (int i = 0; i < sizeof(PLATFORM_IPPROTO_ORDERED) / sizeof(int); i++) {
    if (hints->ai_protocol == PLATFORM_IPPROTO_ORDERED[i]) {
      musl_hints->ai_protocol = MUSL_IPPROTO_ORDERED[i];
    }
  }
  for (int i = 0; i < sizeof(PLATFORM_AF_ORDERED) / sizeof(int); i++) {
    if (hints->ai_family == PLATFORM_AF_ORDERED[i]) {
      musl_hints->ai_family = MUSL_AF_ORDERED[i];
    }
  }
  if (ai_left != 0 ||
      (hints->ai_socktype != 0 && musl_hints->ai_socktype == 0) ||
      (hints->ai_protocol != 0 && musl_hints->ai_protocol == 0) ||
      (hints->ai_family != 0 && musl_hints->ai_family == 0)) {
    return -1;
  }
  return 0;
}

}  // namespace

SB_EXPORT int __abi_wrap_accept(int sockfd,
                                musl_sockaddr* addr,
                                socklen_t* addrlen_ptr) {
  return accept(sockfd, reinterpret_cast<struct sockaddr*>(addr), addrlen_ptr);
}

SB_EXPORT int __abi_wrap_bind(int sockfd,
                              const musl_sockaddr* addr,
                              socklen_t addrlen) {
  return bind(sockfd, reinterpret_cast<const struct sockaddr*>(addr), addrlen);
}

SB_EXPORT int __abi_wrap_connect(int sockfd,
                                 const musl_sockaddr* addr,
                                 socklen_t addrlen) {
  return connect(sockfd, reinterpret_cast<const struct sockaddr*>(addr),
                 addrlen);
}

SB_EXPORT const char* __abi_wrap_gai_strerror(int ecode) {
  return gai_strerror(musl_errcodes_to_platform_errcodes(ecode));
}

SB_EXPORT int __abi_wrap_getaddrinfo(const char* node,
                                     const char* service,
                                     const struct musl_addrinfo* hints,
                                     struct musl_addrinfo** res) {
  if (res == nullptr) {
    return -1;
  }

  // musl addrinfo definition might differ from platform definition.
  // So we need to do a manual conversion to avoid header mismatches.
  struct addrinfo new_hints = {0};

  if (hints != nullptr) {
    if (musl_hints_to_platform_hints(hints, &new_hints) == -1) {
      SbLog(kSbLogPriorityWarning,
            "Unable to convert musl-based addrinfo to platform struct.");
      return -1;
    }
  }

  addrinfo* native_res = nullptr;
  int result = getaddrinfo(
      node, service,
      (hints == nullptr) ? nullptr
                         : reinterpret_cast<const struct addrinfo*>(&new_hints),
      &native_res);

  struct addrinfo* ai = native_res;
  struct musl_addrinfo* last_ai = nullptr;
  *res = nullptr;
  while (ai != nullptr) {
    if (ai->ai_addr != nullptr) {
      addrinfo ai_copy = *ai;
      struct musl_addrinfo* musl_ai =
          (struct musl_addrinfo*)calloc(1, sizeof(struct musl_addrinfo));
      if (platform_hints_to_musl_hints(&ai_copy, musl_ai) == -1) {
        SbLog(kSbLogPriorityWarning,
              "Unable to convert platform addrinfo to musl-based struct.");
        free(musl_ai);
        return -1;
      }
      musl_ai->ai_addrlen = ai_copy.ai_addrlen;
      musl_ai->ai_addr =
          (struct musl_sockaddr*)calloc(1, sizeof(struct musl_sockaddr));
      for (int i = 0; i < sizeof(PLATFORM_AF_ORDERED) / sizeof(int); i++) {
        if (ai_copy.ai_addr->sa_family == PLATFORM_AF_ORDERED[i]) {
          musl_ai->ai_addr->sa_family = MUSL_AF_ORDERED[i];
        }
      }
      if (ai_copy.ai_addr->sa_data != nullptr) {
        memcpy(musl_ai->ai_addr->sa_data, ai_copy.ai_addr->sa_data,
               sizeof(ai_copy.ai_addr->sa_data));
      }
      if (ai_copy.ai_canonname) {
        size_t canonname_len = strlen(ai_copy.ai_canonname);
        musl_ai->ai_canonname =
            reinterpret_cast<char*>(calloc(canonname_len + 1, sizeof(char)));
        memcpy(musl_ai->ai_canonname, ai_copy.ai_canonname, canonname_len);
      }
      if (*res == nullptr) {
        *res = musl_ai;
        last_ai = musl_ai;
      } else {
        last_ai->ai_next = musl_ai;
        last_ai = musl_ai;
      }
    }
    ai = ai->ai_next;
  }
  freeaddrinfo(native_res);

  return result;
}

SB_EXPORT void __abi_wrap_freeaddrinfo(struct musl_addrinfo* ai) {
  struct musl_addrinfo* ptr = ai;
  while (ai != nullptr) {
    if (ai->ai_addr != nullptr) {
      free(ai->ai_addr);
    }
    if (ai->ai_canonname != nullptr) {
      free(ai->ai_canonname);
    }
    ai = ai->ai_next;
    free(ptr);
    ptr = ai;
  }
}

SB_EXPORT int __abi_wrap_getifaddrs(struct ifaddrs** ifap) {
  int result = getifaddrs(ifap);
  return result;
}

SB_EXPORT int __abi_wrap_setsockopt(int socket,
                                    int level,
                                    int option_name,
                                    const void* option_value,
                                    socklen_t option_len) {
  return setsockopt(socket, level, option_name, option_value, option_len);
}

SB_EXPORT int __abi_wrap_shutdown(int socket, int how) {
  return shutdown(socket, musl_shuts_to_platform_shuts(how));
}
