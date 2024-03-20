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

#include <memory>
#include <string>

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/base/event.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/h5vcc/h5vcc_metric_type.h"
#include "cobalt/h5vcc/metric_event_handler_wrapper.h"
#include "cobalt/persistent_storage/persistent_settings.h"
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

  explicit H5vccMetrics(
      persistent_storage::PersistentSettings* persistent_settings,
      base::EventDispatcher* event_dispatcher);

  ~H5vccMetrics();

  H5vccMetrics(const H5vccMetrics&) = delete;
  H5vccMetrics& operator=(const H5vccMetrics&) = delete;

  // Binds a JS event handler that will be invoked every time Cobalt wants to
  // upload a metrics payload.
  void OnMetricEvent(
      const MetricEventHandlerWrapper::ScriptValue& event_handler);

  // Enable Cobalt metrics logging.
  void Enable();

  // Disable Cobalt metrics logging.
  void Disable();

  // Returns current enabled state of metrics logging/reporting.
  bool IsEnabled();

  // Sets the interval in which metrics will be aggregated and sent to the
  // metric event handler.
  void SetMetricEventInterval(uint32 interval_seconds);

  DEFINE_WRAPPABLE_TYPE(H5vccMetrics);

 private:
  // Internal convenience method for toggling enabled/disabled state.
  void ToggleMetricsEnabled(bool is_enabled);

  void RunEventHandler(const cobalt::h5vcc::H5vccMetricType& metric_type,
                       const std::string& serialized_proto);

  void RunEventHandlerInternal(
      const cobalt::h5vcc::H5vccMetricType& metric_type,
      const std::string& serialized_proto);

  // Handler method triggered when EventDispatcher sends OnMetricUploadEvents.
  void OnMetricUploadEvent(const base::Event* event);

  scoped_refptr<h5vcc::MetricEventHandlerWrapper> uploader_callback_;

  scoped_refptr<base::SequencedTaskRunner> const task_runner_;

  persistent_storage::PersistentSettings* persistent_settings_;

  // Non-owned reference used to receive application event callbacks, namely
  // metric log upload events.
  base::EventDispatcher* event_dispatcher_;

  base::EventCallback on_metric_upload_event_callback_;
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_METRICS_H_
