// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_INLINED_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_INLINED_FRAME_H_

#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

// QuicInlinedFrame is the base class of all frame types that is inlined in the
// QuicFrame class. It gurantees all inlined frame types contain a 'type' field
// at offset 0, such that QuicFrame.type can get the correct frame type for both
// inline and out-of-line frame types.
template <typename DerivedT>
struct QUIC_EXPORT_PRIVATE QuicInlinedFrame {
  QuicInlinedFrame(QuicFrameType type) : type(type) {
#if !(defined(STARBOARD) && defined(__GNUC__) && !defined(__clang__) && \
      __GNUC__ <= 7)
    // Raspi compiler does not allow none-static type in offsetof, but we should
    // turn on this check on as many platforms as possible.
    static_assert(offsetof(DerivedT, type) == 0,
                  "type must be the first field.");
#endif
    static_assert(sizeof(DerivedT) <= 24,
                  "Frames larger than 24 bytes should not be inlined.");
  }
  QuicFrameType type;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_INLINED_FRAME_H_
