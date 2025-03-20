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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_METRICS_METRICS_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_METRICS_METRICS_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT MetricsEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MetricsEvent* Create(const AtomicString& type, const String& tbd) {
    return MakeGarbageCollected<MetricsEvent>(type, tbd);
  }

  MetricsEvent(const AtomicString& type, const String& tbd);
  ~MetricsEvent() override;

  const String& tbd() const { return tbd_; }

  const AtomicString& InterfaceName() const override;
  void Trace(Visitor*) const override;

 private:
  const String tbd_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_METRICS_METRICS_EVENT_H_
