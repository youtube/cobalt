/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_

#include <memory>
#include <optional>

#include "api/field_trials_view.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "modules/pacing/interval_budget.h"
#include "rtc_base/experiments/struct_parameters_parser.h"

namespace webrtc {

class RtcEventLog;

struct AlrDetectorConfig {
  // Sent traffic ratio as a function of network capacity used to determine
  // application-limited region. ALR region start when bandwidth usage drops
  // below kAlrStartUsageRatio and ends when it raises above
  // kAlrEndUsageRatio. NOTE: This is intentionally conservative at the moment
  // until BW adjustments of application limited region is fine tuned.
  double bandwidth_usage_ratio = 0.65;
  double start_budget_level_ratio = 0.80;
  double stop_budget_level_ratio = 0.50;
  std::unique_ptr<StructParametersParser> Parser();
};
// Application limited region detector is a class that utilizes signals of
// elapsed time and bytes sent to estimate whether network traffic is
// currently limited by the application's ability to generate traffic.
//
// AlrDetector provides a signal that can be utilized to adjust
// estimate bandwidth.
// Note: This class is not thread-safe.
class AlrDetector {
 public:
  AlrDetector(AlrDetectorConfig config, RtcEventLog* event_log);
  explicit AlrDetector(const FieldTrialsView* key_value_config);
  AlrDetector(const FieldTrialsView* key_value_config, RtcEventLog* event_log);
  ~AlrDetector();

  void OnBytesSent(DataSize bytes_sent, Timestamp send_time);

  // Set current estimated bandwidth.
  void SetEstimatedBitrate(DataRate bitrate);

  // Returns time in milliseconds when the current application-limited region
  // started or empty result if the sender is currently not application-limited.
  std::optional<Timestamp> GetApplicationLimitedRegionStartTime() const;

 private:
  friend class GoogCcStatePrinter;
  const AlrDetectorConfig conf_;

  std::optional<Timestamp> last_send_time_;

  IntervalBudget alr_budget_;
  std::optional<Timestamp> alr_started_time_;

  RtcEventLog* event_log_;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_
