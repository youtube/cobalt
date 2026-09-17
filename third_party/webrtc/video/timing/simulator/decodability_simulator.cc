/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/decodability_simulator.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_frame.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_base/thread_annotations.h"
#include "video/timing/simulator/assembler.h"
#include "video/timing/simulator/decodability_tracker.h"
#include "video/timing/simulator/rtc_event_log_driver.h"

namespace webrtc::video_timing_simulator {

namespace {

// Observes the `Assembler` and `DecodabilityTracker` in order to collect
// frame metadata for decodable frames.
class DecodableFrameCollector : public AssemblerEvents,
                                public DecodabilityTrackerEvents {
 public:
  DecodableFrameCollector(const Environment& env, uint32_t ssrc)
      : env_(env), ssrc_(ssrc) {
    RTC_DCHECK_NE(ssrc_, 0);
  }
  ~DecodableFrameCollector() override = default;

  DecodableFrameCollector(const DecodableFrameCollector&) = delete;
  DecodableFrameCollector& operator=(const DecodableFrameCollector&) = delete;

  // Implements `AssemblerEvents`.
  void OnAssembledFrame(const EncodedFrame& assembled_frame) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    Timestamp now = env_.clock().CurrentTime();
    if (!creation_timestamp_) {
      creation_timestamp_ = now;
    }
    int64_t frame_id = assembled_frame.Id();
    if (frames_.contains(frame_id)) {
      RTC_LOG(LS_WARNING) << "Assembled frame_id=" << frame_id
                          << " on ssrc=" << ssrc_
                          << " had already been collected. Dropping it."
                          << " (simulated_ts=" << env_.clock().CurrentTime()
                          << ")";
      return;
    }
    auto& frame = frames_[frame_id];
    RTC_DCHECK(!assembled_frame.PacketInfos().empty());
    frame.num_packets = static_cast<int>(assembled_frame.PacketInfos().size());
    frame.size = DataSize::Bytes(assembled_frame.size());
    frame.unwrapped_rtp_timestamp =
        rtp_timestamp_unwrapper_.Unwrap(assembled_frame.RtpTimestamp());
    frame.assembled_timestamp = now;
  }

  // Implements `DecodabilityTrackerEvents`.
  void OnDecodableFrame(const EncodedFrame& decodable_frame) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    int64_t frame_id = decodable_frame.Id();
    auto it = frames_.find(frame_id);
    if (it == frames_.end()) {
      RTC_LOG(LS_WARNING)
          << "Decodable frame_id=" << frame_id << " on ssrc=" << ssrc_
          << " had no assembly information collected. Dropping it."
          << " (simulated_ts=" << env_.clock().CurrentTime() << ")";
      return;
    }
    auto& frame = it->second;
    frame.decodable_timestamp = env_.clock().CurrentTime();
  }

  DecodabilitySimulator::Stream GetStream() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    DecodabilitySimulator::Stream stream;
    stream.ssrc = ssrc_;
    if (creation_timestamp_) {
      stream.creation_timestamp = *creation_timestamp_;
    }
    stream.frames.reserve(frames_.size());
    for (const auto& [key, value] : frames_) {
      stream.frames.push_back(value);
    }
    absl::c_sort(stream.frames);
    return stream;
  }

 private:
  SequenceChecker sequence_checker_;
  const Environment env_;
  const uint32_t ssrc_;

  std::optional<Timestamp> creation_timestamp_
      RTC_GUARDED_BY(sequence_checker_);
  SeqNumUnwrapper<uint32_t> rtp_timestamp_unwrapper_
      RTC_GUARDED_BY(sequence_checker_);
  absl::flat_hash_map</*frame_id*/ int64_t, DecodabilitySimulator::Frame>
      frames_ RTC_GUARDED_BY(sequence_checker_);
};

// Combines all objects needed to perform decodability simulation of a single
// stream. Inserts the streams results to the `results` pointer when `Close()`
// is called (at the end of simulation).
class DecodabilitySimulatorStream : public RtcEventLogDriver::StreamInterface {
 public:
  DecodabilitySimulatorStream(const Environment& env,
                              uint32_t ssrc,
                              DecodabilitySimulator::Results* absl_nonnull
                                  results)
      : collector_(env, ssrc),
        tracker_(env, DecodabilityTracker::Config{.ssrc = ssrc}, &collector_),
        assembler_(env, ssrc, &collector_, &tracker_),
        results_(*results) {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    tracker_.SetDecodedFrameIdCallback(&assembler_);
  }
  ~DecodabilitySimulatorStream() override = default;

  // Implements `RtcEventLogDriver::StreamInterface`.
  void InsertPacket(const RtpPacketReceived& rtp_packet) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    assembler_.InsertPacket(rtp_packet);
  }

  void Close() override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    DecodabilitySimulator::Stream stream = collector_.GetStream();
    if (!stream.IsEmpty()) {
      RTC_DCHECK_NE(stream.ssrc, 0u);
      results_.streams.push_back(stream);
    }
  }

 private:
  SequenceChecker sequence_checker_;
  DecodableFrameCollector collector_ RTC_GUARDED_BY(sequence_checker_);
  DecodabilityTracker tracker_ RTC_GUARDED_BY(sequence_checker_);
  Assembler assembler_ RTC_GUARDED_BY(sequence_checker_);
  DecodabilitySimulator::Results& results_;
};

}  // namespace

DecodabilitySimulator::Results DecodabilitySimulator::Simulate(
    const ParsedRtcEventLog& parsed_log) const {
  // Outputs.
  Results results;

  // Simulation.
  auto stream_factory = [&results](const Environment& env, uint32_t ssrc) {
    return std::make_unique<DecodabilitySimulatorStream>(env, ssrc, &results);
  };
  // Decodability should not be a function of any field trials, so we pass the
  // empty string here.
  RtcEventLogDriver rtc_event_log_simulator(&parsed_log,
                                            /*field_trials_string=*/"",
                                            std::move(stream_factory));
  rtc_event_log_simulator.Simulate();

  // Return.
  absl::c_sort(results.streams);
  return results;
}

}  // namespace webrtc::video_timing_simulator
