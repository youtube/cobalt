/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_transport.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/crypto/crypto_options.h"
#include "api/dtls_transport_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/stun.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_transport_state.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_writable_state.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "p2p/dtls/dtls_stun_piggyback_callbacks.h"
#include "p2p/dtls/dtls_stun_piggyback_controller.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "p2p/dtls/dtls_utils.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

template <typename Sink>
void AbslStringify(Sink& sink, DtlsTransportState state) {
  switch (state) {
    case DtlsTransportState::kNew:
      sink.Append("kNew");
      break;
    case DtlsTransportState::kConnecting:
      sink.Append("kConnecting");
      break;
    case DtlsTransportState::kConnected:
      sink.Append("kConnected");
      break;
    case DtlsTransportState::kClosed:
      sink.Append("kClosed");
      break;
    case DtlsTransportState::kFailed:
      sink.Append("kFailed");
      break;
    case DtlsTransportState::kNumValues:
      sink.Append("kNumValues");
      break;
  }
}

// We don't pull the RTP constants from rtputils.h, to avoid a layer violation.
constexpr size_t kMinRtpPacketLen = 12;

// Maximum number of pending packets in the queue. Packets are read immediately
// after they have been written, so a capacity of "1" is sufficient.
//
// However, this bug seems to indicate that's not the case: crbug.com/1063834
// So, temporarily increasing it to 2 to see if that makes a difference.
constexpr size_t kMaxPendingPackets = 2;

// Minimum and maximum values for the initial DTLS handshake timeout. We'll pick
// an initial timeout based on ICE RTT estimates, but clamp it to this range.
constexpr int kMinDtlsHandshakeTimeoutMs = 50;
constexpr int kMaxDtlsHandshakeTimeoutMs = 3000;
// This effectively disables the handshake timeout.
constexpr int kDisabledHandshakeTimeoutMs = 3600 * 1000 * 24;

constexpr uint32_t kMaxCachedClientHello = 4;

static bool IsRtpPacket(ArrayView<const uint8_t> payload) {
  const uint8_t* u = payload.data();
  return (payload.size() >= kMinRtpPacketLen && (u[0] & 0xC0) == 0x80);
}

StreamInterfaceChannel::StreamInterfaceChannel(
    IceTransportInternal* ice_transport)
    : ice_transport_(ice_transport),
      state_(SS_OPEN),
      packets_(kMaxPendingPackets, kMaxDtlsPacketLen) {}

void StreamInterfaceChannel::SetDtlsStunPiggybackController(
    DtlsStunPiggybackController* dtls_stun_piggyback_controller) {
  dtls_stun_piggyback_controller_ = dtls_stun_piggyback_controller;
}

StreamResult StreamInterfaceChannel::Read(ArrayView<uint8_t> buffer,
                                          size_t& read,
                                          int& /* error */) {
  RTC_DCHECK_RUN_ON(&callback_sequence_);

  if (state_ == SS_CLOSED)
    return SR_EOS;
  if (state_ == SS_OPENING)
    return SR_BLOCK;

  if (!packets_.ReadFront(buffer.data(), buffer.size(), &read)) {
    return SR_BLOCK;
  }

  return SR_SUCCESS;
}

StreamResult StreamInterfaceChannel::Write(ArrayView<const uint8_t> data,
                                           size_t& written,
                                           int& /* error */) {
  RTC_DCHECK_RUN_ON(&callback_sequence_);

  // If we use DTLS-in-STUN, DTLS packets will be sent as part of STUN
  // packets, they are captured by the controller.
  if (dtls_stun_piggyback_controller_) {
    dtls_stun_piggyback_controller_->CapturePacket(data);
  }

  AsyncSocketPacketOptions packet_options;
  ice_transport_->SendPacket(reinterpret_cast<const char*>(data.data()),
                             data.size(), packet_options);
  written = data.size();
  return SR_SUCCESS;
}

bool StreamInterfaceChannel::Flush() {
  RTC_DCHECK_RUN_ON(&callback_sequence_);

  if (dtls_stun_piggyback_controller_) {
    dtls_stun_piggyback_controller_->Flush();
  }
  return false;
}

bool StreamInterfaceChannel::OnPacketReceived(const char* data, size_t size) {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  if (packets_.size() > 0) {
    RTC_LOG(LS_WARNING) << "Packet already in queue.";
  }
  bool ret = packets_.WriteBack(data, size, nullptr);
  if (!ret) {
    // Somehow we received another packet before the SSLStreamAdapter read the
    // previous one out of our temporary buffer. In this case, we'll log an
    // error and still signal the read event, hoping that it will read the
    // packet currently in packets_.
    RTC_LOG(LS_ERROR) << "Failed to write packet to queue.";
  }
  FireEvent(SE_READ, 0);
  return ret;
}

StreamState StreamInterfaceChannel::GetState() const {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  return state_;
}

void StreamInterfaceChannel::Close() {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  packets_.Clear();
  state_ = SS_CLOSED;
}

DtlsTransportInternalImpl::DtlsTransportInternalImpl(
    IceTransportInternal* ice_transport,
    const CryptoOptions& crypto_options,
    RtcEventLog* event_log,
    SSLProtocolVersion max_version)
    : component_(ice_transport->component()),
      ice_transport_(ice_transport),
      downward_(nullptr),
      srtp_ciphers_(crypto_options.GetSupportedDtlsSrtpCryptoSuites()),
      ephemeral_key_exchange_cipher_groups_(
          crypto_options.ephemeral_key_exchange_cipher_groups.GetEnabled()),
      ssl_max_version_(max_version),
      event_log_(event_log),
      dtls_stun_piggyback_controller_(
          [this](ArrayView<const uint8_t> piggybacked_dtls_packet) {
            if (piggybacked_dtls_callback_ == nullptr) {
              return;
            }
            piggybacked_dtls_callback_(
                this,
                ReceivedIpPacket(piggybacked_dtls_packet, SocketAddress()));
          }) {
  RTC_DCHECK(ice_transport_);
  ConnectToIceTransport();
  if (auto field_trials = ice_transport_->field_trials()) {
    dtls_in_stun_ = field_trials->IsEnabled("WebRTC-IceHandshakeDtls");
  } else {
    // TODO (BUG=webrtc:367395350): Fix upstream testcase(s).
    RTC_DLOG(LS_ERROR) << "ice_transport_>field_trials() is NULL";
    dtls_in_stun_ = false;
  }
}

DtlsTransportInternalImpl::~DtlsTransportInternalImpl() {
  if (ice_transport_) {
    ice_transport_->ResetDtlsStunPiggybackCallbacks();
    ice_transport_->DeregisterReceivedPacketCallback(this);
  }
}

DtlsTransportState DtlsTransportInternalImpl::dtls_state() const {
  return dtls_state_;
}

const std::string& DtlsTransportInternalImpl::transport_name() const {
  return ice_transport_->transport_name();
}

int DtlsTransportInternalImpl::component() const {
  return component_;
}

bool DtlsTransportInternalImpl::IsDtlsActive() const {
  return dtls_active_;
}

bool DtlsTransportInternalImpl::SetLocalCertificate(
    const scoped_refptr<RTCCertificate>& certificate) {
  if (dtls_active_) {
    if (certificate == local_certificate_) {
      // This may happen during renegotiation.
      RTC_LOG(LS_INFO) << ToString() << ": Ignoring identical DTLS identity";
      return true;
    } else {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Can't change DTLS local identity in this state";
      return false;
    }
  }

  if (certificate) {
    local_certificate_ = certificate;
    dtls_active_ = true;
  } else {
    RTC_LOG(LS_INFO) << ToString()
                     << ": NULL DTLS identity supplied. Not doing DTLS";
  }

  return true;
}

scoped_refptr<RTCCertificate> DtlsTransportInternalImpl::GetLocalCertificate()
    const {
  return local_certificate_;
}

bool DtlsTransportInternalImpl::SetDtlsRole(SSLRole role) {
  if (dtls_) {
    RTC_DCHECK(dtls_role_);
    if (*dtls_role_ != role) {
      RTC_LOG(LS_ERROR)
          << "SSL Role can't be reversed after the session is setup.";
      return false;
    }
    return true;
  }

  dtls_role_ = role;
  return true;
}

bool DtlsTransportInternalImpl::GetDtlsRole(SSLRole* role) const {
  if (!dtls_role_) {
    return false;
  }
  *role = *dtls_role_;
  return true;
}

bool DtlsTransportInternalImpl::GetSslCipherSuite(int* cipher) const {
  if (dtls_state() != DtlsTransportState::kConnected) {
    return false;
  }

  return dtls_->GetSslCipherSuite(cipher);
}

std::optional<absl::string_view>
DtlsTransportInternalImpl::GetTlsCipherSuiteName() const {
  if (dtls_state() != DtlsTransportState::kConnected) {
    return std::nullopt;
  }
  return dtls_->GetTlsCipherSuiteName();
}

RTCError DtlsTransportInternalImpl::SetRemoteParameters(
    absl::string_view digest_alg,
    const uint8_t* digest,
    size_t digest_len,
    std::optional<SSLRole> role) {
  Buffer remote_fingerprint_value(digest, digest_len);
  bool is_dtls_restart =
      dtls_active_ && remote_fingerprint_value_ != remote_fingerprint_value;
  // Set SSL role. Role must be set before fingerprint is applied, which
  // initiates DTLS setup.
  if (role) {
    if (is_dtls_restart) {
      dtls_role_ = *role;
    } else {
      if (!SetDtlsRole(*role)) {
        return RTCError(RTCErrorType::INVALID_PARAMETER,
                        "Failed to set SSL role for the transport.");
      }
    }
  }
  // Apply remote fingerprint.
  if (!SetRemoteFingerprint(digest_alg, digest, digest_len)) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Failed to apply remote fingerprint.");
  }
  return RTCError::OK();
}

bool DtlsTransportInternalImpl::SetRemoteFingerprint(
    absl::string_view digest_alg,
    const uint8_t* digest,
    size_t digest_len) {
  Buffer remote_fingerprint_value(digest, digest_len);

  // Once we have the local certificate, the same remote fingerprint can be set
  // multiple times.
  if (dtls_active_ && remote_fingerprint_value_ == remote_fingerprint_value &&
      !digest_alg.empty()) {
    // This may happen during renegotiation.
    RTC_LOG(LS_INFO) << ToString()
                     << ": Ignoring identical remote DTLS fingerprint";
    return true;
  }

  // If the other side doesn't support DTLS, turn off `dtls_active_`.
  // TODO(deadbeef): Remove this. It's dangerous, because it relies on higher
  // level code to ensure DTLS is actually used, but there are tests that
  // depend on it, for the case where an m= section is rejected. In that case
  // SetRemoteFingerprint shouldn't even be called though.
  if (digest_alg.empty()) {
    RTC_DCHECK(!digest_len);
    RTC_LOG(LS_INFO) << ToString() << ": Other side didn't support DTLS.";
    dtls_active_ = false;
    return true;
  }

  // Otherwise, we must have a local certificate before setting remote
  // fingerprint.
  if (!dtls_active_) {
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Can't set DTLS remote settings in this state.";
    return false;
  }

  // At this point we know we are doing DTLS
  bool fingerprint_changing = remote_fingerprint_value_.size() > 0u;
  remote_fingerprint_value_ = std::move(remote_fingerprint_value);
  remote_fingerprint_algorithm_ = std::string(digest_alg);

  if (dtls_ && !fingerprint_changing) {
    // This can occur if DTLS is set up before a remote fingerprint is
    // received. For instance, if we set up DTLS due to receiving an early
    // ClientHello.
    SSLPeerCertificateDigestError err = dtls_->SetPeerCertificateDigest(
        remote_fingerprint_algorithm_, remote_fingerprint_value_);
    if (err != SSLPeerCertificateDigestError::NONE) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Couldn't set DTLS certificate digest.";
      set_dtls_state(DtlsTransportState::kFailed);
      // If the error is "verification failed", don't return false, because
      // this means the fingerprint was formatted correctly but didn't match
      // the certificate from the DTLS handshake. Thus the DTLS state should go
      // to "failed", but SetRemoteDescription shouldn't fail.
      return err == SSLPeerCertificateDigestError::VERIFICATION_FAILED;
    }
    return true;
  }

  // If the fingerprint is changing, we'll tear down the DTLS association and
  // create a new one, resetting our state.
  if (dtls_ && fingerprint_changing) {
    dtls_.reset(nullptr);
    set_dtls_state(DtlsTransportState::kNew);
    set_writable(false);
  }

  if (!SetupDtls()) {
    set_dtls_state(DtlsTransportState::kFailed);
    return false;
  }

  return true;
}

std::unique_ptr<SSLCertChain> DtlsTransportInternalImpl::GetRemoteSSLCertChain()
    const {
  if (!dtls_) {
    return nullptr;
  }

  return dtls_->GetPeerSSLCertChain();
}

bool DtlsTransportInternalImpl::ExportSrtpKeyingMaterial(
    ZeroOnFreeBuffer<uint8_t>& keying_material) {
  return dtls_ ? dtls_->ExportSrtpKeyingMaterial(keying_material) : false;
}

bool DtlsTransportInternalImpl::SetupDtls() {
  RTC_DCHECK(dtls_role_);

  dtls_in_stun_ = ice_transport_->config().dtls_handshake_in_stun;
  {
    auto downward = std::make_unique<StreamInterfaceChannel>(ice_transport_);
    StreamInterfaceChannel* downward_ptr = downward.get();

    if (dtls_in_stun_) {
      downward_ptr->SetDtlsStunPiggybackController(
          &dtls_stun_piggyback_controller_);
    }
    dtls_ = SSLStreamAdapter::Create(
        std::move(downward),
        [this](SSLHandshakeError error) { OnDtlsHandshakeError(error); },
        ice_transport_->field_trials());
    if (!dtls_) {
      RTC_LOG(LS_ERROR) << ToString() << ": Failed to create DTLS adapter.";
      return false;
    }
    downward_ = downward_ptr;
  }

  // TODO(jonaso,webrtc:367395350): Add more clever handling of MTU
  // (such as automatic packetization smoothing).
  if (dtls_in_stun_) {
    // - This is only needed when using PQC but we don't know that here.
    // - 800 is sufficiently small so that dtls pqc handshake packets
    // can get put into STUN attributes.
    const int kDtlsMtu = 800;
    dtls_->SetMTU(kDtlsMtu);
  }

  dtls_->SetIdentity(local_certificate_->identity()->Clone());
  dtls_->SetMaxProtocolVersion(ssl_max_version_);
  dtls_->SetServerRole(*dtls_role_);
  dtls_->SetEventCallback(
      [this](int events, int err) { OnDtlsEvent(events, err); });
  if (remote_fingerprint_value_.size() &&
      dtls_->SetPeerCertificateDigest(remote_fingerprint_algorithm_,
                                      remote_fingerprint_value_) !=
          SSLPeerCertificateDigestError::NONE) {
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Couldn't set DTLS certificate digest.";
    return false;
  }

  // Set up DTLS-SRTP, if it's been enabled.
  if (!srtp_ciphers_.empty()) {
    if (!dtls_->SetDtlsSrtpCryptoSuites(srtp_ciphers_)) {
      RTC_LOG(LS_ERROR) << ToString() << ": Couldn't set DTLS-SRTP ciphers.";
      return false;
    }
  } else {
    RTC_LOG(LS_INFO) << ToString() << ": Not using DTLS-SRTP.";
  }

  if (!dtls_->SetSslGroupIds(ephemeral_key_exchange_cipher_groups_)) {
    RTC_LOG(LS_ERROR) << ToString() << ": Couldn't set DTLS SSL Group Ids.";
    return false;
  }

  RTC_LOG(LS_INFO) << ToString()
                   << ": DTLS setup complete, dtls_in_stun: " << dtls_in_stun_;

  // If the underlying ice_transport is already writable at this point, we may
  // be able to start DTLS right away.
  MaybeStartDtls();
  return true;
}

bool DtlsTransportInternalImpl::GetSrtpCryptoSuite(int* cipher) const {
  if (dtls_state() != DtlsTransportState::kConnected) {
    return false;
  }

  return dtls_->GetDtlsSrtpCryptoSuite(cipher);
}

bool DtlsTransportInternalImpl::GetSslVersionBytes(int* version) const {
  if (dtls_state() != DtlsTransportState::kConnected) {
    return false;
  }

  return dtls_->GetSslVersionBytes(version);
}

uint16_t DtlsTransportInternalImpl::GetSslGroupId() const {
  return dtls_->GetSslGroupId();
}

uint16_t DtlsTransportInternalImpl::GetSslPeerSignatureAlgorithm() const {
  if (dtls_state() != DtlsTransportState::kConnected) {
    return kSslSignatureAlgorithmUnknown;  // "not applicable"
  }
  return dtls_->GetPeerSignatureAlgorithm();
}

// Called from upper layers to send a media packet.
int DtlsTransportInternalImpl::SendPacket(
    const char* data,
    size_t size,
    const AsyncSocketPacketOptions& options,
    int flags) {
  if (!dtls_active_) {
    // Not doing DTLS.
    return ice_transport_->SendPacket(data, size, options);
  }

  switch (dtls_state()) {
    case DtlsTransportState::kNew:
      // Can't send data until the connection is active.
      // TODO(ekr@rtfm.com): assert here if dtls_ is NULL?
      return -1;
    case DtlsTransportState::kConnecting:
      // Can't send data until the connection is active.
      return -1;
    case DtlsTransportState::kConnected:
      if (flags & PF_SRTP_BYPASS) {
        RTC_DCHECK(!srtp_ciphers_.empty());
        if (!IsRtpPacket(
                MakeArrayView(reinterpret_cast<const uint8_t*>(data), size))) {
          return -1;
        }

        return ice_transport_->SendPacket(data, size, options);
      } else {
        size_t written;
        int error;
        return (dtls_->WriteAll(
                    MakeArrayView(reinterpret_cast<const uint8_t*>(data), size),
                    written, error) == SR_SUCCESS)
                   ? static_cast<int>(size)
                   : -1;
      }
    case DtlsTransportState::kFailed:
      // Can't send anything when we're failed.
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Couldn't send packet due to "
                           "webrtc::DtlsTransportState::kFailed.";
      return -1;
    case DtlsTransportState::kClosed:
      // Can't send anything when we're closed.
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Couldn't send packet due to "
                           "webrtc::DtlsTransportState::kClosed.";
      return -1;
    default:
      RTC_DCHECK_NOTREACHED();
      return -1;
  }
}

IceTransportInternal* DtlsTransportInternalImpl::ice_transport() {
  return ice_transport_;
}

bool DtlsTransportInternalImpl::IsDtlsConnected() {
  return dtls_ && dtls_->IsTlsConnected();
}

bool DtlsTransportInternalImpl::receiving() const {
  return receiving_;
}

bool DtlsTransportInternalImpl::writable() const {
  return writable_;
}

int DtlsTransportInternalImpl::GetError() {
  return ice_transport_->GetError();
}

std::optional<NetworkRoute> DtlsTransportInternalImpl::network_route() const {
  return ice_transport_->network_route();
}

bool DtlsTransportInternalImpl::GetOption(Socket::Option opt, int* value) {
  return ice_transport_->GetOption(opt, value);
}

int DtlsTransportInternalImpl::SetOption(Socket::Option opt, int value) {
  return ice_transport_->SetOption(opt, value);
}

void DtlsTransportInternalImpl::ConnectToIceTransport() {
  RTC_DCHECK(ice_transport_);
  ice_transport_->SignalWritableState.connect(
      this, &DtlsTransportInternalImpl::OnWritableState);
  ice_transport_->RegisterReceivedPacketCallback(
      this,
      [&](PacketTransportInternal* transport, const ReceivedIpPacket& packet) {
        OnReadPacket(transport, packet, /* piggybacked= */ false);
      });

  ice_transport_->SignalSentPacket.connect(
      this, &DtlsTransportInternalImpl::OnSentPacket);
  ice_transport_->SignalReadyToSend.connect(
      this, &DtlsTransportInternalImpl::OnReadyToSend);
  ice_transport_->SignalReceivingState.connect(
      this, &DtlsTransportInternalImpl::OnReceivingState);
  ice_transport_->SignalNetworkRouteChanged.connect(
      this, &DtlsTransportInternalImpl::OnNetworkRouteChanged);
  ice_transport_->SetDtlsStunPiggybackCallbacks(
      DtlsStunPiggybackCallbacks(
          [&](auto stun_message_type) {
            std::optional<absl::string_view> data;
            std::optional<absl::string_view> ack;
            if (dtls_in_stun_) {
              data = dtls_stun_piggyback_controller_.GetDataToPiggyback(
                  stun_message_type);
              ack = dtls_stun_piggyback_controller_.GetAckToPiggyback(
                  stun_message_type);
            }
            return std::make_pair(data, ack);
          },
          [&](auto data, auto ack) {
            if (!dtls_in_stun_) {
              return;
            }
            dtls_stun_piggyback_controller_.ReportDataPiggybacked(data, ack);
          }));
  SetPiggybackDtlsDataCallback([this](PacketTransportInternal* transport,
                                      const ReceivedIpPacket& packet) {
    RTC_DCHECK(dtls_active_);
    RTC_DCHECK(IsDtlsPacket(packet.payload()));
    if (!dtls_active_) {
      // Not doing DTLS.
      return;
    }
    if (!IsDtlsPacket(packet.payload())) {
      return;
    }
    OnReadPacket(ice_transport_, packet, /* piggybacked= */ true);
  });
}

// The state transition logic here is as follows:
// (1) If we're not doing DTLS-SRTP, then the state is just the
//     state of the underlying impl()
// (2) If we're doing DTLS-SRTP:
//     - Prior to the DTLS handshake, the state is neither receiving nor
//       writable
//     - When the impl goes writable for the first time we
//       start the DTLS handshake
//     - Once the DTLS handshake completes, the state is that of the
//       impl again
void DtlsTransportInternalImpl::OnWritableState(
    PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(transport == ice_transport_);
  RTC_LOG(LS_INFO) << ToString() << ": ice_transport writable state changed to "
                   << ice_transport_->writable()
                   << " dtls_state: " << dtls_state();

  if (!ice_has_been_writable_) {
    // Ice starts as not writable. The first time this method is called, it
    // should be when ice change to writable = true.
    RTC_DCHECK(ice_transport_->writable());
  }
  bool first_ice_writable = !ice_has_been_writable_;
  ice_has_been_writable_ = true;

  if (!dtls_active_) {
    // Not doing DTLS.
    // Note: SignalWritableState fired by set_writable.
    set_writable(ice_transport_->writable());
    return;
  }

  switch (dtls_state()) {
    case DtlsTransportState::kNew:
      MaybeStartDtls();
      break;
    case DtlsTransportState::kConnected:
      // Note: SignalWritableState fired by set_writable.
      if (dtls_in_stun_ && dtls_ && first_ice_writable) {
        // Dtls1.3 has one remaining packet after it has become kConnected (?),
        // make sure that this packet is sent too.
        ConfigureHandshakeTimeout();
        PeriodicRetransmitDtlsPacketUntilDtlsConnected();
      }
      set_writable(ice_transport_->writable());
      break;
    case DtlsTransportState::kConnecting:
      if (dtls_in_stun_ && dtls_) {
        // If DTLS piggybacking is enabled, we set the timeout
        // on the DTLS object (which is then different from the
        // inital kDisabledHandshakeTimeoutMs)
        ConfigureHandshakeTimeout();
        PeriodicRetransmitDtlsPacketUntilDtlsConnected();
      }
      break;
    case DtlsTransportState::kFailed:
      // Should not happen. Do nothing.
      RTC_LOG(LS_ERROR) << ToString()
                        << ": OnWritableState() called in state "
                           "webrtc::DtlsTransportState::kFailed.";
      break;
    case DtlsTransportState::kClosed:
      // Should not happen. Do nothing.
      RTC_LOG(LS_ERROR) << ToString()
                        << ": OnWritableState() called in state "
                           "webrtc::DtlsTransportState::kClosed.";
      break;
    case DtlsTransportState::kNumValues:
      RTC_DCHECK_NOTREACHED();
      break;
  }
}

void DtlsTransportInternalImpl::OnReceivingState(
    PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(transport == ice_transport_);
  RTC_LOG(LS_VERBOSE) << ToString()
                      << ": ice_transport "
                         "receiving state changed to "
                      << ice_transport_->receiving();
  if (!dtls_active_ || dtls_state() == DtlsTransportState::kConnected) {
    // Note: SignalReceivingState fired by set_receiving.
    set_receiving(ice_transport_->receiving());
  }
}

void DtlsTransportInternalImpl::OnReadPacket(PacketTransportInternal* transport,
                                             const ReceivedIpPacket& packet,
                                             bool piggybacked) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(transport == ice_transport_);

  if (!dtls_active_) {
    // Not doing DTLS.
    NotifyPacketReceived(packet);
    return;
  }

  switch (dtls_state()) {
    case DtlsTransportState::kNew:
      if (dtls_) {
        RTC_LOG(LS_INFO) << ToString()
                         << ": Packet received before DTLS started.";
      } else {
        RTC_LOG(LS_WARNING) << ToString()
                            << ": Packet received before we know if we are "
                               "doing DTLS or not.";
      }
      // Cache a client hello packet received before DTLS has actually started.
      if (IsDtlsClientHelloPacket(packet.payload())) {
        RTC_LOG(LS_INFO) << ToString()
                         << ": Caching DTLS ClientHello packet until DTLS is "
                            "started.";
        cached_client_hello_.AddIfUnique(packet.payload());
        cached_client_hello_.Prune(kMaxCachedClientHello);
        // If we haven't started setting up DTLS yet (because we don't have a
        // remote fingerprint/role), we can use the client hello as a clue that
        // the peer has chosen the client role, and proceed with the handshake.
        // The fingerprint will be verified when it's set.
        if (!dtls_ && local_certificate_) {
          SetDtlsRole(SSL_SERVER);
          SetupDtls();
        }
      } else {
        RTC_LOG(LS_INFO) << ToString()
                         << ": Not a DTLS ClientHello packet; dropping.";
      }
      break;

    case DtlsTransportState::kConnecting:
    case DtlsTransportState::kConnected:
      // We should only get DTLS or SRTP packets; STUN's already been demuxed.
      // Is this potentially a DTLS packet?
      if (IsDtlsPacket(packet.payload())) {
        if (!HandleDtlsPacket(packet.payload())) {
          RTC_LOG(LS_ERROR) << ToString() << ": Failed to handle DTLS packet.";
          return;
        }
      } else {
        // Not a DTLS packet; our handshake should be complete by now.
        if (dtls_state() != DtlsTransportState::kConnected) {
          RTC_LOG(LS_ERROR) << ToString()
                            << ": Received non-DTLS packet before DTLS "
                               "complete.";
          return;
        }

        // And it had better be a SRTP packet.
        if (!IsRtpPacket(packet.payload())) {
          RTC_LOG(LS_ERROR)
              << ToString() << ": Received unexpected non-DTLS packet.";
          return;
        }

        // Sanity check.
        RTC_DCHECK(!srtp_ciphers_.empty());

        // Signal this upwards as a bypass packet.
        NotifyPacketReceived(
            packet.CopyAndSet(ReceivedIpPacket::kSrtpEncrypted));
      }
      break;
    case DtlsTransportState::kFailed:
    case DtlsTransportState::kClosed:
    case DtlsTransportState::kNumValues:
      // This shouldn't be happening. Drop the packet.
      break;
  }
}

void DtlsTransportInternalImpl::OnSentPacket(
    PacketTransportInternal* /* transport */,
    const SentPacketInfo& sent_packet) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalSentPacket(this, sent_packet);
}

void DtlsTransportInternalImpl::OnReadyToSend(
    PacketTransportInternal* /* transport */) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (writable()) {
    SignalReadyToSend(this);
  }
}

void DtlsTransportInternalImpl::OnDtlsEvent(int sig, int err) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(dtls_);

  if (sig & SE_OPEN) {
    // This is the first time.
    RTC_LOG(LS_INFO) << ToString() << ": DTLS handshake complete.";
    // The check for OPEN shouldn't be necessary but let's make
    // sure we don't accidentally frob the state if it's closed.
    if (dtls_->GetState() == SS_OPEN) {
      int ssl_version_bytes;
      bool ret = dtls_->GetSslVersionBytes(&ssl_version_bytes);
      RTC_DCHECK(ret);
      dtls_stun_piggyback_controller_.SetDtlsHandshakeComplete(
          dtls_role_ == SSL_CLIENT, ssl_version_bytes == kDtls13VersionBytes);
      downward_->SetDtlsStunPiggybackController(nullptr);
      set_dtls_state(DtlsTransportState::kConnected);
      set_writable(true);
    }
  }
  if (sig & SE_READ) {
    uint8_t buf[kMaxDtlsPacketLen];
    size_t read;
    int read_error;
    StreamResult ret;
    // The underlying DTLS stream may have received multiple DTLS records in
    // one packet, so read all of them.
    do {
      ret = dtls_->Read(buf, read, read_error);
      if (ret == SR_SUCCESS) {
        // TODO(bugs.webrtc.org/15368): It should be possible to use information
        // from the original packet here to populate socket address and
        // timestamp.
        NotifyPacketReceived(ReceivedIpPacket(
            MakeArrayView(buf, read), SocketAddress(),
            Timestamp::Micros(TimeMicros()), EcnMarking::kNotEct,
            ReceivedIpPacket::kDtlsDecrypted));
      } else if (ret == SR_EOS) {
        // Remote peer shut down the association with no error.
        RTC_LOG(LS_INFO) << ToString() << ": DTLS transport closed by remote";
        set_writable(false);
        set_dtls_state(DtlsTransportState::kClosed);
        NotifyOnClose();
      } else if (ret == SR_ERROR) {
        // Remote peer shut down the association with an error.
        RTC_LOG(LS_INFO)
            << ToString()
            << ": Closed by remote with DTLS transport error, code="
            << read_error;
        set_writable(false);
        set_dtls_state(DtlsTransportState::kFailed);
        NotifyOnClose();
      }
    } while (ret == SR_SUCCESS);
  }
  if (sig & SE_CLOSE) {
    RTC_DCHECK(sig == SE_CLOSE);  // SE_CLOSE should be by itself.
    set_writable(false);
    if (!err) {
      RTC_LOG(LS_INFO) << ToString() << ": DTLS transport closed";
      set_dtls_state(DtlsTransportState::kClosed);
    } else {
      RTC_LOG(LS_INFO) << ToString() << ": DTLS transport error, code=" << err;
      set_dtls_state(DtlsTransportState::kFailed);
    }
  }
}

void DtlsTransportInternalImpl::OnNetworkRouteChanged(
    std::optional<NetworkRoute> network_route) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalNetworkRouteChanged(network_route);
}

void DtlsTransportInternalImpl::MaybeStartDtls() {
  RTC_DCHECK(ice_transport_);
  //  When adding the DTLS handshake in STUN we want to call StartSSL even
  //  before the ICE transport is ready.
  if (dtls_ && (ice_transport_->writable() || dtls_in_stun_)) {
    ConfigureHandshakeTimeout();

    if (dtls_->StartSSL()) {
      // This should never fail:
      // Because we are operating in a nonblocking mode and all
      // incoming packets come in via OnReadPacket(), which rejects
      // packets in this state, the incoming queue must be empty. We
      // ignore write errors, thus any errors must be because of
      // configuration and therefore are our fault.
      RTC_DCHECK_NOTREACHED() << "StartSSL failed.";
      RTC_LOG(LS_ERROR) << ToString() << ": Couldn't start DTLS handshake";
      set_dtls_state(DtlsTransportState::kFailed);
      return;
    }
    RTC_LOG(LS_INFO)
        << ToString()
        << ": DtlsTransportInternalImpl: Started DTLS handshake active="
        << IsDtlsActive()
        << " role=" << (*dtls_role_ == SSL_SERVER ? "server" : "client");
    set_dtls_state(DtlsTransportState::kConnecting);
    // Now that the handshake has started, we can process a cached ClientHello
    // (if one exists).
    if (!cached_client_hello_.empty()) {
      if (*dtls_role_ == SSL_SERVER) {
        int size = cached_client_hello_.size();
        RTC_LOG(LS_INFO) << ToString() << ": Handling #" << size
                         << " cached DTLS ClientHello packet(s).";
        for (int i = 0; i < size; i++) {
          if (!HandleDtlsPacket(cached_client_hello_.GetNext())) {
            RTC_LOG(LS_ERROR)
                << ToString() << ": Failed to handle DTLS packet.";
            break;
          }
        }
      } else {
        RTC_LOG(LS_WARNING) << ToString()
                            << ": Discarding cached DTLS ClientHello packet "
                               "because we don't have the server role.";
      }
      cached_client_hello_.clear();
    }
  }
}

// Called from OnReadPacket when a DTLS packet is received.
bool DtlsTransportInternalImpl::HandleDtlsPacket(
    ArrayView<const uint8_t> payload) {
  // Pass to the StreamInterfaceChannel which ends up being passed to the DTLS
  // stack.
  return downward_->OnPacketReceived(
      reinterpret_cast<const char*>(payload.data()), payload.size());
}

void DtlsTransportInternalImpl::set_receiving(bool receiving) {
  if (receiving_ == receiving) {
    return;
  }
  receiving_ = receiving;
  SignalReceivingState(this);
}

void DtlsTransportInternalImpl::set_writable(bool writable) {
  if (writable_ == writable) {
    return;
  }
  if (writable && !ice_has_been_writable_) {
    // Wait with reporting writable until ICE has become writable once,
    // so as to not confuse other part of stack (such as sctp).
    RTC_DCHECK(dtls_in_stun_);
    RTC_LOG(LS_INFO)
        << ToString()
        << ": defer set_writable(true) until ICE has become writable once";
    return;
  }

  if (event_log_) {
    event_log_->Log(std::make_unique<RtcEventDtlsWritableState>(writable));
  }
  RTC_LOG(LS_VERBOSE) << ToString() << ": set_writable to: " << writable;
  writable_ = writable;
  if (writable_) {
    SignalReadyToSend(this);
  }
  SignalWritableState(this);
}

void DtlsTransportInternalImpl::set_dtls_state(DtlsTransportState state) {
  if (dtls_state_ == state) {
    return;
  }
  if (event_log_) {
    event_log_->Log(std::make_unique<RtcEventDtlsTransportState>(state));
  }
  RTC_LOG(LS_VERBOSE) << ToString() << ": set_dtls_state from:"
                      << static_cast<int>(dtls_state_) << " to "
                      << static_cast<int>(state);
  dtls_state_ = state;
  if (dtls_state_ == DtlsTransportState::kFailed) {
    dtls_stun_piggyback_controller_.SetDtlsFailed();
  }
  SendDtlsState(this, state);
}

void DtlsTransportInternalImpl::OnDtlsHandshakeError(SSLHandshakeError error) {
  SendDtlsHandshakeError(error);
}

int ComputeRetransmissionTimeout(int rtt_ms) {
  return std::max(kMinDtlsHandshakeTimeoutMs,
                  std::min(kMaxDtlsHandshakeTimeoutMs, 2 * (rtt_ms)));
}

void DtlsTransportInternalImpl::ConfigureHandshakeTimeout() {
  RTC_DCHECK(dtls_);
  std::optional<int> rtt_ms = ice_transport_->GetRttEstimate();
  if (rtt_ms) {
    // Limit the timeout to a reasonable range in case the ICE RTT takes
    // extreme values.
    int initial_timeout_ms = ComputeRetransmissionTimeout(*rtt_ms);
    RTC_LOG(LS_INFO) << ToString() << ": configuring DTLS handshake timeout "
                     << initial_timeout_ms << "ms based on ICE RTT " << *rtt_ms;
    dtls_->SetInitialRetransmissionTimeout(initial_timeout_ms);
  } else if (dtls_in_stun_) {
    // Configure a very high timeout to effectively disable the DTLS timeout
    // and avoid fragmented resends. This is ok since DTLS-in-STUN caches
    // the handshake pacets and resends them using the pacing of ICE.
    RTC_LOG(LS_INFO) << ToString() << ": configuring DTLS handshake timeout "
                     << kDisabledHandshakeTimeoutMs << "ms for DTLS-in-STUN";
    dtls_->SetInitialRetransmissionTimeout(kDisabledHandshakeTimeoutMs);
  } else {
    RTC_LOG(LS_INFO)
        << ToString()
        << ": no RTT estimate - using default DTLS handshake timeout";
  }
}

void DtlsTransportInternalImpl::SetPiggybackDtlsDataCallback(
    absl::AnyInvocable<void(PacketTransportInternal* transport,
                            const ReceivedIpPacket& packet)> callback) {
  RTC_DCHECK(callback == nullptr || !piggybacked_dtls_callback_);
  piggybacked_dtls_callback_ = std::move(callback);
}

bool DtlsTransportInternalImpl::IsDtlsPiggybackSupportedByPeer() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(ice_transport_);
  return dtls_in_stun_ && (dtls_stun_piggyback_controller_.state() !=
                           DtlsStunPiggybackController::State::OFF);
}

bool DtlsTransportInternalImpl::WasDtlsCompletedByPiggybacking() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(ice_transport_);
  return dtls_in_stun_ && (dtls_stun_piggyback_controller_.state() ==
                               DtlsStunPiggybackController::State::COMPLETE ||
                           dtls_stun_piggyback_controller_.state() ==
                               DtlsStunPiggybackController::State::PENDING);
}

// TODO (jonaso, webrtc:367395350): Switch to upcoming
// DTLSv1_set_timeout_duration. Remove once we can get DTLS to handle
// retransmission also when handshake is not complete but we become writable
// (e.g. by setting a good timeout).
void DtlsTransportInternalImpl::
    PeriodicRetransmitDtlsPacketUntilDtlsConnected() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (pending_periodic_retransmit_dtls_packet_ == true) {
    // PeriodicRetransmitDtlsPacketUntilDtlsConnected is called in two places
    // a) Either by PostTask, where pending_ping_until_dtls_connected_ is FALSE
    // b) When Ice get connected, in which it is unknown if
    // pending_periodic_retransmit_dtls_packet_.
    return;
  }
  if (ice_transport_->writable() && dtls_in_stun_) {
    auto data_to_send = dtls_stun_piggyback_controller_.GetDataToPiggyback(
        STUN_BINDING_INDICATION);
    if (!data_to_send) {
      // No data to send, we're done.
      return;
    }
    AsyncSocketPacketOptions packet_options;
    ice_transport_->SendPacket(data_to_send->data(), data_to_send->size(),
                               packet_options, /* flags= */ 0);
  }

  const auto rtt_ms = ice_transport_->GetRttEstimate().value_or(100);
  const int delay_ms = ComputeRetransmissionTimeout(rtt_ms);

  // Set pending before we post task.
  pending_periodic_retransmit_dtls_packet_ = true;
  Thread::Current()->PostDelayedHighPrecisionTask(
      SafeTask(safety_flag_.flag(),
               [this] {
                 RTC_DCHECK_RUN_ON(&thread_checker_);
                 // Clear pending then the PostTask runs.
                 pending_periodic_retransmit_dtls_packet_ = false;
                 PeriodicRetransmitDtlsPacketUntilDtlsConnected();
               }),
      TimeDelta::Millis(delay_ms));
  RTC_LOG(LS_INFO) << ToString()
                   << ": Scheduled retransmit of DTLS packet, delay_ms: "
                   << delay_ms;
}

int DtlsTransportInternalImpl::GetRetransmissionCount() const {
  if (!dtls_) {
    return 0;
  }
  return dtls_->GetRetransmissionCount();
}

int DtlsTransportInternalImpl::GetStunDataCount() const {
  if (!dtls_in_stun_) {
    return 0;
  }
  return dtls_stun_piggyback_controller_.GetCountOfReceivedData();
}

}  // namespace webrtc
