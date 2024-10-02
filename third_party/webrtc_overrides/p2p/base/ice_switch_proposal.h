// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBRTC_OVERRIDES_P2P_BASE_ICE_SWITCH_PROPOSAL_H_
#define THIRD_PARTY_WEBRTC_OVERRIDES_P2P_BASE_ICE_SWITCH_PROPOSAL_H_

#include <ostream>
#include <string>
#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/p2p/base/ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/ice_switch_reason.h"
#include "third_party/webrtc/rtc_base/system/rtc_export.h"
#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"
#include "third_party/webrtc_overrides/p2p/base/ice_proposal.h"

namespace blink {

// Reasons for performing an ICE switch.
enum class IceSwitchReason {
  kUnknown,
  kRemoteCandidateGenerationChange,
  kNetworkPreferenceChange,
  kNewConnectionFromLocalCandidate,
  kNewConnectionFromRemoteCandidate,
  kNewConnectionFromUnknownRemoteAddress,
  kNominationOnControlledSide,
  kDataReceived,
  kConnectStateChange,
  kSelectedConnectionDestroyed,
  kIceControllerRecheck,
  kApplicationRequested,
};

std::string IceSwitchReasonToString(IceSwitchReason reason);
RTC_EXPORT IceSwitchReason
ConvertFromWebrtcIceSwitchReason(cricket::IceSwitchReason reason);
cricket::IceSwitchReason ConvertToWebrtcIceSwitchReason(IceSwitchReason reason);

// Represents a future event to check whether an ICE switch should be performed.
struct IceRecheckEvent {
  explicit IceRecheckEvent(const cricket::IceRecheckEvent& event);

  IceSwitchReason reason;
  int recheck_delay_ms;
};

// A proposal to switch the ICE transport to use the proposed ICE connection.
// Optionally indicates the duration until another check is performed whether to
// switch the connection.
class RTC_EXPORT IceSwitchProposal : public IceProposal {
 public:
  IceSwitchProposal(
      cricket::IceSwitchReason reason,
      const cricket::IceControllerInterface::SwitchResult& switch_result,
      bool reply_expected);

  IceSwitchProposal(const IceSwitchProposal&) = default;

  ~IceSwitchProposal() override = default;

  // The reason for which this switch is proposed.
  IceSwitchReason reason() const { return reason_; }
  // The connection that the ICE transport will switch to.
  absl::optional<const IceConnection> connection() const { return connection_; }
  // An optional event describing when the next check will be performed to
  // switch to a new connection.
  absl::optional<IceRecheckEvent> recheck_event() const {
    return recheck_event_;
  }
  // Connections for which some learnt state should be reset.
  // TODO(crbug.com/1369096): this is probably not necessary, check!
  const rtc::ArrayView<const IceConnection> connections_to_forget_state_on()
      const {
    return connections_to_forget_state_on_;
  }

  std::string ToString() const;
  // Pretty printing for unit test matchers.
  friend void PrintTo(const IceSwitchProposal& proposal, std::ostream* os) {
    *os << proposal.ToString();
  }

 private:
  IceSwitchReason reason_;
  absl::optional<IceConnection> connection_;
  absl::optional<IceRecheckEvent> recheck_event_;
  std::vector<IceConnection> connections_to_forget_state_on_;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_P2P_BASE_ICE_SWITCH_PROPOSAL_H_
