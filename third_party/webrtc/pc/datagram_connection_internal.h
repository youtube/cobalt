/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef PC_DATAGRAM_CONNECTION_INTERNAL_H_
#define PC_DATAGRAM_CONNECTION_INTERNAL_H_

#include <memory>

#include "api/datagram_connection.h"
#include "api/environment/environment.h"
#include "api/ref_count.h"
#include "call/rtp_packet_sink_interface.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/port_allocator.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/dtls_transport.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_EXPORT DatagramConnectionInternal : public DatagramConnection,
                                              public RtpPacketSinkInterface {
 public:
  DatagramConnectionInternal(const Environment& env,
                             std::unique_ptr<PortAllocator> port_allocator,
                             absl::string_view transport_name,
                             bool ice_controlling,
                             scoped_refptr<RTCCertificate> certificate,
                             std::unique_ptr<Observer> observer,
                             std::unique_ptr<IceTransportInternal>
                                 custom_ice_transport_internal = nullptr);

  void SetRemoteIceParameters(const IceParameters& ice_parameters) override;
  void AddRemoteCandidate(const Candidate& candidate) override;
  bool Writable() override;
  void SetRemoteDtlsParameters(absl::string_view digestAlgorithm,
                               const uint8_t* digest,
                               size_t digest_len,
                               DatagramConnection::SSLRole ssl_role) override;
  bool SendPacket(ArrayView<const uint8_t> data) override;

  void Terminate(
      absl::AnyInvocable<void()> terminate_complete_callback) override;

  void OnCandidateGathered(IceTransportInternal* ice_transport,
                           const Candidate& candidate);

  void OnWritableStatePossiblyChanged();
  void OnTransportWritableStateChanged(PacketTransportInternal*);
  void OnConnectionError();

  // RtpPacketSinkInterface
  void OnRtpPacket(const RtpPacketReceived& packet) override;

#if RTC_DCHECK_IS_ON
  DtlsSrtpTransport* GetDtlsSrtpTransportForTesting() {
    return dtls_srtp_transport_.get();
  }
#endif

 private:
  enum class State { kActive, kTerminated };
  State current_state_ = State::kActive;

  // Note the destruction order of these transport objects must be preserved.
  const std::unique_ptr<PortAllocator> port_allocator_;
  const std::unique_ptr<IceTransportInternal> transport_channel_;
  const scoped_refptr<DtlsTransport> dtls_transport_;
  const std::unique_ptr<DtlsSrtpTransport> dtls_srtp_transport_;

  const std::unique_ptr<Observer> observer_;

  bool last_writable_state_ = false;
  const SequenceChecker sequence_checker_;
  uint16_t next_seq_num_ RTC_GUARDED_BY(sequence_checker_) = 0;
  uint32_t next_ts_ RTC_GUARDED_BY(sequence_checker_) = 10000;
};

}  // namespace webrtc
#endif  // PC_DATAGRAM_CONNECTION_INTERNAL_H_
