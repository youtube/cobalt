// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_IOVEC_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_IOVEC_IMPL_H_

#include <stddef.h>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN) || defined(COMPILER_MSVC)
/* Structure for scatter/gather I/O.  */

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_UIO_H_
struct iovec {
  void* iov_base; /* Pointer to data.  */
  size_t iov_len; /* Length of data.  */
};
#endif

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA) || defined(COBALT_PENDING_CLEAN_UP)
#include <sys/uio.h>
#endif  // BUILDFLAG(IS_WIN)

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_IOVEC_IMPL_H_
