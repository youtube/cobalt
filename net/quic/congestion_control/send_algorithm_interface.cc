// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/send_algorithm_interface.h"

#include "net/quic/congestion_control/fix_rate_sender.h"

namespace net {

// TODO(pwestin): Change to cubic when implemented.
const bool kUseReno = true;

// Factory for send side congestion control algorithm.
SendAlgorithmInterface* SendAlgorithmInterface::Create(
    QuicClock* clock,
    CongestionFeedbackType type) {
  switch (type) {
    case kNone:
      LOG(DFATAL) << "Attempted to create a SendAlgorithm with kNone.";
      break;
    case kTCP:
      //return new TcpCubicSender(clock, kUseReno);
    case kInterArrival:
      break;  // TODO(pwestin) Implement.
    case kFixRate:
      return new FixRateSender(clock);
  }
  return NULL;
}

}  // namespace net
