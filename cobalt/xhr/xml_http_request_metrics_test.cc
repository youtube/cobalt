// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/xhr/xml_http_request_metrics.h"

#include <stdint.h>

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/base/load_timing_info.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace xhr {
namespace {
constexpr char kUmaPrefix[] = "Cobalt.Media.XHR.ResourceTiming.";


class XMLHttpRequestMetricsTest : public ::testing::Test {
 protected:
  XMLHttpRequestMetricsTest() : metrics_(&clock_) {}

  void SetUp() override { clock_.SetNowTicks(base::TimeTicks()); }

  struct LoadTimingTickInfo {
    uint32_t request_start;
    uint32_t proxy_start;
    uint32_t proxy_end;
    uint32_t dns_start;
    uint32_t dns_end;
    uint32_t connect_start;
    uint32_t ssl_start;
    uint32_t ssl_end;
    uint32_t connect_end;
    uint32_t send_start;
    uint32_t send_end;
    uint32_t response_start;
    uint32_t worker_start;
    uint32_t request_end;
  };

  net::LoadTimingInfo SetClockState(const LoadTimingTickInfo& ticks) {
    net::LoadTimingInfo info = {};
    auto start_ticks = base::TimeTicks();

    if (ticks.request_start > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.request_start));
      info.request_start = clock_.NowTicks();
    }

    if (ticks.proxy_start > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.proxy_start));
      info.proxy_resolve_start = clock_.NowTicks();
    }

    if (ticks.proxy_end > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.proxy_end));
      info.proxy_resolve_end = clock_.NowTicks();
    }

    if (ticks.dns_start > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.dns_start));
      info.connect_timing.dns_start = clock_.NowTicks();
    }

    if (ticks.dns_end > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.dns_end));
      info.connect_timing.dns_end = clock_.NowTicks();
    }

    if (ticks.connect_start > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.connect_start));
      info.connect_timing.connect_start = clock_.NowTicks();
    }

    if (ticks.ssl_start > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.ssl_start));
      info.connect_timing.ssl_start = clock_.NowTicks();
    }

    if (ticks.ssl_end > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.ssl_end));
      info.connect_timing.ssl_end = clock_.NowTicks();
    }

    if (ticks.connect_end > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.connect_end));
      info.connect_timing.connect_end = clock_.NowTicks();
    }

    if (ticks.send_start > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.send_start));
      info.send_start = clock_.NowTicks();
    }

    if (ticks.send_end > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.send_end));
      info.send_end = clock_.NowTicks();
    }

    if (ticks.response_start > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.response_start));
      info.receive_headers_end = clock_.NowTicks();
    }

    if (ticks.worker_start > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.worker_start));
      info.service_worker_start_time = clock_.NowTicks();
    }

    if (ticks.request_end > 0) {
      clock_.SetNowTicks(start_ticks);
      clock_.Advance(base::TimeDelta::FromMilliseconds(ticks.request_end));
    }

    return info;
  }

  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock clock_;

  XMLHttpRequestMetrics metrics_;
};

TEST_F(XMLHttpRequestMetricsTest, HandlesEmptyTimingInfo) {
  metrics_.ReportResourceTimingMetrics(net::LoadTimingInfo{});

  auto counts = histogram_tester_.GetTotalCountsForPrefix(kUmaPrefix);

  EXPECT_EQ(0, counts.size());
}

TEST_F(XMLHttpRequestMetricsTest, ReportsTcpHandshakeTime) {
  XMLHttpRequestMetricsTest::LoadTimingTickInfo data = {};
  data.connect_start = 7;
  data.connect_end = 13;
  auto info = SetClockState(data);

  metrics_.ReportResourceTimingMetrics(info);

  auto counts = histogram_tester_.GetTotalCountsForPrefix(kUmaPrefix);
  EXPECT_EQ(1, counts.size());

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "TcpHandshakeTime", 6, 1);
}

TEST_F(XMLHttpRequestMetricsTest, ReportsDnsLookupTime) {
  XMLHttpRequestMetricsTest::LoadTimingTickInfo data = {};
  data.dns_start = 7;
  data.dns_end = 13;
  auto info = SetClockState(data);

  metrics_.ReportResourceTimingMetrics(info);

  auto counts = histogram_tester_.GetTotalCountsForPrefix(kUmaPrefix);
  EXPECT_EQ(1, counts.size());

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "DnsLookupTime", 6, 1);
}

TEST_F(XMLHttpRequestMetricsTest, ReportsRequestTime) {
  XMLHttpRequestMetricsTest::LoadTimingTickInfo data = {};
  data.send_start = 7;
  data.response_start = 13;
  auto info = SetClockState(data);

  metrics_.ReportResourceTimingMetrics(info);

  auto counts = histogram_tester_.GetTotalCountsForPrefix(kUmaPrefix);
  EXPECT_EQ(1, counts.size());

  histogram_tester_.ExpectUniqueSample(std::string(kUmaPrefix) + "RequestTime",
                                       6, 1);
}

TEST_F(XMLHttpRequestMetricsTest, ReportsTlsNegotiationTime) {
  XMLHttpRequestMetricsTest::LoadTimingTickInfo data = {};
  data.ssl_start = 7;
  data.send_start = 13;
  auto info = SetClockState(data);

  metrics_.ReportResourceTimingMetrics(info);

  auto counts = histogram_tester_.GetTotalCountsForPrefix(kUmaPrefix);
  EXPECT_EQ(1, counts.size());

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "TlsNegotiationTime", 6, 1);
}

TEST_F(XMLHttpRequestMetricsTest, ReportsTimeToFetch) {
  XMLHttpRequestMetricsTest::LoadTimingTickInfo data = {};
  data.request_start = 7;
  data.request_end = 13;
  auto info = SetClockState(data);

  metrics_.ReportResourceTimingMetrics(info);

  auto counts = histogram_tester_.GetTotalCountsForPrefix(kUmaPrefix);
  EXPECT_EQ(1, counts.size());

  histogram_tester_.ExpectUniqueSample(std::string(kUmaPrefix) + "TimeToFetch",
                                       6, 1);
}

TEST_F(XMLHttpRequestMetricsTest, ReportsServiceWorkerStartTime) {
  XMLHttpRequestMetricsTest::LoadTimingTickInfo data = {};
  data.worker_start = 7;
  data.request_start = 13;
  auto info = SetClockState(data);

  metrics_.ReportResourceTimingMetrics(info);

  // This also includes a TimeToFetch event.
  auto counts = histogram_tester_.GetTotalCountsForPrefix(kUmaPrefix);
  EXPECT_EQ(2, counts.size());

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "ServiceWorkerProcessingTime", 6, 1);
}

TEST_F(XMLHttpRequestMetricsTest, CanReportAllMetricsSimultaneously) {
  XMLHttpRequestMetricsTest::LoadTimingTickInfo data = {};
  data.worker_start = 1;
  data.request_start = 2;
  data.proxy_start = 4;
  data.proxy_end = 8;
  data.dns_start = 16;
  data.dns_end = 32;
  data.connect_start = 64;
  data.ssl_start = 128;
  data.ssl_end = 256;
  data.connect_end = 512;
  data.send_start = 1024;
  data.send_end = 2048;
  data.response_start = 4096;
  data.request_end = 8192;
  auto info = SetClockState(data);

  metrics_.ReportResourceTimingMetrics(info);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "TcpHandshakeTime", 448, 1);
  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "DnsLookupTime", 16, 1);
  histogram_tester_.ExpectUniqueSample(std::string(kUmaPrefix) + "RequestTime",
                                       3072, 1);
  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "TlsNegotiationTime", 896, 1);
  histogram_tester_.ExpectUniqueSample(std::string(kUmaPrefix) + "TimeToFetch",
                                       8190, 1);
  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "ServiceWorkerProcessingTime", 1, 1);
}

}  // namespace
}  // namespace xhr
}  // namespace cobalt
