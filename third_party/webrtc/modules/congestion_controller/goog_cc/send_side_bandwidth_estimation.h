/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  FEC and NACK added bitrate is handled outside class
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "api/field_trials_view.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/goog_cc/loss_based_bwe.h"
#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

class RtcEventLog;

class RttBasedBackoff {
 public:
  explicit RttBasedBackoff(const FieldTrialsView& key_value_config);
  ~RttBasedBackoff();
  void UpdatePropagationRtt(Timestamp at_time, TimeDelta propagation_rtt);
  bool IsRttAboveLimit() const;

  FieldTrialFlag disabled_;
  FieldTrialParameter<TimeDelta> configured_limit_;
  FieldTrialParameter<double> drop_fraction_;
  FieldTrialParameter<TimeDelta> drop_interval_;
  FieldTrialParameter<DataRate> bandwidth_floor_;

 public:
  TimeDelta rtt_limit_;
  Timestamp last_propagation_rtt_update_;
  TimeDelta last_propagation_rtt_;
  Timestamp last_packet_sent_;

 private:
  TimeDelta CorrectedRtt() const;
};

class SendSideBandwidthEstimation {
 public:
  SendSideBandwidthEstimation() = delete;
  SendSideBandwidthEstimation(const FieldTrialsView* key_value_config,
                              RtcEventLog* event_log);
  ~SendSideBandwidthEstimation();

  void OnRouteChange();

  DataRate target_rate() const;
  LossBasedState loss_based_state() const;
  // Return whether the current rtt is higher than the rtt limited configured in
  // RttBasedBackoff.
  bool IsRttAboveLimit() const;
  uint8_t fraction_loss() const { return loss_based_bwe_.fraction_loss(); }
  TimeDelta round_trip_time() const { return last_round_trip_time_; }

  // Call periodically to update estimate.
  void OnPeriodicUpdate(Timestamp at_time);
  void OnSentPacket(const SentPacket& sent_packet);
  void UpdatePropagationRtt(Timestamp at_time, TimeDelta propagation_rtt);

  // Call when we receive a RTCP message with TMMBR or REMB.
  void UpdateReceiverEstimate(Timestamp at_time, DataRate bandwidth);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdatePacketsLost(int64_t packets_lost,
                         int64_t packets_received,
                         Timestamp at_time);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdateRtt(TimeDelta rtt, Timestamp at_time);

  void SetBitrates(std::optional<DataRate> send_bitrate,
                   DataRate min_bitrate,
                   DataRate max_bitrate,
                   Timestamp at_time);
  void SetMinMaxBitrate(DataRate min_bitrate, DataRate max_bitrate);
  int GetMinBitrate() const;

  void OnTransportPacketsFeedback(const TransportPacketsFeedback& report,
                                  DataRate delay_based_estimate,
                                  std::optional<DataRate> acknowledged_rate,
                                  bool is_probe_rate,
                                  bool in_alr);

 private:
  friend class GoogCcStatePrinter;

  enum UmaState { kNoUpdate, kFirstDone, kDone };

  bool IsInStartPhase(Timestamp at_time) const;

  void UpdateUmaStatsPacketsLost(Timestamp at_time, int packets_lost);

  // Updates history of min bitrates.
  // After this method returns min_bitrate_history_.front().second contains the
  // min bitrate used during last kBweIncreaseIntervalMs.
  void UpdateMinHistory(Timestamp at_time);

  // Prints a warning if `bitrate` if sufficiently long time has past since last
  // warning.
  void MaybeLogLowBitrateWarning(DataRate bitrate, Timestamp at_time);
  // Stores an update to the event log if the loss rate has changed, the target
  // has changed, or sufficient time has passed since last stored event.
  void MaybeLogLossBasedEvent(Timestamp at_time);

  void ApplyTargetLimits(Timestamp at_time);

  const FieldTrialsView* key_value_config_;
  RttBasedBackoff rtt_backoff_;
  LossBasedBwe loss_based_bwe_;

  std::optional<DataRate> acknowledged_rate_;
  uint8_t last_logged_fraction_loss_;
  TimeDelta last_round_trip_time_;
  // The max bitrate as set by the receiver in the call. This is typically
  // signalled using the REMB RTCP message and is used when we don't have any
  // send side delay based estimate.
  DataRate receiver_limit_;
  DataRate delay_based_limit_;
  DataRate loss_based_limit_;

  // `rtt_back_off_rate_` is calculated in relation to a limit and can only be
  // lower than the limit. If not, it is nullopt.
  std::optional<DataRate> rtt_back_off_rate_;

  DataRate current_target_;  // Current combined target rate.
  DataRate last_logged_target_;
  DataRate min_bitrate_configured_;
  DataRate max_bitrate_configured_;
  Timestamp last_low_bitrate_log_;

  Timestamp time_last_decrease_due_to_rtt_;
  Timestamp first_loss_report_time_;
  int initially_lost_packets_;
  DataRate bitrate_at_2_seconds_;
  UmaState uma_update_state_;
  UmaState uma_rtt_state_;
  std::vector<bool> rampup_uma_stats_updated_;
  RtcEventLog* const event_log_;
  Timestamp last_rtc_event_log_;
};
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
