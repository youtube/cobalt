// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_script_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/task_attribution_timing.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {
AtomicString GetScriptName(ScriptTimingInfo* info,
                           LocalDOMWindow* owner_window) {
  const String& url =
      info->GetSourceLocation().url
          ? info->GetSourceLocation().url
          : (owner_window ? owner_window->BaseURL().GetString() : "inline");

  switch (info->GetType()) {
    case ScriptTimingInfo::Type::kClassicScript:
    case ScriptTimingInfo::Type::kModuleScript:
    case ScriptTimingInfo::Type::kExecuteScript:
      return AtomicString(url);
    case ScriptTimingInfo::Type::kEventHandler:
    case ScriptTimingInfo::Type::kUserCallback: {
      WTF::StringBuilder builder;
      if (info->GetType() == ScriptTimingInfo::Type::kEventHandler) {
        builder.Append(info->ClassLikeName());
        builder.Append(".");
        builder.Append("on");
      }
      builder.Append(info->PropertyLikeName());
      return builder.ToAtomicString();
    }

    case ScriptTimingInfo::Type::kPromiseResolve:
    case ScriptTimingInfo::Type::kPromiseReject: {
      WTF::StringBuilder builder;
      if (info->ClassLikeName().empty() && info->PropertyLikeName().empty()) {
        return info->GetType() == ScriptTimingInfo::Type::kPromiseResolve
                   ? "Promise.resolve"
                   : "Promise.reject";
      }

      if (!info->ClassLikeName().empty()) {
        builder.Append(info->ClassLikeName());
        builder.Append(".");
      }
      builder.Append(info->PropertyLikeName());
      builder.Append(".");
      builder.Append(info->GetType() == ScriptTimingInfo::Type::kPromiseResolve
                         ? "then"
                         : "catch");
      return builder.ToAtomicString();
    }
  }
}
}  // namespace

PerformanceScriptTiming::PerformanceScriptTiming(
    ScriptTimingInfo* info,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    DOMWindow* source)
    : PerformanceEntry(
          (info->EndTime() - info->StartTime()).InMilliseconds(),
          GetScriptName(info, info->Window()),
          DOMWindowPerformance::performance(*source->ToLocalDOMWindow())
              ->MonotonicTimeToDOMHighResTimeStamp(info->StartTime()),
          source) {
  info_ = info;
  time_origin_ = time_origin;
  cross_origin_isolated_capability_ = cross_origin_isolated_capability;
  DCHECK(info_->Window() && source);
  if (info_->Window() == source) {
    window_attribution_ = "self";
  } else if (!info_->Window()->GetFrame()) {
    window_attribution_ = "other";
  } else if (info_->Window()->GetFrame()->Tree().IsDescendantOf(
                 source->GetFrame())) {
    window_attribution_ = "descendant";
  } else if (source->GetFrame()->Tree().IsDescendantOf(
                 info_->Window()->GetFrame())) {
    window_attribution_ = "ancestor";
  } else if (source->GetFrame()->Tree().Top() ==
             info_->Window()->GetFrame()->Top()) {
    window_attribution_ = "same-page";
  } else {
    window_attribution_ = "other";
  }
}

PerformanceScriptTiming::~PerformanceScriptTiming() = default;

const AtomicString& PerformanceScriptTiming::entryType() const {
  return performance_entry_names::kScript;
}

DOMHighResTimeStamp PerformanceScriptTiming::executionStart() const {
  return ToMonotonicTime(info_->ExecutionStartTime());
}

DOMHighResTimeStamp PerformanceScriptTiming::desiredExecutionStart() const {
  return ToMonotonicTime(info_->DesiredExecutionStartTime());
}

DOMHighResTimeStamp PerformanceScriptTiming::ToMonotonicTime(
    base::TimeTicks time) const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, time, /*allow_negative_value=*/false,
      cross_origin_isolated_capability_);
}

DOMHighResTimeStamp PerformanceScriptTiming::forcedStyleAndLayoutDuration()
    const {
  return (info_->StyleDuration() + info_->LayoutDuration()).InMilliseconds();
}

DOMHighResTimeStamp PerformanceScriptTiming::pauseDuration() const {
  return info_->PauseDuration().InMilliseconds();
}

LocalDOMWindow* PerformanceScriptTiming::window() const {
  return info_->Window();
}

const AtomicString& PerformanceScriptTiming::windowAttribution() const {
  return window_attribution_;
}
AtomicString PerformanceScriptTiming::type() const {
  switch (info_->GetType()) {
    case ScriptTimingInfo::Type::kClassicScript:
      return "classic-script";
    case ScriptTimingInfo::Type::kModuleScript:
      return "module-script";
    case ScriptTimingInfo::Type::kExecuteScript:
      return "execute-script";
    case ScriptTimingInfo::Type::kEventHandler:
      return "event-listener";
    case ScriptTimingInfo::Type::kUserCallback:
      return "user-callback";
    case ScriptTimingInfo::Type::kPromiseResolve:
      return "resolve-promise";
    case ScriptTimingInfo::Type::kPromiseReject:
      return "reject-promise";
  }
}

WTF::String PerformanceScriptTiming::sourceLocation() const {
  const ScriptTimingInfo::ScriptSourceLocation& source_location =
      info_->GetSourceLocation();
  if (!source_location.url) {
    return WTF::String();
  }

  StringBuilder builder;
  if (!source_location.function_name.empty()) {
    builder.Append(source_location.function_name);
    builder.Append("@");
  }

  builder.Append(source_location.url);
  builder.Append(":");
  builder.AppendNumber(source_location.line_number);
  builder.Append(":");
  builder.AppendNumber(source_location.column_number);
  return builder.ToString();
}

PerformanceEntryType PerformanceScriptTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kScript;
}

void PerformanceScriptTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("type", type());
  builder.AddString("windowAttribution", windowAttribution());
  builder.AddNumber("executionStart", executionStart());
  builder.AddNumber("desiredExecutionStart", desiredExecutionStart());
  builder.AddNumber("forcedStyleAndLayoutDuration",
                    forcedStyleAndLayoutDuration());
  builder.AddNumber("pauseDuration", pauseDuration());
  builder.AddString("sourceLocation", sourceLocation());
}

void PerformanceScriptTiming::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
  visitor->Trace(info_);
}

}  // namespace blink
