// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_metrics/metrics_event.h"

#include "third_party/blink/renderer/modules/event_interface_modules_names.h"

namespace blink {

MetricsEvent::MetricsEvent(const AtomicString& type, const String& tbd)
    : Event(type, Bubbles::kNo, Cancelable::kNo), tbd_(tbd) {}

MetricsEvent::~MetricsEvent() = default;

const AtomicString& MetricsEvent::InterfaceName() const {
  return event_interface_names::kMetricsEvent;
}

void MetricsEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
