// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a convenience header to pull in the platform-specific
// headers that define functions for byte-order conversion,
// particularly: ntohs(), htons(), ntohl(), htonl(). Prefer including
// this file instead of directly writing the #if / #else, since it
// avoids duplicating the platform-specific selections.
// This header also provides ntohll() and htonll() for byte-order conversion
// for 64-bit integers.

#ifndef BASE_SYS_BYTEORDER_H_
#define BASE_SYS_BYTEORDER_H_

#include "base/basictypes.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

// Include headers to provide byteswap for all platforms.
#if defined(COMPILER_MSVC)
#include <stdlib.h>
#elif defined(OS_MACOSX)
#include <libkern/OSByteOrder.h>
#elif defined(OS_OPENBSD)
#include <sys/endian.h>
#else
#include <byteswap.h>
#endif


namespace base {

// Returns a value with all bytes in |x| swapped, i.e. reverses the endianness.
inline uint64 ByteSwap(uint64 x) {
#if defined(COMPILER_MSVC)
  return _byteswap_uint64(x);
#elif defined(OS_MACOSX)
  return OSSwapInt64(x);
#elif defined(OS_OPENBSD)
  return swap64(x);
#else
  return bswap_64(x);
#endif
}

// Converts the bytes in |x| from network to host order (endianness), and
// returns the result.
inline uint64 ntohll(uint64 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}

// Converts the bytes in |x| from host to network order (endianness), and
// returns the result.
inline uint64 htonll(uint64 x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}

}  // namespace base


#endif  // BASE_SYS_BYTEORDER_H_
