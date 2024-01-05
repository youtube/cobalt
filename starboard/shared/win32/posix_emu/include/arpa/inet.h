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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_ARPA_INET_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_ARPA_INET_H_

#if defined(STARBOARD) && defined(_MSC_VER)

#include <stdlib.h>

#include "starboard/configuration.h"

inline uint32_t htonl(uint32_t in) {
#if SB_IS(BIG_ENDIAN)
  return in;
#else
  return _byteswap_ulong(in);
#endif
}

inline uint16_t htons(uint16_t in) {
#if SB_IS(BIG_ENDIAN)
  return in;
#else
  return _byteswap_ushort(in);
#endif
}

inline uint32_t ntohl(uint32_t in) {
#if SB_IS(BIG_ENDIAN)
  return in;
#else
  return _byteswap_ulong(in);
#endif
}

inline uint16_t ntohs(uint16_t in) {
#if SB_IS(BIG_ENDIAN)
  return in;
#else
  return _byteswap_ushort(in);
#endif
}

#endif  // defined(STARBOARD) && defined(_MSC_VER)

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_ARPA_INET_H_
