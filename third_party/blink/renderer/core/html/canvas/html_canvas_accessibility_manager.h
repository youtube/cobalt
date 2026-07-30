// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_HTML_CANVAS_ACCESSIBILITY_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_HTML_CANVAS_ACCESSIBILITY_MANAGER_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class HTMLCanvasElement;

// HTMLCanvasAccessibilityManager contains the accessibility information of the
// canvas element, determines if the canvas element needs accessibility support,
// and adds the support if needed.
class CORE_EXPORT HTMLCanvasAccessibilityManager
    : public GarbageCollected<HTMLCanvasAccessibilityManager> {
 public:
  using Options = unsigned;
  enum Option : Options {
    kNone = 0,
    kCollectTextRuns = 1 << 0,
    kUpdateHeuristicResults = 1 << 1,
    kAnnotateAXTree = 1 << 2,
    kAll = kCollectTextRuns | kUpdateHeuristicResults | kAnnotateAXTree,
  };

  // `canvas_element` should outlive this object and should be the owner of it.
  HTMLCanvasAccessibilityManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool is_ignored,
      HTMLCanvasElement* canvas_element,
      Options options);
  HTMLCanvasAccessibilityManager(const HTMLCanvasAccessibilityManager&) =
      delete;
  HTMLCanvasAccessibilityManager& operator=(
      const HTMLCanvasAccessibilityManager&) = delete;

  bool NeedsA11ySupport() const {
    return (options_ & kAnnotateAXTree) &&
           heuristic_result_ == HeuristicResult::kNeedsA11ySupport;
  }

  void Trace(Visitor* visitor) const;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(CanvasAccessibilityHeuristicResult)
  enum class HeuristicResult {
    kUnknown,
    kDeprecatedIsNotVisible,  // Not used anymore.
    kIsIgnored,
    kTooSmall,
    kHasLayoutSubtree,
    kHasFallbackContent,
    kHasAriaAttributes,
    kNeedsA11ySupport,
    kMaxValue = kNeedsA11ySupport,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:CanvasAccessibilityHeuristicResult)

  HeuristicResult GetHeuristicResultForTesting() const {
    return heuristic_result_;
  }

  void ReadAriaAttributes();

  void SetIsIgnored(bool is_ignored);
  void EnableUpdatingHeuristicResults();

  void SetHasLayoutSubtree(bool has_layoutsubtree);

  void UpdateHasFallbackElementContent();

  void OnResize();

  void OnUpdate();

  void RecordRenderedText(const String& text,
                          const gfx::RectF& bounds,
                          float font_height);
  void ClearRenderedText(const gfx::RectF& rect);
  void ClearRenderedText();
  const String& CanvasAnnotation() const { return canvas_annotation_; }
  void UpdateAnnotation();

  // Records the heuristic result to UMA if it hasn't been recorded yet. UMA is
  // recorded as a best effort in a timer to let the canvas element update its
  // accessibility related information. If it is not recorder by the time the
  // canvas element's context is destroyed, it will be recorded then.
  void FlushUmaIfNeeded();

  bool ShouldCaptureRenderedTextForTesting() const {
    return options_ & kCollectTextRuns;
  }

 private:
  void SetHeuristicResult(HeuristicResult result);
  void RecordUma(TimerBase*);
  bool IsTooSmall() const;

  struct RenderedTextRun {
    String text;
    gfx::RectF bounds;
    float font_height;
  };

  Options options_;
  HeuristicResult heuristic_result_ = HeuristicResult::kUnknown;

  // If canvas is drawn by html-in-canvas API, it should have accessibility
  // support.
  bool has_layoutsubtree_ = false;

  // If canvas is ignored, accessibility support is not needed.
  bool is_ignored_ = false;

  // Set to true if any of the role, label, labelledby, describedby, or title
  // aria attributes are set, or aria-hidden=false. This is an indicator that
  // the developer has considered accessibility for the canvas.
  bool has_aria_attributes_ = false;

  // If the canvas has fallback element content, it already has accessibility
  // support. `Element` is used to avoid pure text or comment nodes.
  bool has_fallback_element_content_ = false;

  std::optional<bool> needs_a11y_support_;

  HeapTaskRunnerTimer<HTMLCanvasAccessibilityManager> uma_timer_;
  bool is_uma_recorded_ = false;

  // Owns this object and should outlive it.
  Member<HTMLCanvasElement> canvas_element_;

  Vector<RenderedTextRun> text_runs_;
  String canvas_annotation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_HTML_CANVAS_ACCESSIBILITY_MANAGER_H_
