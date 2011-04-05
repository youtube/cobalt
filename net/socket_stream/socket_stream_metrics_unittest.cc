// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "googleurl/src/gurl.h"
#include "net/socket_stream/socket_stream_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Histogram;
using base::StatisticsRecorder;

namespace net {

TEST(SocketStreamMetricsTest, Initialize) {
  if (!StatisticsRecorder::IsActive()) {
    // Create the recorder if not yet started, as SocketStreamMetrics
    // relys on the StatisticsRecorder to be present. This is useful when
    // tests are run with --gtest_filter='SocketStreamMetricsTest*'.
    static StatisticsRecorder *recorder = NULL;
    recorder = new StatisticsRecorder;
  }
}

TEST(SocketStreamMetricsTest, ProtocolType) {
  Histogram* histogram;

  // First we'll preserve the original values. We need to do this
  // as histograms can get affected by other tests. In particular,
  // SocketStreamTest and WebSocketTest can affect the histograms.
  Histogram::SampleSet original;
  if (StatisticsRecorder::FindHistogram(
          "Net.SocketStream.ProtocolType", &histogram)) {
    histogram->SnapshotSample(&original);
  }

  SocketStreamMetrics unknown(GURL("unknown://www.example.com/"));
  SocketStreamMetrics ws1(GURL("ws://www.example.com/"));
  SocketStreamMetrics ws2(GURL("ws://www.example.com/"));
  SocketStreamMetrics wss1(GURL("wss://www.example.com/"));
  SocketStreamMetrics wss2(GURL("wss://www.example.com/"));
  SocketStreamMetrics wss3(GURL("wss://www.example.com/"));

  ASSERT_TRUE(StatisticsRecorder::FindHistogram(
      "Net.SocketStream.ProtocolType", &histogram));
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());

  Histogram::SampleSet sample;
  histogram->SnapshotSample(&sample);
  original.Resize(*histogram);  // Ensure |original| size is same as |sample|.
  sample.Subtract(original); // Cancel the original values.
  EXPECT_EQ(1, sample.counts(SocketStreamMetrics::PROTOCOL_UNKNOWN));
  EXPECT_EQ(2, sample.counts(SocketStreamMetrics::PROTOCOL_WEBSOCKET));
  EXPECT_EQ(3, sample.counts(SocketStreamMetrics::PROTOCOL_WEBSOCKET_SECURE));
}

TEST(SocketStreamMetricsTest, ConnectionType) {
  Histogram* histogram;

  // First we'll preserve the original values.
  Histogram::SampleSet original;
  if (StatisticsRecorder::FindHistogram(
          "Net.SocketStream.ConnectionType", &histogram)) {
    histogram->SnapshotSample(&original);
  }

  SocketStreamMetrics metrics(GURL("ws://www.example.com/"));
  for (int i = 0; i < 1; ++i)
    metrics.OnStartConnection();
  for (int i = 0; i < 2; ++i)
    metrics.OnTunnelProxy();
  for (int i = 0; i < 3; ++i)
    metrics.OnSOCKSProxy();
  for (int i = 0; i < 4; ++i)
    metrics.OnSSLConnection();

  ASSERT_TRUE(StatisticsRecorder::FindHistogram(
      "Net.SocketStream.ConnectionType", &histogram));
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());

  Histogram::SampleSet sample;
  histogram->SnapshotSample(&sample);
  original.Resize(*histogram);
  sample.Subtract(original);
  EXPECT_EQ(1, sample.counts(SocketStreamMetrics::ALL_CONNECTIONS));
  EXPECT_EQ(2, sample.counts(SocketStreamMetrics::TUNNEL_CONNECTION));
  EXPECT_EQ(3, sample.counts(SocketStreamMetrics::SOCKS_CONNECTION));
  EXPECT_EQ(4, sample.counts(SocketStreamMetrics::SSL_CONNECTION));
}

TEST(SocketStreamMetricsTest, OtherNumbers) {
  Histogram* histogram;

  // First we'll preserve the original values.
  int64 original_received_bytes = 0;
  int64 original_received_counts = 0;
  int64 original_sent_bytes = 0;
  int64 original_sent_counts = 0;

  Histogram::SampleSet original;
  if (StatisticsRecorder::FindHistogram(
          "Net.SocketStream.ReceivedBytes", &histogram)) {
    histogram->SnapshotSample(&original);
    original_received_bytes = original.sum();
  }
  if (StatisticsRecorder::FindHistogram(
          "Net.SocketStream.ReceivedCounts", &histogram)) {
    histogram->SnapshotSample(&original);
    original_received_counts = original.sum();
  }
  if (StatisticsRecorder::FindHistogram(
          "Net.SocketStream.SentBytes", &histogram)) {
    histogram->SnapshotSample(&original);
    original_sent_bytes = original.sum();
  }
  if (StatisticsRecorder::FindHistogram(
          "Net.SocketStream.SentCounts", &histogram)) {
    histogram->SnapshotSample(&original);
    original_sent_counts = original.sum();
  }

  SocketStreamMetrics metrics(GURL("ws://www.example.com/"));
  metrics.OnWaitConnection();
  metrics.OnStartConnection();
  metrics.OnConnected();
  metrics.OnRead(1);
  metrics.OnRead(10);
  metrics.OnWrite(2);
  metrics.OnWrite(20);
  metrics.OnWrite(200);
  metrics.OnClose();

  Histogram::SampleSet sample;

  // ConnectionLatency.
  ASSERT_TRUE(StatisticsRecorder::FindHistogram(
      "Net.SocketStream.ConnectionLatency", &histogram));
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  // We don't check the contents of the histogram as it's time sensitive.

  // ConnectionEstablish.
  ASSERT_TRUE(StatisticsRecorder::FindHistogram(
      "Net.SocketStream.ConnectionEstablish", &histogram));
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  // We don't check the contents of the histogram as it's time sensitive.

  // Duration.
  ASSERT_TRUE(StatisticsRecorder::FindHistogram(
      "Net.SocketStream.Duration", &histogram));
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  // We don't check the contents of the histogram as it's time sensitive.

  // ReceivedBytes.
  ASSERT_TRUE(StatisticsRecorder::FindHistogram(
      "Net.SocketStream.ReceivedBytes", &histogram));
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(11, sample.sum() - original_received_bytes);  // 11 bytes read.

  // ReceivedCounts.
  ASSERT_TRUE(StatisticsRecorder::FindHistogram(
      "Net.SocketStream.ReceivedCounts", &histogram));
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(2, sample.sum() - original_received_counts);  // 2 read requests.

  // SentBytes.
  ASSERT_TRUE(StatisticsRecorder::FindHistogram(
      "Net.SocketStream.SentBytes", &histogram));
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(222, sample.sum() - original_sent_bytes);  // 222 bytes sent.

  // SentCounts.
  ASSERT_TRUE(StatisticsRecorder::FindHistogram(
      "Net.SocketStream.SentCounts", &histogram));
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(3, sample.sum() - original_sent_counts);  // 3 write requests.
}

}  // namespace net
