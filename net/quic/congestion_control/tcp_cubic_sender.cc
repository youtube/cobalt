// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/tcp_cubic_sender.h"

#include "net/quic/congestion_control/quic_send_scheduler.h"

namespace {
// Constants based on TCP defaults.
const size_t kHybridStartLowWindow = 16;
const int kMaxSegmentSize = net::kMaxPacketSize;
const int kDefaultReceiveWindow = 64000;
const size_t kInitialCongestionWindow = 10;
const size_t kMaxCongestionWindow = 10000;
const int kMaxBurstLength = 3;
};

namespace net {

TcpCubicSender::TcpCubicSender(const QuicClock* clock, bool reno)
    : hybrid_slow_start_(clock),
      cubic_(clock),
      reno_(reno),
      congestion_window_count_(0),
      receiver_congestion_window_in_bytes_(kDefaultReceiveWindow),
      last_received_accumulated_number_of_lost_packets_(0),
      bytes_in_flight_(0),
      update_end_sequence_number_(true),
      end_sequence_number_(0),
      congestion_window_(kInitialCongestionWindow),
      slowstart_threshold_(kMaxCongestionWindow),
      delay_min_() {
}

void TcpCubicSender::OnIncomingCongestionInfo(
    const CongestionInfo& congestion_info) {
  if (last_received_accumulated_number_of_lost_packets_ !=
      congestion_info.tcp.accumulated_number_of_lost_packets) {
    int recovered_lost_packets =
        last_received_accumulated_number_of_lost_packets_ -
        congestion_info.tcp.accumulated_number_of_lost_packets;
    last_received_accumulated_number_of_lost_packets_ =
        congestion_info.tcp.accumulated_number_of_lost_packets;
    if (recovered_lost_packets > 0) {
      OnIncomingLoss(recovered_lost_packets);
    }
  }
  receiver_congestion_window_in_bytes_ =
      congestion_info.tcp.receive_window << 4;
}

void TcpCubicSender::OnIncomingAck(
    QuicPacketSequenceNumber acked_sequence_number, size_t acked_bytes,
    QuicTime::Delta rtt) {
  bytes_in_flight_ -= acked_bytes;
  CongestionAvoidance(acked_sequence_number);
  AckAccounting(rtt);
  if (end_sequence_number_ == acked_sequence_number) {
    DLOG(INFO) << "Start update end sequence number @" << acked_sequence_number;
    update_end_sequence_number_ = true;
  }
}

void TcpCubicSender::OnIncomingLoss(int /*number_of_lost_packets*/) {
  // In a normal TCP we would need to know the lowest missing packet to detect
  // if we receive 3 missing packets. Here we get a missing packet for which we
  // enter TCP Fast Retransmit immediately.
  if (reno_) {
    congestion_window_ = congestion_window_ >> 1;
    slowstart_threshold_ = congestion_window_;
  } else {
    congestion_window_ =
        cubic_.CongestionWindowAfterPacketLoss(congestion_window_);
    slowstart_threshold_ = congestion_window_;
  }
  // Sanity, make sure that we don't end up with an empty window.
  if (congestion_window_ == 0) {
    congestion_window_ = 1;
  }
}

void TcpCubicSender::SentPacket(QuicPacketSequenceNumber sequence_number,
    size_t bytes, bool retransmit) {
  if (!retransmit) {
    bytes_in_flight_ += bytes;
  }
  if (!retransmit && update_end_sequence_number_) {
    end_sequence_number_ = sequence_number;
    if (AvailableCongestionWindow() == 0) {
      update_end_sequence_number_ = false;
      DLOG(INFO) << "Stop update end sequence number @" << sequence_number;
    }
  }
}

QuicTime::Delta TcpCubicSender::TimeUntilSend(bool retransmit) {
  if (retransmit) {
    // For TCP we can always send a retransmit immediately.
    return QuicTime::Delta();
  }
  if (AvailableCongestionWindow() == 0) {
    return QuicTime::Delta::Infinite();
  }
  return QuicTime::Delta();
}

size_t TcpCubicSender::AvailableCongestionWindow() {
  if (bytes_in_flight_ > CongestionWindow()) {
    return 0;
  }
  return CongestionWindow() - bytes_in_flight_;
}

size_t TcpCubicSender::CongestionWindow() {
  // What's the current congestion window in bytes.
  return std::min(receiver_congestion_window_in_bytes_,
                  static_cast<int>(congestion_window_ * kMaxSegmentSize));
}

int TcpCubicSender::BandwidthEstimate() {
  // TODO(pwestin): make a long term estimate, based on RTT and loss rate? or
  // instantaneous estimate?
  // Throughput ~= (1/RTT)*sqrt(3/2p)
  return kNoValidEstimate;
}

void TcpCubicSender::Reset() {
  delay_min_ = QuicTime::Delta();  // Reset to 0.
  hybrid_slow_start_.Restart();
}

bool TcpCubicSender::IsCwndLimited() const {
  const size_t congestion_window_bytes = congestion_window_ * kMaxSegmentSize;
  if (bytes_in_flight_ >= congestion_window_bytes) {
    return true;
  }
  const size_t tcp_max_burst = kMaxBurstLength * kMaxSegmentSize;
  const size_t left = congestion_window_bytes - bytes_in_flight_;
  return left <= tcp_max_burst;
}

// Called when we receive and ack. Normal TCP tracks how many packets one ack
// represents, but quic has a separate ack for each packet.
void TcpCubicSender::CongestionAvoidance(QuicPacketSequenceNumber ack) {
  if (!IsCwndLimited()) {
    // We don't update the congestion window unless we are close to using the
    // window we have available.
    DLOG(INFO) << "Congestion avoidance window not limited";
    return;
  }
  if (congestion_window_ < slowstart_threshold_) {
    // Slow start.
    if (hybrid_slow_start_.EndOfRound(ack)) {
      hybrid_slow_start_.Reset(end_sequence_number_);
    }
    // congestion_window_cnt is the number of acks since last change of snd_cwnd
    if (congestion_window_ < kMaxCongestionWindow) {
      // TCP slow start, exponentail growth, increase by one for each ACK.
      congestion_window_++;
    }
    DLOG(INFO) << "Slow start; congestion window:" << congestion_window_;
  } else {
    if (congestion_window_ < kMaxCongestionWindow) {
      if (reno_) {
        // Classic Reno congestion avoidance provided for testing.
        if (congestion_window_count_ >= congestion_window_) {
          congestion_window_++;
          congestion_window_count_ = 0;
        } else {
          congestion_window_count_++;
        }
        DLOG(INFO) << "Reno; congestion window:" << congestion_window_;
      } else {
        congestion_window_ = cubic_.CongestionWindowAfterAck(congestion_window_,
                                                             delay_min_);
        DLOG(INFO) << "Cubic; congestion window:" << congestion_window_;
      }
    }
  }
}

// TODO(pwestin): what is the timout value?
void TcpCubicSender::OnTimeOut() {
  cubic_.Reset();
  congestion_window_ = 1;
}

void TcpCubicSender::AckAccounting(QuicTime::Delta rtt) {
  if (rtt.ToMicroseconds() <= 0) {
    // RTT can't be 0 or negative.
    return;
  }
  // TODO(pwestin): Discard delay samples right after fast recovery,
  // during 1 second?.

  // First time call or link delay decreases.
  if (delay_min_.IsZero() || delay_min_ > rtt) {
    delay_min_ = rtt;
  }
  // Hybrid start triggers when cwnd is larger than some threshold.
  if (congestion_window_ <= slowstart_threshold_ &&
      congestion_window_ >= kHybridStartLowWindow) {
    if (!hybrid_slow_start_.started()) {
      // Time to start the hybrid slow start.
      hybrid_slow_start_.Reset(end_sequence_number_);
    }
    hybrid_slow_start_.Update(rtt, delay_min_);
    if (hybrid_slow_start_.Exit()) {
      slowstart_threshold_ = congestion_window_;
    }
  }
}

}  // namespace net
