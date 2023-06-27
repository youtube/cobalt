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

#include "cobalt/browser/metrics/cobalt_h5vcc_metrics_uploader_callback.h"

namespace cobalt {
namespace browser {
namespace metrics {

void CobaltH5vccMetricsUploaderCallback::Run(
    const cobalt::h5vcc::H5vccMetricType& type, const std::string& payload) {
  event_handler_->callback.value().Run(type, payload);
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
