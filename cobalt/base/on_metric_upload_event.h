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

#ifndef COBALT_BASE_ON_METRIC_UPLOAD_EVENT_H_
#define COBALT_BASE_ON_METRIC_UPLOAD_EVENT_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "cobalt/base/event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/h5vcc/h5vcc_metric_type.h"

namespace base {

// Event sent when a metric payload is ready for upload.
class OnMetricUploadEvent : public Event {
 public:
  OnMetricUploadEvent(const cobalt::h5vcc::H5vccMetricType& metric_type,
                      const std::string& serialized_proto)
      : metric_type_(metric_type), serialized_proto_(serialized_proto) {}

  explicit OnMetricUploadEvent(const Event* event) {
    CHECK(event != nullptr);
    const base::OnMetricUploadEvent* on_metric_upload_event =
        base::polymorphic_downcast<const base::OnMetricUploadEvent*>(event);
    metric_type_ = on_metric_upload_event->metric_type();
    serialized_proto_ = on_metric_upload_event->serialized_proto();
  }

  const cobalt::h5vcc::H5vccMetricType& metric_type() const {
    return metric_type_;
  }

  const std::string& serialized_proto() const { return serialized_proto_; }


  BASE_EVENT_SUBCLASS(OnMetricUploadEvent);

 private:
  cobalt::h5vcc::H5vccMetricType metric_type_;
  std::string serialized_proto_;
};

}  // namespace base

#endif  // COBALT_BASE_ON_METRIC_UPLOAD_EVENT_H_
