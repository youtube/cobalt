// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Fix rate receive side congestion control, used for initial testing.

#ifndef NET_QUIC_CONGESTION_CONTROL_FIX_RATE_RECEIVER_H_
#define NET_QUIC_CONGESTION_CONTROL_FIX_RATE_RECEIVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"

namespace net {

class NET_EXPORT_PRIVATE FixRateReceiver : public ReceiveAlgorithmInterface {
 public:
  FixRateReceiver();

  // Implements ReceiveAlgorithmInterface.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback) override;

  // Implements ReceiveAlgorithmInterface.
  virtual void RecordIncomingPacket(size_t bytes,
                                    QuicPacketSequenceNumber sequence_number,
                                    QuicTime timestamp,
                                    bool recovered) override;

  void SetBitrate(int bytes_per_second);  // Used for testing only.

 private:
  int bitrate_in_bytes_per_second_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_FIX_RATE_RECEIVER_H_
