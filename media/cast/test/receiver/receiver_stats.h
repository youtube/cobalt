// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_RECEIVER_RECEIVER_STATS_H_
#define MEDIA_CAST_TEST_RECEIVER_RECEIVER_STATS_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/net/rtp/rtp_defines.h"

namespace media {
namespace cast {

class ReceiverStats {
 public:
  explicit ReceiverStats(const base::TickClock* clock);

  ReceiverStats(const ReceiverStats&) = delete;
  ReceiverStats& operator=(const ReceiverStats&) = delete;

  RtpReceiverStatistics GetStatistics();
  void UpdateStatistics(const RtpCastHeader& header, int rtp_timebase);

 private:
  const raw_ptr<const base::TickClock> clock_;  // Not owned by this class.

  // Global metrics.
  uint16_t min_sequence_number_;
  uint16_t max_sequence_number_;
  uint32_t total_number_packets_;
  uint16_t sequence_number_cycles_;
  RtpTimeTicks last_received_rtp_timestamp_;
  base::TimeTicks last_received_packet_time_;
  base::TimeDelta jitter_;

  // Intermediate metrics - between RTCP reports.
  int interval_min_sequence_number_;
  int interval_number_packets_;
  int interval_wrap_count_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_RECEIVER_RECEIVER_STATS_H_
