// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CONSTANTS_H_
#define MEDIA_CAST_CONSTANTS_H_

////////////////////////////////////////////////////////////////////////////////
// NOTE: This file should only contain constants that are reasonably globally
// used (i.e., by many modules, and in all or nearly all subdirs).  Do NOT add
// non-POD constants, functions, interfaces, or any logic to this module.
////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>

namespace media {
namespace cast {

// Integer constants set either by the Cast Streaming Protocol Spec or due to
// design limitations.
enum Specifications {
  // Target number of milliseconds between the sending of RTCP reports.  Both
  // senders and receivers regularly send RTCP reports to their peer.
  kRtcpReportIntervalMs = 500,

  // This is an important system-wide constant.  This limits how much history
  // the implementation must retain in order to process the acknowledgements of
  // past frames.
  //
  // This value is carefully choosen such that it fits in the 8-bits range for
  // frame IDs. It is also less than half of the full 8-bits range such that
  // logic can handle wrap around and compare two frame IDs meaningfully.
  kMaxUnackedFrames = 120,

  // The spec declares RTP timestamps must always have a timebase of 90000 ticks
  // per second for video.
  kVideoFrequency = 90000,
};

// Success/in-progress/failure status codes reported to clients to indicate
// readiness state.
enum OperationalStatus {
  // Client should not send frames yet (sender), or should not expect to receive
  // frames yet (receiver).
  STATUS_UNINITIALIZED,

  // Client may now send or receive frames.
  STATUS_INITIALIZED,

  // Codec is being re-initialized.  Client may continue sending frames, but
  // some may be ignored/dropped until a transition back to STATUS_INITIALIZED.
  STATUS_CODEC_REINIT_PENDING,

  // Session has halted due to invalid configuration.
  STATUS_INVALID_CONFIGURATION,

  // Session has halted due to an unsupported codec.
  STATUS_UNSUPPORTED_CODEC,

  // Session has halted due to a codec initialization failure.  Note that this
  // can be reported after STATUS_INITIALIZED/STATUS_CODEC_REINIT_PENDING if the
  // codec was re-initialized during the session.
  STATUS_CODEC_INIT_FAILED,

  // Session has halted due to a codec runtime failure.
  STATUS_CODEC_RUNTIME_ERROR,
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CONSTANTS_H_
