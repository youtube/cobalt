/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/datagram_connection_internal.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/environment/environment.h"
#include "api/ice_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/transport/enums.h"
#include "call/rtp_demuxer.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/packet_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "p2p/dtls/dtls_transport.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/dtls_transport.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace webrtc {
namespace {
// Fixed SSRC for DatagramConnections. Transport won't be shared with any
// other streams, so a single fixed SSRC is safe.
constexpr uint32_t kDatagramConnectionSsrc = 0x1EE7;

// Helper function to create IceTransportInit
IceTransportInit CreateIceTransportInit(const Environment& env,
                                        PortAllocator* allocator) {
  IceTransportInit init(env);
  init.set_port_allocator(allocator);
  return init;
}

// Helper function to create DtlsTransportInternal
std::unique_ptr<DtlsTransportInternal> CreateDtlsTransportInternal(
    const Environment& env,
    IceTransportInternal* transport_channel) {
  return std::make_unique<DtlsTransportInternalImpl>(
      env, transport_channel, CryptoOptions{},
      /*ssl_max_version=*/SSL_PROTOCOL_DTLS_13);
}
}  // namespace

DatagramConnectionInternal::DatagramConnectionInternal(
    const Environment& env,
    std::unique_ptr<PortAllocator> port_allocator,
    absl::string_view transport_name,
    bool ice_controlling,
    scoped_refptr<RTCCertificate> certificate,
    std::unique_ptr<Observer> observer,
    WireProtocol wire_protocol,
    std::unique_ptr<IceTransportInternal> custom_ice_transport_internal)
    : wire_protocol_(wire_protocol),
      port_allocator_(std::move(port_allocator)),
      transport_channel_(
          custom_ice_transport_internal
              ? std::move(custom_ice_transport_internal)
              : P2PTransportChannel::Create(
                    transport_name,
                    ICE_CANDIDATE_COMPONENT_RTP,
                    CreateIceTransportInit(env, port_allocator_.get()))),
      dtls_transport_(make_ref_counted<DtlsTransport>(
          CreateDtlsTransportInternal(env, transport_channel_.get()))),
      dtls_srtp_transport_(
          wire_protocol_ == WireProtocol::kDtlsSrtp
              ? std::make_unique<DtlsSrtpTransport>(/*rtcp_mux_enabled=*/true,
                                                    env.field_trials())
              : nullptr),
      observer_(std::move(observer)) {
  RTC_CHECK(observer_);

  if (wire_protocol_ == WireProtocol::kDtls) {
    dtls_transport_->internal()->RegisterReceivedPacketCallback(
        this, [this](PacketTransportInternal* transport,
                     const ReceivedIpPacket& packet) {
          this->OnDtlsPacket(CopyOnWriteBuffer(packet.payload().data(),
                                               packet.payload().size()));
        });
  } else {
    dtls_srtp_transport_->SetDtlsTransports(dtls_transport_->internal(),
                                            /*rtcp_dtls_transport=*/nullptr);
  }

  dtls_transport_->ice_transport()->internal()->SubscribeCandidateGathered(
      std::bind_front(&DatagramConnectionInternal::OnCandidateGathered, this));

  if (wire_protocol_ == WireProtocol::kDtls) {
    dtls_transport_->internal()->SubscribeWritableState(
        this,
        [this](bool is_writable) { this->OnWritableStatePossiblyChanged(); });
  } else {
    dtls_srtp_transport_->SubscribeWritableState(
        this, [this](bool) { this->OnWritableStatePossiblyChanged(); });
  }

  transport_channel_->SubscribeIceTransportStateChanged(
      [this](IceTransportInternal* transport) {
        if (transport->GetIceTransportState() ==
            webrtc::IceTransportState::kFailed) {
          OnConnectionError();
        }
      });
  dtls_transport_->internal()->SubscribeDtlsHandshakeError(
      [this](webrtc::SSLHandshakeError) { OnConnectionError(); });

  // TODO(crbug.com/443019066): Bind to SetCandidateErrorCallback() and
  // propagate back to the Observer.
  constexpr int kIceUfragLength = 16;
  std::string ufrag = CreateRandomString(kIceUfragLength);
  std::string icepw = CreateRandomString(ICE_PWD_LENGTH);
  dtls_transport_->ice_transport()->internal()->SetIceParameters(
      IceParameters(ufrag, icepw,
                    /*ice_renomination=*/false));
  dtls_transport_->ice_transport()->internal()->SetIceRole(
      ice_controlling ? ICEROLE_CONTROLLING : ICEROLE_CONTROLLED);
  dtls_transport_->ice_transport()->internal()->MaybeStartGathering();

  if (wire_protocol_ == WireProtocol::kDtlsSrtp) {
    // Match everything for our fixed SSRC (should be everything).
    RtpDemuxerCriteria demuxer_criteria(/*mid=*/"");
    demuxer_criteria.ssrcs().insert(kDatagramConnectionSsrc);
    dtls_srtp_transport_->RegisterRtpDemuxerSink(demuxer_criteria, this);
  }

  RTC_CHECK(dtls_transport_->internal()->SetLocalCertificate(certificate));
}

void DatagramConnectionInternal::SetRemoteIceParameters(
    const IceParameters& ice_parameters) {
  if (current_state_ != State::kActive) {
    // TODO(crbug.com/443019066): Propagate an error back to the caller.
    return;
  }

  dtls_transport_->ice_transport()->internal()->SetRemoteIceParameters(
      ice_parameters);
}

void DatagramConnectionInternal::AddRemoteCandidate(
    const Candidate& candidate) {
  if (current_state_ != State::kActive) {
    // TODO(crbug.com/443019066): Propagate an error back to the caller.
    return;
  }

  dtls_transport_->ice_transport()->internal()->AddRemoteCandidate(candidate);
}

bool DatagramConnectionInternal::Writable() {
  if (current_state_ != State::kActive) {
    return false;
  }
  if (wire_protocol_ == WireProtocol::kDtls) {
    return dtls_transport_->internal()->writable();
  }
  return dtls_transport_->ice_transport()->internal()->writable() &&
         dtls_srtp_transport_->IsSrtpActive();
}

void DatagramConnectionInternal::SetRemoteDtlsParameters(
    absl::string_view digestAlgorithm,
    const uint8_t* digest,
    size_t digest_len,
    SSLRole ssl_role) {
  if (current_state_ != State::kActive) {
    // TODO(crbug.com/443019066): Propagate an error back to the caller.
    return;
  }

  webrtc::SSLRole mapped_ssl_role =
      ssl_role == SSLRole::kClient ? SSL_CLIENT : SSL_SERVER;
  dtls_transport_->internal()->SetRemoteParameters(digestAlgorithm, digest,
                                                   digest_len, mapped_ssl_role);
}

bool DatagramConnectionInternal::SendPacket(ArrayView<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (current_state_ != State::kActive) {
    return false;
  }

  if (wire_protocol_ == WireProtocol::kDtls) {
    // Directly send the payload inside a DTLS packet.
    return dtls_transport_->internal()->SendPacket(
        reinterpret_cast<const char*>(data.data()), data.size(),
        AsyncSocketPacketOptions());
  }

  if (!dtls_srtp_transport_->IsSrtpActive()) {
    // TODO(crbug.com/443019066): Propagate an error back to the caller.
    RTC_LOG(LS_ERROR) << "Dropping packet on non-active DTLS";
    return false;
  }
  // TODO(crbug.com/443019066): Update this representation inside an SRTP
  // packet as the spec level discussions continue.
  RtpPacket packet;
  packet.SetSequenceNumber(next_seq_num_++);
  packet.SetTimestamp(next_ts_++);
  packet.SetSsrc(kDatagramConnectionSsrc);
  packet.SetPayload(data);
  CopyOnWriteBuffer buffer = packet.Buffer();
  // Provide the flag PF_SRTP_BYPASS as these packets are being encrypted by
  // SRTP, so should bypass DTLS encryption.
  return dtls_srtp_transport_->SendRtpPacket(&buffer,
                                             AsyncSocketPacketOptions(),
                                             /*flags=*/PF_SRTP_BYPASS);
}

void DatagramConnectionInternal::Terminate(
    absl::AnyInvocable<void()> terminate_complete_callback) {
  if (current_state_ != State::kActive) {
    terminate_complete_callback();
    return;
  }

  if (wire_protocol_ == WireProtocol::kDtlsSrtp) {
    dtls_srtp_transport_->UnregisterRtpDemuxerSink(this);
  }
  // TODO(crbug.com/443019066): Once we need asynchronous termination, set state
  // to TerminationInProgress here and Terminated later once done.
  current_state_ = State::kTerminated;
  terminate_complete_callback();
}
void DatagramConnectionInternal::OnCandidateGathered(
    IceTransportInternal*,
    const Candidate& candidate) {
  if (current_state_ != State::kActive) {
    return;
  }
  observer_->OnCandidateGathered(candidate);
}

void DatagramConnectionInternal::OnTransportWritableStateChanged(
    PacketTransportInternal*) {
  OnWritableStatePossiblyChanged();
}

void DatagramConnectionInternal::OnWritableStatePossiblyChanged() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (current_state_ != State::kActive) {
    return;
  }
  bool writable = Writable();
  if (last_writable_state_ != writable) {
    observer_->OnWritableChange();
    last_writable_state_ = writable;
  }
}

void DatagramConnectionInternal::OnConnectionError() {
  if (current_state_ != State::kActive) {
    return;
  }
  observer_->OnConnectionError();
}

void DatagramConnectionInternal::OnRtpPacket(const RtpPacketReceived& packet) {
  if (current_state_ != State::kActive) {
    return;
  }
  observer_->OnPacketReceived(packet.payload());
}

void DatagramConnectionInternal::OnDtlsPacket(CopyOnWriteBuffer packet) {
  if (current_state_ != State::kActive) {
    return;
  }
  observer_->OnPacketReceived(packet);
}

}  // namespace webrtc
