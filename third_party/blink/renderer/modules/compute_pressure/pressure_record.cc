// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_record.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

PressureRecord::PressureRecord(V8PressureSource::Enum source,
                               V8PressureState::Enum state,
                               const double own_contribution_estimate,
                               const DOMHighResTimeStamp time)
    : source_(source),
      state_(state),
      own_contribution_estimate_(own_contribution_estimate),
      time_(time) {}

PressureRecord::~PressureRecord() = default;

V8PressureSource PressureRecord::source() const {
  return V8PressureSource(source_);
}

V8PressureState PressureRecord::state() const {
  return V8PressureState(state_);
}

std::optional<double> PressureRecord::ownContributionEstimate() const {
  if (own_contribution_estimate_ <= 0) {
    return std::nullopt;
  }
  return own_contribution_estimate_;
}

DOMHighResTimeStamp PressureRecord::time() const {
  return time_;
}

ScriptObject PressureRecord::toJSON(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddString("source", source().AsCStr());
  result.AddString("state", state().AsCStr());
  if (ownContributionEstimate().has_value()) {
    result.AddNumber("ownContributionEstimate", own_contribution_estimate_);
  } else {
    result.AddNull("ownContributionEstimate");
  }
  result.AddNumber("time", time());
  return result.ToScriptObject();
}

}  // namespace blink
