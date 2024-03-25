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

#include "base/metrics/histogram_macros.h"
#include "net/base/load_timing_info.h"

namespace cobalt {
namespace xhr {

void XMLHttpRequestMetrics::ReportResourceTimingMetrics(
    const net::LoadTimingInfo& timing_info) {
  if (!(timing_info.connect_timing.connect_start.is_null() ||
        timing_info.connect_timing.connect_end.is_null())) {
    base::TimeDelta tcp_handshake_time =
        timing_info.connect_timing.connect_end -
        timing_info.connect_timing.connect_start;
    UMA_HISTOGRAM_TIMES("Cobalt.Media.XHR.ResourceTiming.TcpHandshakeTime",
                        tcp_handshake_time);
  }

  if (!(timing_info.connect_timing.dns_start.is_null() ||
        timing_info.connect_timing.dns_end.is_null())) {
    base::TimeDelta dns_lookup_time = timing_info.connect_timing.dns_end -
                                      timing_info.connect_timing.dns_start;
    UMA_HISTOGRAM_TIMES("Cobalt.Media.XHR.ResourceTiming.DnsLookupTime",
                        dns_lookup_time);
  }

  if (!(timing_info.send_start.is_null() ||
        timing_info.receive_headers_end.is_null())) {
    base::TimeDelta request_time =
        timing_info.receive_headers_end - timing_info.send_start;
    UMA_HISTOGRAM_TIMES("Cobalt.Media.XHR.ResourceTiming.RequestTime",
                        request_time);
  }

  if (!(timing_info.connect_timing.ssl_start.is_null() ||
        timing_info.send_start.is_null())) {
    base::TimeDelta tls_negotiation_time =
        timing_info.send_start - timing_info.connect_timing.ssl_start;
    UMA_HISTOGRAM_TIMES("Cobalt.Media.XHR.ResourceTiming.TlsNegotiationTime",
                        tls_negotiation_time);
  }

  if (!(timing_info.request_start.is_null())) {
    base::TimeDelta time_to_fetch =
        clock_->NowTicks() - timing_info.request_start;
    UMA_HISTOGRAM_TIMES("Cobalt.Media.XHR.ResourceTiming.TimeToFetch",
                        time_to_fetch);
  }

  if (!(timing_info.service_worker_start_time.is_null() ||
        timing_info.request_start.is_null())) {
    base::TimeDelta service_worker_processing_time =
        timing_info.request_start - timing_info.service_worker_start_time;
    UMA_HISTOGRAM_TIMES(
        "Cobalt.Media.XHR.ResourceTiming.ServiceWorkerProcessingTime",
        service_worker_processing_time);
  }
}

}  // namespace xhr
}  // namespace cobalt
