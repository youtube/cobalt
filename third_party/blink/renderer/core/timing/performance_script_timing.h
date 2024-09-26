// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_SCRIPT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_SCRIPT_TIMING_H_

#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PerformanceScriptTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // This constructor uses int for |duration| to coarsen it in advance.
  // LongAnimationFrameTiming is always at 1-ms granularity.
  PerformanceScriptTiming(ScriptTimingInfo* info,
                          base::TimeTicks time_origin,
                          bool cross_origin_isolated_capability,
                          DOMWindow* source);
  ~PerformanceScriptTiming() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  DOMHighResTimeStamp executionStart() const;
  DOMHighResTimeStamp forcedStyleAndLayoutDuration() const;
  DOMHighResTimeStamp pauseDuration() const;
  DOMHighResTimeStamp desiredExecutionStart() const;
  LocalDOMWindow* window() const;
  WTF::String sourceLocation() const;
  const AtomicString& windowAttribution() const;
  AtomicString type() const;
  void Trace(Visitor*) const override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;
  DOMHighResTimeStamp ToMonotonicTime(base::TimeTicks) const;
  base::TimeTicks time_origin_;
  bool cross_origin_isolated_capability_;
  Member<ScriptTimingInfo> info_;
  AtomicString window_attribution_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_SCRIPT_TIMING_H_
