/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_SCREAM_NETWORK_CONTROLLER_GOOG_CC_SCREAM_NETWORK_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_SCREAM_NETWORK_CONTROLLER_GOOG_CC_SCREAM_NETWORK_CONTROLLER_H_

#include <memory>
#include <string_view>

#include "absl/functional/any_invocable.h"
#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
#include "modules/congestion_controller/scream/scream_network_controller.h"

namespace webrtc {

// GoogCcScreamNetworkController chooses if GoogCC or Scream should be used
// depending on the field trial key WebRTC-Bwe-ScreamV2.
// It should only be used together with RFC 8888 congestion control feedback.
// The purpose of this wrapper is to simplify experimentation with Scream in L4S
// enabled networks, without having to be better than Goog CC in all scenarios.
//
// If field trial value of the key WebRTC-Bwe-ScreamV2 contain:
//   Enabled or mode:always - Scream is used always.
//   mode:only_after_ce -  GoogCC is iniallally used, but Scream state is
//                         updated in parallel. If ECN CE marks is seen in the
//                         feedback, Scream is used instead of GoogCC for the
//                         remaining duration of the call.
//   mode:goog_cc_with_ect1 - Goog CC is always used, but the controller will
//                            claim to support ECN adaptation until the first
//                            CE mark is seen. After that, packets are not sent
//                            as ECT(1).
class GoogCcScreamNetworkController : public NetworkControllerInterface {
 public:
  enum class CcType { kGoogCC, kScream };
  GoogCcScreamNetworkController(NetworkControllerConfig config,
                                GoogCcConfig goog_cc_config);
  ~GoogCcScreamNetworkController() override;

  // webrtc::NetworkControllerInterface overrides.
  NetworkControlUpdate OnNetworkAvailability(NetworkAvailability msg) override;
  NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange msg) override;
  NetworkControlUpdate OnProcessInterval(ProcessInterval msg) override;
  NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport msg) override;
  NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate msg) override;
  NetworkControlUpdate OnSentPacket(SentPacket msg) override;
  NetworkControlUpdate OnReceivedPacket(ReceivedPacket msg) override;
  NetworkControlUpdate OnStreamsConfig(StreamsConfig msg) override;
  NetworkControlUpdate OnTargetRateConstraints(
      TargetRateConstraints msg) override;
  NetworkControlUpdate OnTransportLossReport(TransportLossReport msg) override;
  NetworkControlUpdate OnTransportPacketsFeedback(
      TransportPacketsFeedback msg) override;

  NetworkControlUpdate OnNetworkStateEstimate(
      NetworkStateEstimate msg) override;

  bool SupportsEcnAdaptation() const override {
    switch (mode_) {
      case Mode::kGoogCc:
        return false;
      case Mode::kGoogCcWithEct1:
        return !ecn_ce_seen_;
      case Mode::kScreamAlways:
      case Mode::kScreamAfterCe:
        return true;
    }
  }
  // Returns "ScreamV2" or "GoogCC" depending on currently used network
  // controller.
  std::string_view CurrentControllerType() const;

 private:
  enum class Mode { kGoogCc, kScreamAlways, kScreamAfterCe, kGoogCcWithEct1 };

  static Mode ParseMode(const FieldTrialsView& field_trials);

  NetworkControlUpdate MaybeRunOnAllControllers(
      absl::AnyInvocable<NetworkControlUpdate(NetworkControllerInterface&)>
          update);

  Environment env_;
  Mode mode_;
  bool scream_in_use_ = false;
  bool ecn_ce_seen_ = false;

  std::unique_ptr<GoogCcNetworkController> goog_cc_;
  std::unique_ptr<ScreamNetworkController> scream_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_SCREAM_NETWORK_CONTROLLER_GOOG_CC_SCREAM_NETWORK_CONTROLLER_H_
