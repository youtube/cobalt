// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_METRICS_COBALT_H5VCC_METRICS_UPLOADER_CALLBACK_H_
#define COBALT_BROWSER_METRICS_COBALT_H5VCC_METRICS_UPLOADER_CALLBACK_H_

#include <string>

#include "cobalt/browser/metrics/cobalt_metrics_uploader_callback.h"
#include "cobalt/h5vcc/h5vcc_metric_type.h"
#include "cobalt/h5vcc/metric_event_handler_wrapper.h"

namespace cobalt {
namespace browser {
namespace metrics {

// An implementation of CobaltMetricsUploaderCallback to support binding an
// H5vcc (JS) callback to be called with a metric payload.
class CobaltH5vccMetricsUploaderCallback
    : public CobaltMetricsUploaderCallback {
 public:
  CobaltH5vccMetricsUploaderCallback(
      h5vcc::MetricEventHandlerWrapper* event_handler)
      : event_handler_(event_handler) {}

  // Runs the h5vcc event_handler callback for the given type and payload.
  void Run(const cobalt::h5vcc::H5vccMetricType& type,
           const std::string& payload) override;

 private:
  h5vcc::MetricEventHandlerWrapper* event_handler_;
};
}  // namespace metrics
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_H5VCC_METRICS_UPLOADER_CALLBACK_H_
