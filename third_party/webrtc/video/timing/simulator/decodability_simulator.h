/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_DECODABILITY_SIMULATOR_H_
#define VIDEO_TIMING_SIMULATOR_DECODABILITY_SIMULATOR_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"

namespace webrtc::video_timing_simulator {

// The `DecodabilitySimulator` takes an `ParsedRtcEventLog` and produces a
// sequence of metadata about decodable frames that were contained in the log.
class DecodabilitySimulator {
 public:
  // Metadata about a single decodable frame.
  struct Frame {
    // Frame information.
    int num_packets = -1;
    DataSize size = DataSize::Zero();

    // RTP header information.
    int64_t unwrapped_rtp_timestamp = -1;

    // Frame timestamps.
    Timestamp assembled_timestamp = Timestamp::PlusInfinity();
    Timestamp decodable_timestamp = Timestamp::PlusInfinity();

    bool operator<(const Frame& other) const {
      return decodable_timestamp < other.decodable_timestamp;
    }

    std::optional<int> InterPacketCount(const Frame& prev) const {
      if (num_packets <= 0 || prev.num_packets <= 0) {
        return std::nullopt;
      }
      return num_packets - prev.num_packets;
    }
    std::optional<int64_t> InterFrameSizeBytes(const Frame& prev) const {
      if (size.IsZero() || prev.size.IsZero()) {
        return std::nullopt;
      }
      return size.bytes() - prev.size.bytes();
    }
    TimeDelta InterDepartureTime(const Frame& prev) const {
      if (unwrapped_rtp_timestamp < 0 || prev.unwrapped_rtp_timestamp < 0) {
        return TimeDelta::PlusInfinity();
      }
      constexpr int64_t kRtpTicksPerMs = 90;
      int64_t inter_departure_time_ms =
          (unwrapped_rtp_timestamp - prev.unwrapped_rtp_timestamp) /
          kRtpTicksPerMs;
      return TimeDelta::Millis(inter_departure_time_ms);
    }
    TimeDelta InterAssemblyTime(const Frame& prev) const {
      return assembled_timestamp - prev.assembled_timestamp;
    }
    TimeDelta InterArrivalTime(const Frame& prev) const {
      return decodable_timestamp - prev.decodable_timestamp;
    }
  };

  // All frames in one stream.
  struct Stream {
    Timestamp creation_timestamp = Timestamp::PlusInfinity();
    uint32_t ssrc = 0;
    std::vector<Frame> frames;

    bool IsEmpty() const { return frames.empty(); }

    bool operator<(const Stream& other) const {
      if (creation_timestamp != other.creation_timestamp) {
        return creation_timestamp < other.creation_timestamp;
      }
      return ssrc < other.ssrc;
    }
  };

  // All streams.
  struct Results {
    std::vector<Stream> streams;
  };

  DecodabilitySimulator() = default;
  ~DecodabilitySimulator() = default;

  DecodabilitySimulator(const DecodabilitySimulator&) = delete;
  DecodabilitySimulator& operator=(const DecodabilitySimulator&) = delete;

  Results Simulate(const ParsedRtcEventLog& parsed_log) const;
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_DECODABILITY_SIMULATOR_H_
