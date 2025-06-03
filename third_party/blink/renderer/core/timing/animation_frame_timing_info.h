// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_ANIMATION_FRAME_TIMING_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_ANIMATION_FRAME_TIMING_INFO_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class ExecutionContext;
class SourceLocation;

class ScriptTimingInfo : public GarbageCollected<ScriptTimingInfo> {
 public:
  enum class Type {
    kClassicScript,
    kModuleScript,
    kUserCallback,
    kEventHandler,
    kPromiseResolve,
    kPromiseReject
  };

  // Not using blink::SourceLocation directly as using it relies on stack traces
  // even in simple cases. We often only need the URL (e.g script blocks) so
  // this is a lighter-weight version.
  struct ScriptSourceLocation {
    WTF::String url;
    WTF::String function_name;
    int start_position = 0;
  };

  ScriptTimingInfo(ExecutionContext* context,
                   Type type,
                   base::TimeTicks start_time,
                   base::TimeTicks execution_start_time,
                   base::TimeTicks end_time,
                   base::TimeDelta style_duration,
                   base::TimeDelta layout_duration);

  void Trace(Visitor* visitor) const;
  Type GetType() const { return type_; }
  base::TimeTicks StartTime() const { return start_time_; }
  base::TimeTicks ExecutionStartTime() const { return execution_start_time_; }
  base::TimeTicks EndTime() const { return end_time_; }
  base::TimeTicks DesiredExecutionStartTime() const {
    return desired_execution_start_time_;
  }
  void SetDesiredExecutionStartTime(base::TimeTicks queue_time) {
    desired_execution_start_time_ = queue_time;
  }
  base::TimeDelta PauseDuration() const { return pause_duration_; }
  void SetPauseDuration(base::TimeDelta duration) {
    pause_duration_ = duration;
  }
  base::TimeDelta StyleDuration() const { return style_duration_; }
  base::TimeDelta LayoutDuration() const { return layout_duration_; }
  const ScriptSourceLocation& GetSourceLocation() const {
    return source_location_;
  }
  void SetSourceLocation(const ScriptSourceLocation& location) {
    source_location_ = location;
    if (KURL(location.url).ProtocolIsData()) {
      source_location_.url = "data:";
    }
  }

  const AtomicString& ClassLikeName() const { return class_like_name_; }
  void SetClassLikeName(const AtomicString& name) { class_like_name_ = name; }
  const AtomicString& PropertyLikeName() const { return property_like_name_; }
  void SetPropertyLikeName(const AtomicString& name) {
    property_like_name_ = name;
  }
  LocalDOMWindow* Window() const { return window_.Get(); }
  const SecurityOrigin* GetSecurityOrigin() const {
    return security_origin_.get();
  }

 private:
  Type type_;
  AtomicString class_like_name_ = WTF::g_empty_atom;
  AtomicString property_like_name_ = WTF::g_empty_atom;
  base::TimeTicks start_time_;
  base::TimeTicks execution_start_time_;
  base::TimeTicks end_time_;
  base::TimeTicks desired_execution_start_time_;
  base::TimeDelta style_duration_;
  base::TimeDelta layout_duration_;
  base::TimeDelta pause_duration_;
  ScriptSourceLocation source_location_;
  WeakMember<LocalDOMWindow> window_;
  scoped_refptr<const SecurityOrigin> security_origin_;
};

class AnimationFrameTimingInfo
    : public GarbageCollected<AnimationFrameTimingInfo> {
 public:
  explicit AnimationFrameTimingInfo(base::TimeTicks start_time)
      : frame_start_time(start_time) {}
  void SetRenderStartTime(base::TimeTicks time) { render_start_time = time; }

  void SetStyleAndLayoutStartTime(base::TimeTicks time) {
    style_and_layout_start_time = time;
  }

  void SetRenderEndTime(base::TimeTicks time) { render_end_time = time; }
  void SetDesiredRenderStartTime(base::TimeTicks time) {
    desired_render_start_time = time;
  }
  void SetFirstUIEventTime(base::TimeTicks time) { first_ui_event_time = time; }

  base::TimeTicks FrameStartTime() const { return frame_start_time; }
  base::TimeTicks RenderStartTime() const { return render_start_time; }
  base::TimeTicks StyleAndLayoutStartTime() const {
    return style_and_layout_start_time;
  }
  base::TimeTicks RenderEndTime() const { return render_end_time; }
  base::TimeTicks DesiredRenderStartTime() const {
    return desired_render_start_time;
  }
  base::TimeTicks FirstUIEventTime() const { return first_ui_event_time; }
  base::TimeDelta Duration() const {
    return RenderEndTime() - FrameStartTime();
  }

  const HeapVector<Member<ScriptTimingInfo>>& Scripts() const {
    return scripts_;
  }

  void SetScripts(const HeapVector<Member<ScriptTimingInfo>>& scripts) {
    scripts_ = scripts;
  }

  const base::TimeDelta& TotalBlockingDuration() const {
    return total_blocking_duration_;
  }

  void SetTotalBlockingDuration(base::TimeDelta duration) {
    total_blocking_duration_ = duration;
  }

  void SetDidPause() { did_pause_ = true; }
  bool DidPause() const { return did_pause_; }

  virtual void Trace(Visitor*) const;

 private:
  // Measured at the beginning of the first task that caused a frame update,
  // or at the beginning of rendering.
  base::TimeTicks frame_start_time;

  // Measured right before BeginMainFrame ("update the rendering").
  base::TimeTicks render_start_time;

  // Measured when we start the main frame lifecycle of styling and layouting.
  base::TimeTicks style_and_layout_start_time;

  // Measured after BeginMainFrame, or at the end of a task that did not trigger
  // a main frame update
  base::TimeTicks render_end_time;

  // The desired time of the frame, when the compositor is ready to receive it.
  // Should be the same as the timestamp received in requestAnimationFrame()
  // callbacks.
  base::TimeTicks desired_render_start_time;

  // The event timestamp of the first UI event that coincided with the frame.
  base::TimeTicks first_ui_event_time;

  // Collecting durations of all tasks in the LoAF, not including rendering.
  base::TimeDelta total_blocking_duration_;

  HeapVector<Member<ScriptTimingInfo>> scripts_;

  // Whether the LoAF included sync XHR or alerts (pause).
  bool did_pause_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_ANIMATION_FRAME_TIMING_INFO_H_
