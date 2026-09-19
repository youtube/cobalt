/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_RENDERING_SIMULATOR_H_
#define VIDEO_TIMING_SIMULATOR_RENDERING_SIMULATOR_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/environment/environment.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/video_coding/timing/timing.h"

namespace webrtc::video_timing_simulator {

// The `RenderingSimulator` takes an `ParsedRtcEventLog` and produces a
// sequence of metadata about decoded and rendered frames that were contained in
// the log.
class RenderingSimulator {
 public:
  struct Config {
    using VideoTimingFactory =
        std::function<std::unique_ptr<VCMTiming>(Environment)>;

    std::string name = "";
    std::string field_trials_string = "";
    VideoTimingFactory video_timing_factory = [](Environment env) {
      return std::make_unique<VCMTiming>(&env.clock(), env.field_trials());
    };
  };

  // Metadata about a single rendered frame.
  struct Frame {
    // Frame information.
    int num_packets = -1;
    DataSize size = DataSize::Zero();

    // RTP header information.
    int payload_type = -1;
    uint32_t rtp_timestamp = 0;
    int64_t unwrapped_rtp_timestamp = -1;

    // Dependency descriptor information.
    int64_t frame_id = -1;
    int spatial_id = -1;
    int temporal_id = -1;
    int num_references = -1;

    // Packet timestamps.
    Timestamp first_packet_arrival_timestamp = Timestamp::PlusInfinity();
    Timestamp last_packet_arrival_timestamp = Timestamp::MinusInfinity();

    // Frame timestamps.
    Timestamp assembled_timestamp = Timestamp::PlusInfinity();
    Timestamp render_timestamp = Timestamp::PlusInfinity();
    Timestamp decoded_timestamp = Timestamp::PlusInfinity();
    Timestamp rendered_timestamp = Timestamp::PlusInfinity();

    // Jitter buffer state at the time of this frame.
    int frames_dropped = -1;
    // TODO: b/423646186 - Add `current_delay_ms`.
    TimeDelta jitter_buffer_minimum_delay = TimeDelta::MinusInfinity();
    TimeDelta jitter_buffer_target_delay = TimeDelta::MinusInfinity();
    TimeDelta jitter_buffer_delay = TimeDelta::MinusInfinity();

    bool operator<(const Frame& other) const {
      return rendered_timestamp < other.rendered_timestamp;
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
    TimeDelta InterArrivalTime(const Frame& prev) const {
      return rendered_timestamp - prev.rendered_timestamp;
    }
    TimeDelta AssemblyDuration() const {
      return last_packet_arrival_timestamp - first_packet_arrival_timestamp;
    }
    TimeDelta PreDecodeBufferDuration() const {
      return decoded_timestamp - assembled_timestamp;
    }
    TimeDelta PostDecodeMargin() const {
      return render_timestamp - rendered_timestamp -
             RenderingSimulator::kRenderDelay;
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
    std::string config_name;
    std::vector<Stream> streams;
  };

  // Static configuration.
  static constexpr TimeDelta kRenderDelay = TimeDelta::Millis(10);

  explicit RenderingSimulator(Config config);
  ~RenderingSimulator();

  RenderingSimulator(const RenderingSimulator&) = delete;
  RenderingSimulator& operator=(const RenderingSimulator&) = delete;

  Results Simulate(const ParsedRtcEventLog& parsed_log) const;

 private:
  const Config config_;
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_RENDERING_SIMULATOR_H_
