// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket_stream/socket_stream_metrics.h"

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Histogram;
using base::StatisticsRecorder;

namespace net {

TEST(SocketStreamMetricsTest, ProtocolType) {
  // First we'll preserve the original values. We need to do this
  // as histograms can get affected by other tests. In particular,
  // SocketStreamTest and WebSocketTest can affect the histograms.
  Histogram::SampleSet original;
  Histogram* histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ProtocolType");
  if (histogram) {
    histogram->SnapshotSample(&original);
  }

  SocketStreamMetrics unknown(GURL("unknown://www.example.com/"));
  SocketStreamMetrics ws1(GURL("ws://www.example.com/"));
  SocketStreamMetrics ws2(GURL("ws://www.example.com/"));
  SocketStreamMetrics wss1(GURL("wss://www.example.com/"));
  SocketStreamMetrics wss2(GURL("wss://www.example.com/"));
  SocketStreamMetrics wss3(GURL("wss://www.example.com/"));

  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ProtocolType");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());

  Histogram::SampleSet sample;
  histogram->SnapshotSample(&sample);
  original.Resize(*histogram);  // Ensure |original| size is same as |sample|.
  sample.Subtract(original);  // Cancel the original values.
  EXPECT_EQ(1, sample.counts(SocketStreamMetrics::PROTOCOL_UNKNOWN));
  EXPECT_EQ(2, sample.counts(SocketStreamMetrics::PROTOCOL_WEBSOCKET));
  EXPECT_EQ(3, sample.counts(SocketStreamMetrics::PROTOCOL_WEBSOCKET_SECURE));
}

TEST(SocketStreamMetricsTest, ConnectionType) {
  // First we'll preserve the original values.
  Histogram::SampleSet original;
  Histogram* histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ConnectionType");
  if (histogram) {
    histogram->SnapshotSample(&original);
  }

  SocketStreamMetrics metrics(GURL("ws://www.example.com/"));
  for (int i = 0; i < 1; ++i)
    metrics.OnStartConnection();
  for (int i = 0; i < 2; ++i)
    metrics.OnCountConnectionType(SocketStreamMetrics::TUNNEL_CONNECTION);
  for (int i = 0; i < 3; ++i)
    metrics.OnCountConnectionType(SocketStreamMetrics::SOCKS_CONNECTION);
  for (int i = 0; i < 4; ++i)
    metrics.OnCountConnectionType(SocketStreamMetrics::SSL_CONNECTION);


  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ConnectionType");
  ASSERT_TRUE(histogram != NULL);
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

TEST(SocketStreamMetricsTest, WireProtocolType) {
  // First we'll preserve the original values.
  Histogram::SampleSet original;
  Histogram* histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.WireProtocolType");
  if (histogram) {
    histogram->SnapshotSample(&original);
  }

  SocketStreamMetrics metrics(GURL("ws://www.example.com/"));
  for (int i = 0; i < 3; ++i)
    metrics.OnCountWireProtocolType(
        SocketStreamMetrics::WIRE_PROTOCOL_WEBSOCKET);
  for (int i = 0; i < 7; ++i)
    metrics.OnCountWireProtocolType(SocketStreamMetrics::WIRE_PROTOCOL_SPDY);

  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.WireProtocolType");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());

  Histogram::SampleSet sample;
  histogram->SnapshotSample(&sample);
  original.Resize(*histogram);
  sample.Subtract(original);
  EXPECT_EQ(3, sample.counts(SocketStreamMetrics::WIRE_PROTOCOL_WEBSOCKET));
  EXPECT_EQ(7, sample.counts(SocketStreamMetrics::WIRE_PROTOCOL_SPDY));
}

TEST(SocketStreamMetricsTest, OtherNumbers) {
  // First we'll preserve the original values.
  int64 original_received_bytes = 0;
  int64 original_received_counts = 0;
  int64 original_sent_bytes = 0;
  int64 original_sent_counts = 0;

  Histogram::SampleSet original;

  Histogram* histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ReceivedBytes");
  if (histogram) {
    histogram->SnapshotSample(&original);
    original_received_bytes = original.sum();
  }
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ReceivedCounts");
  if (histogram) {
    histogram->SnapshotSample(&original);
    original_received_counts = original.sum();
  }
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.SentBytes");
  if (histogram) {
    histogram->SnapshotSample(&original);
    original_sent_bytes = original.sum();
  }
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.SentCounts");
  if (histogram) {
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
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ConnectionLatency");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  // We don't check the contents of the histogram as it's time sensitive.

  // ConnectionEstablish.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ConnectionEstablish");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  // We don't check the contents of the histogram as it's time sensitive.

  // Duration.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.Duration");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  // We don't check the contents of the histogram as it's time sensitive.

  // ReceivedBytes.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ReceivedBytes");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(11, sample.sum() - original_received_bytes);  // 11 bytes read.

  // ReceivedCounts.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.ReceivedCounts");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(2, sample.sum() - original_received_counts);  // 2 read requests.

  // SentBytes.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.SentBytes");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(222, sample.sum() - original_sent_bytes);  // 222 bytes sent.

  // SentCounts.
  histogram =
      StatisticsRecorder::FindHistogram("Net.SocketStream.SentCounts");
  ASSERT_TRUE(histogram != NULL);
  EXPECT_EQ(Histogram::kUmaTargetedHistogramFlag, histogram->flags());
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(3, sample.sum() - original_sent_counts);  // 3 write requests.
}

}  // namespace net
