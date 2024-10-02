// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/local_web_memory_measurer.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_controller.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using performance_manager::mojom::blink::WebMemoryAttribution;
using performance_manager::mojom::blink::WebMemoryAttributionPtr;
using performance_manager::mojom::blink::WebMemoryBreakdownEntry;
using performance_manager::mojom::blink::WebMemoryBreakdownEntryPtr;
using performance_manager::mojom::blink::WebMemoryMeasurement;
using performance_manager::mojom::blink::WebMemoryMeasurementPtr;
using performance_manager::mojom::blink::WebMemoryUsage;

namespace {
v8::MeasureMemoryExecution ToV8MeasureMemoryExecution(
    WebMemoryMeasurement::Mode mode) {
  switch (mode) {
    case WebMemoryMeasurement::Mode::kDefault:
      return v8::MeasureMemoryExecution::kDefault;
    case WebMemoryMeasurement::Mode::kEager:
      return v8::MeasureMemoryExecution::kEager;
  }
  NOTREACHED();
}
}  // anonymous namespace

// static
void LocalWebMemoryMeasurer::StartMeasurement(
    v8::Isolate* isolate,
    WebMemoryMeasurement::Mode mode,
    MeasureMemoryController* controller,
    WebMemoryAttribution::Scope attribution_scope,
    WTF::String attribution_url) {
  // We cannot use std::make_unique here because the constructor is private.
  auto delegate =
      std::unique_ptr<LocalWebMemoryMeasurer>(new LocalWebMemoryMeasurer(
          controller, attribution_scope, attribution_url));
  isolate->MeasureMemory(std::move(delegate), ToV8MeasureMemoryExecution(mode));
}

LocalWebMemoryMeasurer::LocalWebMemoryMeasurer(
    MeasureMemoryController* controller,
    WebMemoryAttribution::Scope attribution_scope,
    WTF::String attribution_url)
    : controller_(controller),
      attribution_scope_(attribution_scope),
      attribution_url_(attribution_url) {}

LocalWebMemoryMeasurer::~LocalWebMemoryMeasurer() = default;

bool LocalWebMemoryMeasurer::ShouldMeasure(v8::Local<v8::Context> context) {
  return true;
}

void LocalWebMemoryMeasurer::MeasurementComplete(
    const std::vector<std::pair<v8::Local<v8::Context>, size_t>>& context_sizes,
    size_t unattributed_size) {
  DCHECK_LE(context_sizes.size(), 1u);
  // The isolate has only one context, so all memory of the isolate can be
  // attributed to that context.
  size_t bytes = unattributed_size;
  for (const auto& context_size : context_sizes) {
    bytes += context_size.second;
  }
  WebMemoryAttributionPtr attribution = WebMemoryAttribution::New();
  attribution->scope = attribution_scope_;
  attribution->url = attribution_url_;
  WebMemoryBreakdownEntryPtr breakdown = WebMemoryBreakdownEntry::New();
  breakdown->attribution.emplace_back(std::move(attribution));
  breakdown->memory = WebMemoryUsage::New(bytes);
  WebMemoryMeasurementPtr measurement = WebMemoryMeasurement::New();
  measurement->breakdown.push_back(std::move(breakdown));
  measurement->blink_memory = WebMemoryUsage::New(0);
  measurement->shared_memory = WebMemoryUsage::New(0);
  controller_->MeasurementComplete(std::move(measurement));
}

}  // namespace blink
