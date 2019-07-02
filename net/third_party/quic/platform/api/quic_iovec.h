// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_IOVEC_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_IOVEC_H_

#include <cstddef>
#include <type_traits>

#include "net/third_party/quic/platform/impl/quic_iovec_impl.h"


#if !defined(STARBOARD)
// offsetof is not portable across compilers.
namespace quic {

// The impl header has to export struct IOVEC, or a POSIX-compatible polyfill.
// Below, we mostly assert that what we have is appropriate.
static_assert(std::is_standard_layout<struct IOVEC>::value,
              "iovec has to be a standard-layout struct");

static_assert(offsetof(struct IOVEC, iov_base) < sizeof(struct IOVEC),
              "iovec has to have iov_base");
static_assert(offsetof(struct IOVEC, iov_len) < sizeof(struct IOVEC),
              "iovec has to have iov_len");

}  // namespace quic

#endif  // STARBOARD

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_IOVEC_H_
