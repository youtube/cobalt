// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_packetizer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtp/packet_storage.h"
#include "media/cast/net/rtp/rtp_parser.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

namespace {
static const int kPayload = 127;
static const uint32_t kTimestampMs = 10;
static const uint16_t kSeqNum = 33;
static const int kMaxPacketLength = 1500;
static const int kSsrc = 0x12345;
static const unsigned int kFrameSize = 5000;
}  // namespace

class TestRtpPacketTransport : public PacketTransport {
 public:
  explicit TestRtpPacketTransport(RtpPacketizerConfig config)
      : config_(config),
        sequence_number_(kSeqNum),
        packets_sent_(0),
        expected_number_of_packets_(0),
        expected_packet_id_(0),
        expected_frame_id_(FrameId::first() + 1) {}

  void VerifyRtpHeader(const RtpCastHeader& rtp_header) {
    VerifyCommonRtpHeader(rtp_header);
    VerifyCastRtpHeader(rtp_header);
  }

  void VerifyCommonRtpHeader(const RtpCastHeader& rtp_header) {
    EXPECT_EQ(kPayload, rtp_header.payload_type);
    EXPECT_EQ(sequence_number_, rtp_header.sequence_number);
    EXPECT_EQ(expected_rtp_timestamp_, rtp_header.rtp_timestamp);
    EXPECT_EQ(config_.ssrc, rtp_header.sender_ssrc);
    EXPECT_EQ(0, rtp_header.num_csrcs);
  }

  void VerifyCastRtpHeader(const RtpCastHeader& rtp_header) {
    EXPECT_FALSE(rtp_header.is_key_frame);
    EXPECT_EQ(expected_frame_id_, rtp_header.frame_id);
    EXPECT_EQ(expected_packet_id_, rtp_header.packet_id);
    EXPECT_EQ(expected_number_of_packets_ - 1, rtp_header.max_packet_id);
    EXPECT_TRUE(rtp_header.is_reference);
    EXPECT_EQ(expected_frame_id_ - 1, rtp_header.reference_frame_id);
    if (rtp_header.packet_id != 0) {
      EXPECT_EQ(rtp_header.num_extensions, 0)
          << "Extensions only allowed on first packet of a frame";
    }
  }

  bool SendPacket(PacketRef packet, base::OnceClosure cb) final {
    ++packets_sent_;
    RtpParser parser(kSsrc, kPayload);
    RtpCastHeader rtp_header;
    const uint8_t* payload_data;
    size_t payload_size;
    parser.ParsePacket(&packet->data[0], packet->data.size(), &rtp_header,
                       &payload_data, &payload_size);
    VerifyRtpHeader(rtp_header);
    ++sequence_number_;
    ++expected_packet_id_;
    return true;
  }

  int64_t GetBytesSent() final { return 0; }

  void StartReceiving(PacketReceiverCallbackWithStatus packet_receiver) final {}

  void StopReceiving() final {}

  size_t number_of_packets_received() const { return packets_sent_; }

  void set_expected_number_of_packets(size_t expected_number_of_packets) {
    expected_number_of_packets_ = expected_number_of_packets;
  }

  void set_rtp_timestamp(RtpTimeTicks rtp_timestamp) {
    expected_rtp_timestamp_ = rtp_timestamp;
  }

  RtpPacketizerConfig config_;
  uint32_t sequence_number_;
  size_t packets_sent_;
  size_t number_of_packets_;
  size_t expected_number_of_packets_;
  // Assuming packets arrive in sequence.
  int expected_packet_id_;
  FrameId expected_frame_id_;
  RtpTimeTicks expected_rtp_timestamp_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRtpPacketTransport);
};

class RtpPacketizerTest : public ::testing::Test {
 protected:
  RtpPacketizerTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)) {
    config_.sequence_number = kSeqNum;
    config_.ssrc = kSsrc;
    config_.payload_type = kPayload;
    config_.max_payload_length = kMaxPacketLength;
    transport_ = std::make_unique<TestRtpPacketTransport>(config_);
    pacer_ = std::make_unique<PacedSender>(kTargetBurstSize, kMaxBurstSize,
                                           &testing_clock_, nullptr,
                                           transport_.get(), task_runner_);
    pacer_->RegisterSsrc(config_.ssrc, false);
    rtp_packetizer_ = std::make_unique<RtpPacketizer>(
        pacer_.get(), &packet_storage_, config_);
    video_frame_.dependency = EncodedFrame::DEPENDENT;
    video_frame_.frame_id = FrameId::first() + 1;
    video_frame_.referenced_frame_id = video_frame_.frame_id - 1;
    video_frame_.data.assign(kFrameSize, 123);
    video_frame_.rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(0x0055aa11));
  }

  void RunTasks(int during_ms) {
    for (int i = 0; i < during_ms; ++i) {
      // Call process the timers every 1 ms.
      testing_clock_.Advance(base::Milliseconds(1));
      task_runner_->RunTasks();
    }
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  EncodedFrame video_frame_;
  PacketStorage packet_storage_;
  RtpPacketizerConfig config_;
  std::unique_ptr<TestRtpPacketTransport> transport_;
  std::unique_ptr<PacedSender> pacer_;
  std::unique_ptr<RtpPacketizer> rtp_packetizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RtpPacketizerTest);
};

TEST_F(RtpPacketizerTest, SendStandardPackets) {
  size_t expected_num_of_packets = kFrameSize / kMaxPacketLength + 1;
  transport_->set_expected_number_of_packets(expected_num_of_packets);
  transport_->set_rtp_timestamp(video_frame_.rtp_timestamp);

  testing_clock_.Advance(base::Milliseconds(kTimestampMs));
  video_frame_.reference_time = testing_clock_.NowTicks();
  rtp_packetizer_->SendFrameAsPackets(video_frame_);
  RunTasks(33 + 1);
  EXPECT_EQ(expected_num_of_packets, transport_->number_of_packets_received());
}

TEST_F(RtpPacketizerTest, SendPacketsWithAdaptivePlayoutExtension) {
  size_t expected_num_of_packets = kFrameSize / kMaxPacketLength + 1;
  transport_->set_expected_number_of_packets(expected_num_of_packets);
  transport_->set_rtp_timestamp(video_frame_.rtp_timestamp);

  testing_clock_.Advance(base::Milliseconds(kTimestampMs));
  video_frame_.reference_time = testing_clock_.NowTicks();
  video_frame_.new_playout_delay_ms = 500;
  rtp_packetizer_->SendFrameAsPackets(video_frame_);
  RunTasks(33 + 1);
  EXPECT_EQ(expected_num_of_packets, transport_->number_of_packets_received());
}

TEST_F(RtpPacketizerTest, Stats) {
  EXPECT_FALSE(rtp_packetizer_->send_packet_count());
  EXPECT_FALSE(rtp_packetizer_->send_octet_count());
  // Insert packets at varying lengths.
  size_t expected_num_of_packets = kFrameSize / kMaxPacketLength + 1;
  transport_->set_expected_number_of_packets(expected_num_of_packets);
  transport_->set_rtp_timestamp(video_frame_.rtp_timestamp);

  testing_clock_.Advance(base::Milliseconds(kTimestampMs));
  video_frame_.reference_time = testing_clock_.NowTicks();
  rtp_packetizer_->SendFrameAsPackets(video_frame_);
  RunTasks(33 + 1);
  EXPECT_EQ(expected_num_of_packets, rtp_packetizer_->send_packet_count());
  EXPECT_EQ(kFrameSize, rtp_packetizer_->send_octet_count());
  EXPECT_EQ(expected_num_of_packets, transport_->number_of_packets_received());
}

}  // namespace cast
}  // namespace media
