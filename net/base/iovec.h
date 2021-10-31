// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IOVEC_H_
#define NET_BASE_IOVEC_H_

#include "build/build_config.h"

#if defined(STARBOARD)
// Starboard sometimes can include sys/uio.h indirectly and it's very difficult
// to eliminate all such includes which can introduce conflicts with the iovec
// declaration here.
struct IOVEC {
  void* iov_base; /* Pointer to data.  */
  size_t iov_len; /* Length of data.  */
};
#else  // STARBOARD
#if defined(OS_WIN) || defined(OS_NACL)
/* Structure for scatter/gather I/O.  */
struct iovec {
  void* iov_base;  /* Pointer to data.  */
  size_t iov_len;  /* Length of data.  */
};
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <sys/uio.h>

#include "starboard/types.h"
#endif  // defined(OS_WIN) || defined(OS_NACL)
#endif  // STARBOARD

#endif  // NET_BASE_IOVEC_H_
