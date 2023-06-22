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

#ifndef COBALT_BROWSER_METRICS_COBALT_METRICS_UPLOADER_CALLBACK_H_
#define COBALT_BROWSER_METRICS_COBALT_METRICS_UPLOADER_CALLBACK_H_

#include <string>

#include "base/callback.h"
#include "cobalt/h5vcc/h5vcc_metric_type.h"

namespace cobalt {
namespace browser {
namespace metrics {

typedef base::RepeatingCallback<void(
    const cobalt::h5vcc::H5vccMetricType& metric_type,
    const std::string& serialized_proto)>
    CobaltMetricsUploaderCallback;

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_METRICS_UPLOADER_CALLBACK_H_
