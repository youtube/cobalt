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

#ifndef COBALT_H5VCC_METRIC_EVENT_HANDLER_WRAPPER_H_
#define COBALT_H5VCC_METRIC_EVENT_HANDLER_WRAPPER_H_

#include <string>

#include "cobalt/h5vcc/h5vcc_metric_type.h"
#include "cobalt/h5vcc/script_callback_wrapper.h"
#include "cobalt/script/callback_function.h"

namespace cobalt {
namespace h5vcc {

// H5vccMetric callback function typedef.
typedef script::CallbackFunction<void(const H5vccMetricType&,
                                      const std::string&)>
    MetricEventHandler;

// Typedef for the ScriptCallbackWrapper callback.
typedef ScriptCallbackWrapper<MetricEventHandler> MetricEventHandlerWrapper;

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_METRIC_EVENT_HANDLER_WRAPPER_H_
