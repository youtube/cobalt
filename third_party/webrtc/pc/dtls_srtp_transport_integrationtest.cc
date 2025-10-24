/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>

#include "absl/strings/str_cat.h"
#include "api/crypto/crypto_options.h"
#include "api/dtls_transport_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "call/rtp_demuxer.h"
#include "media/base/fake_rtp.h"
#include "p2p/base/transport_description.h"
#include "p2p/dtls/dtls_transport.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "p2p/test/fake_ice_transport.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/srtp_transport.h"
#include "pc/test/rtp_transport_test_util.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"
#include "test/wait_until.h"

const int kRtpAuthTagLen = 10;
static const int kTimeout = 10000;

/* A test using a DTLS-SRTP transport on one side and
 * SrtpTransport+DtlsTransport on the other side, connected by a
 * FakeIceTransport.
 */
class DtlsSrtpTransportIntegrationTest : public ::testing::Test {
 protected:
  DtlsSrtpTransportIntegrationTest()
      : client_ice_transport_(MakeIceTransport(webrtc::ICEROLE_CONTROLLING)),
        server_ice_transport_(MakeIceTransport(webrtc::ICEROLE_CONTROLLED)),
        client_dtls_transport_(MakeDtlsTransport(client_ice_transport_.get())),
        server_dtls_transport_(MakeDtlsTransport(server_ice_transport_.get())),
        client_certificate_(MakeCertificate()),
        server_certificate_(MakeCertificate()),
        dtls_srtp_transport_(false, field_trials_),
        srtp_transport_(false, field_trials_) {
    dtls_srtp_transport_.SetDtlsTransports(server_dtls_transport_.get(),
                                           nullptr);
    srtp_transport_.SetRtpPacketTransport(client_ice_transport_.get());

    webrtc::RtpDemuxerCriteria demuxer_criteria;
    demuxer_criteria.payload_types() = {0x00};
    dtls_srtp_transport_.RegisterRtpDemuxerSink(demuxer_criteria,
                                                &dtls_srtp_transport_observer_);
    srtp_transport_.RegisterRtpDemuxerSink(demuxer_criteria,
                                           &srtp_transport_observer_);
  }
  ~DtlsSrtpTransportIntegrationTest() {
    dtls_srtp_transport_.UnregisterRtpDemuxerSink(
        &dtls_srtp_transport_observer_);
    srtp_transport_.UnregisterRtpDemuxerSink(&srtp_transport_observer_);
  }

  webrtc::scoped_refptr<webrtc::RTCCertificate> MakeCertificate() {
    return webrtc::RTCCertificate::Create(
        webrtc::SSLIdentity::Create("test", webrtc::KT_DEFAULT));
  }
  std::unique_ptr<webrtc::FakeIceTransport> MakeIceTransport(
      webrtc::IceRole role) {
    auto ice_transport = std::make_unique<webrtc::FakeIceTransport>(
        "fake_" + absl::StrCat(static_cast<int>(role)), 0);
    ice_transport->SetAsync(true);
    ice_transport->SetAsyncDelay(0);
    ice_transport->SetIceRole(role);
    return ice_transport;
  }

  std::unique_ptr<webrtc::DtlsTransportInternalImpl> MakeDtlsTransport(
      webrtc::FakeIceTransport* ice_transport) {
    return std::make_unique<webrtc::DtlsTransportInternalImpl>(
        ice_transport, webrtc::CryptoOptions(),
        /*event_log=*/nullptr, webrtc::SSL_PROTOCOL_DTLS_12);
  }
  void SetRemoteFingerprintFromCert(
      webrtc::DtlsTransportInternalImpl* transport,
      const webrtc::scoped_refptr<webrtc::RTCCertificate>& cert) {
    std::unique_ptr<webrtc::SSLFingerprint> fingerprint =
        webrtc::SSLFingerprint::CreateFromCertificate(*cert);

    transport->SetRemoteParameters(
        fingerprint->algorithm,
        reinterpret_cast<const uint8_t*>(fingerprint->digest.data()),
        fingerprint->digest.size(), std::nullopt);
  }

  void Connect() {
    client_dtls_transport_->SetLocalCertificate(client_certificate_);
    client_dtls_transport_->SetDtlsRole(webrtc::SSL_SERVER);
    server_dtls_transport_->SetLocalCertificate(server_certificate_);
    server_dtls_transport_->SetDtlsRole(webrtc::SSL_CLIENT);

    SetRemoteFingerprintFromCert(server_dtls_transport_.get(),
                                 client_certificate_);
    SetRemoteFingerprintFromCert(client_dtls_transport_.get(),
                                 server_certificate_);

    // Wire up the ICE and transport.
    client_ice_transport_->SetDestination(server_ice_transport_.get());

    // Wait for the DTLS connection to be up.
    EXPECT_THAT(webrtc::WaitUntil(
                    [&] {
                      return client_dtls_transport_->writable() &&
                             server_dtls_transport_->writable();
                    },
                    ::testing::IsTrue(),
                    {.timeout = webrtc::TimeDelta::Millis(kTimeout),
                     .clock = &fake_clock_}),
                webrtc::IsRtcOk());
    EXPECT_EQ(client_dtls_transport_->dtls_state(),
              webrtc::DtlsTransportState::kConnected);
    EXPECT_EQ(server_dtls_transport_->dtls_state(),
              webrtc::DtlsTransportState::kConnected);
  }
  void SetupClientKeysManually() {
    // Setup the client-side SRTP transport with the keys from the server DTLS
    // transport.
    int selected_crypto_suite;
    ASSERT_TRUE(
        server_dtls_transport_->GetSrtpCryptoSuite(&selected_crypto_suite));
    int key_len;
    int salt_len;
    ASSERT_TRUE(webrtc::GetSrtpKeyAndSaltLengths((selected_crypto_suite),
                                                 &key_len, &salt_len));

    // Extract the keys. The order depends on the role!
    webrtc::ZeroOnFreeBuffer<uint8_t> dtls_buffer(key_len * 2 + salt_len * 2);
    ASSERT_TRUE(server_dtls_transport_->ExportSrtpKeyingMaterial(dtls_buffer));

    webrtc::ZeroOnFreeBuffer<unsigned char> client_write_key(
        &dtls_buffer[0], key_len, key_len + salt_len);
    webrtc::ZeroOnFreeBuffer<unsigned char> server_write_key(
        &dtls_buffer[key_len], key_len, key_len + salt_len);
    client_write_key.AppendData(&dtls_buffer[key_len + key_len], salt_len);
    server_write_key.AppendData(&dtls_buffer[key_len + key_len + salt_len],
                                salt_len);

    EXPECT_TRUE(srtp_transport_.SetRtpParams(
        selected_crypto_suite, server_write_key, {}, selected_crypto_suite,
        client_write_key, {}));
  }

  webrtc::CopyOnWriteBuffer CreateRtpPacket() {
    size_t rtp_len = sizeof(kPcmuFrame);
    size_t packet_size = rtp_len + kRtpAuthTagLen;
    webrtc::Buffer rtp_packet_buffer(packet_size);
    char* rtp_packet_data = rtp_packet_buffer.data<char>();
    memcpy(rtp_packet_data, kPcmuFrame, rtp_len);

    return {rtp_packet_data, rtp_len, packet_size};
  }

  void SendRtpPacketFromSrtpToDtlsSrtp() {
    webrtc::AsyncSocketPacketOptions options;
    webrtc::CopyOnWriteBuffer packet = CreateRtpPacket();

    EXPECT_TRUE(srtp_transport_.SendRtpPacket(&packet, options,
                                              webrtc::PF_SRTP_BYPASS));
    EXPECT_THAT(webrtc::WaitUntil(
                    [&] { return dtls_srtp_transport_observer_.rtp_count(); },
                    ::testing::Eq(1),
                    {.timeout = webrtc::TimeDelta::Millis(kTimeout),
                     .clock = &fake_clock_}),
                webrtc::IsRtcOk());
    EXPECT_EQ(1, dtls_srtp_transport_observer_.rtp_count());
    ASSERT_TRUE(dtls_srtp_transport_observer_.last_recv_rtp_packet().data());
    EXPECT_EQ(
        0,
        std::memcmp(dtls_srtp_transport_observer_.last_recv_rtp_packet().data(),
                    kPcmuFrame, sizeof(kPcmuFrame)));
  }

  void SendRtpPacketFromDtlsSrtpToSrtp() {
    webrtc::AsyncSocketPacketOptions options;
    webrtc::CopyOnWriteBuffer packet = CreateRtpPacket();

    EXPECT_TRUE(dtls_srtp_transport_.SendRtpPacket(&packet, options,
                                                   webrtc::PF_SRTP_BYPASS));
    EXPECT_THAT(
        webrtc::WaitUntil([&] { return srtp_transport_observer_.rtp_count(); },
                          ::testing::Eq(1),
                          {.timeout = webrtc::TimeDelta::Millis(kTimeout),
                           .clock = &fake_clock_}),
        webrtc::IsRtcOk());
    EXPECT_EQ(1, srtp_transport_observer_.rtp_count());
    ASSERT_TRUE(srtp_transport_observer_.last_recv_rtp_packet().data());
    EXPECT_EQ(
        0, std::memcmp(srtp_transport_observer_.last_recv_rtp_packet().data(),
                       kPcmuFrame, sizeof(kPcmuFrame)));
  }

 private:
  webrtc::AutoThread main_thread_;
  webrtc::ScopedFakeClock fake_clock_;
  webrtc::test::ScopedKeyValueConfig field_trials_;

  std::unique_ptr<webrtc::FakeIceTransport> client_ice_transport_;
  std::unique_ptr<webrtc::FakeIceTransport> server_ice_transport_;

  std::unique_ptr<webrtc::DtlsTransportInternalImpl> client_dtls_transport_;
  std::unique_ptr<webrtc::DtlsTransportInternalImpl> server_dtls_transport_;

  webrtc::scoped_refptr<webrtc::RTCCertificate> client_certificate_;
  webrtc::scoped_refptr<webrtc::RTCCertificate> server_certificate_;

  webrtc::DtlsSrtpTransport dtls_srtp_transport_;
  webrtc::SrtpTransport srtp_transport_;

  webrtc::TransportObserver dtls_srtp_transport_observer_;
  webrtc::TransportObserver srtp_transport_observer_;
};

TEST_F(DtlsSrtpTransportIntegrationTest, SendRtpFromSrtpToDtlsSrtp) {
  Connect();
  SetupClientKeysManually();
  SendRtpPacketFromSrtpToDtlsSrtp();
}

TEST_F(DtlsSrtpTransportIntegrationTest, SendRtpFromDtlsSrtpToSrtp) {
  Connect();
  SetupClientKeysManually();
  SendRtpPacketFromDtlsSrtpToSrtp();
}
