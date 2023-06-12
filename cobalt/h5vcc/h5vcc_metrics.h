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

#ifndef COBALT_H5VCC_H5VCC_METRICS_H_
#define COBALT_H5VCC_H5VCC_METRICS_H_

#include <string>

#include "cobalt/h5vcc/h5vcc_metric_type.h"
#include "cobalt/h5vcc/metric_event_handler_wrapper.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"


namespace cobalt {
namespace h5vcc {

// Implementation of the h5vcc_metrics.idl interface. Supports interactions
// with the web client related to metrics logging.
class H5vccMetrics : public script::Wrappable {
 public:
  // Required to put a typedef under the H5vccMetrics class as expected by the
  // IDL definition. The "MetricEventHandler" typedef is explicitly separate
  // so it can be used by other classes outside H5vcc without bringing H5vcc in
  // its entirety.
  typedef MetricEventHandler H5vccMetricEventHandler;

  H5vccMetrics() {}

  H5vccMetrics(const H5vccMetrics&) = delete;
  H5vccMetrics& operator=(const H5vccMetrics&) = delete;

  // Binds an event handler that will be invoked every time Cobalt wants to
  // upload a metrics payload.
  void OnMetricEvent(
      const MetricEventHandlerWrapper::ScriptValue& eventHandler);

  // API to explicitly enable or disable Cobalt metrics logging.  Pass true
  // to turn on and false to disable. If disabled, the metric event handler
  // should never get called after that point.
  void ToggleMetricsEnabled(bool isEnabled);

  // Returns current enabled state of metrics logging/reporting.
  bool GetMetricsEnabled();

  DEFINE_WRAPPABLE_TYPE(H5vccMetrics);

 private:
  scoped_refptr<MetricEventHandlerWrapper> event_handler_;
  bool is_enabled_ = false;
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_METRICS_H_
