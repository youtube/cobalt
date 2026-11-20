// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_accessibility_manager.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/accessibility_features.h"

namespace blink {

namespace {

// Two rectangles are considered to be sufficiently overlapping if the
// intersection area is greater than 80% of the area of either rectangle. The
// value is chosen arbitrarily and can be tuned.
constexpr float kSufficientlyOverlappingThreshold = 0.8f;

// Two texts are considered to be on the same line if the vertical distance
// between them is less than 10% of the size of the smaller font. The value is
// chosen arbitrarily and can be tuned.
constexpr float kSameLineFontRatioThreshold = 0.1f;

// Delay to ensure the canvas element has had a chance to update its
// accessibility related information.
constexpr base::TimeDelta kUMATimerDelay = base::Seconds(5);

bool SufficientlyOverlapping(const gfx::RectF& a, const gfx::RectF& b) {
  gfx::RectF intersection = gfx::IntersectRects(a, b);
  if (intersection.IsEmpty()) {
    return false;
  }
  float intersection_area = intersection.size().GetArea();
  float a_area = a.size().GetArea();
  float b_area = b.size().GetArea();
  CHECK_GT(a_area, 0);
  CHECK_GT(b_area, 0);
  return (intersection_area / a_area > kSufficientlyOverlappingThreshold) ||
         (intersection_area / b_area > kSufficientlyOverlappingThreshold);
}

}  // namespace

HTMLCanvasAccessibilityManager::HTMLCanvasAccessibilityManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool is_ignored,
    HTMLCanvasElement* canvas_element,
    bool is_for_ukm_only)
    : is_ignored_(is_ignored),
      uma_timer_(std::move(task_runner),
                 this,
                 &HTMLCanvasAccessibilityManager::RecordUma),
      is_for_ukm_only_(is_for_ukm_only),
      canvas_element_(canvas_element) {
  UpdateHasFallbackElementContent();

  has_layoutsubtree_ = canvas_element->layoutSubtree();

  ReadAriaAttributes();

  is_initialized_ = true;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::SetIgnored(bool is_ignored) {
  // If the manager is in `is_for_ukm_only` mode, exit the mode and update
  // anyway.
  if (is_for_ukm_only_) {
    is_for_ukm_only_ = false;
  } else if (is_ignored_ == is_ignored) {
    return;
  }
  // The manager is only created if the canvas is initially not ignored. If it
  // becomes ignored later, we keep it to avoid the overhead of repeated
  // creation and destruction.
  is_ignored_ = is_ignored;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::UpdateHasFallbackElementContent() {
  bool has_fallback_element_content =
      ElementTraversal::FirstChild(*canvas_element_) != nullptr;
  if (has_fallback_element_content_ == has_fallback_element_content) {
    return;
  }
  has_fallback_element_content_ = has_fallback_element_content;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::ReadAriaAttributes() {
  if (is_ignored_) {
    // If the canvas is ignored, accessibility support is not needed and we
    // don't need to check for aria attributes.
    return;
  }
  if (!has_aria_attributes_) {
    has_aria_attributes_ =
        canvas_element_->FastHasAttribute(html_names::kRoleAttr) ||
        canvas_element_->FastHasAttribute(html_names::kAriaLabelAttr) ||
        canvas_element_->FastHasAttribute(html_names::kAriaLabelledbyAttr) ||
        canvas_element_->FastHasAttribute(html_names::kAriaDescribedbyAttr) ||
        canvas_element_->FastHasAttribute(html_names::kTitleAttr);
  }

  OnUpdate();
}

void HTMLCanvasAccessibilityManager::SetHasLayoutSubtree(
    bool has_layoutsubtree) {
  if (has_layoutsubtree_ == has_layoutsubtree) {
    return;
  }
  has_layoutsubtree_ = has_layoutsubtree;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::OnResize() {
  ClearRenderedText();
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::OnUpdate() {
  if (!is_initialized_) {
    return;
  }

  // If AX ignores the canvas, it definitely doesn't need support.
  // Note that covers elements with aria-hidden=true, or not visible, or 1x1
  // pixel size are ignored.
  if (is_ignored_) {
    SetHeuristicResult(HeuristicResult::kIsIgnored);
    return;
  }

  // Minimum size canvas to assume it can have text.
  if (IsTooSmall()) {
    SetHeuristicResult(HeuristicResult::kTooSmall);
    return;
  }

  if (has_layoutsubtree_) {
    SetHeuristicResult(HeuristicResult::kHasLayoutSubtree);
    return;
  }

  if (has_fallback_element_content_) {
    SetHeuristicResult(HeuristicResult::kHasFallbackContent);
    return;
  }

  if (has_aria_attributes_) {
    SetHeuristicResult(HeuristicResult::kHasAriaAttributes);
    return;
  }

  SetHeuristicResult(HeuristicResult::kNeedsA11ySupport);
}

bool HTMLCanvasAccessibilityManager::IsTooSmall() const {
  const float kMinPixelSizeForTextVisibility = 10;
  gfx::SizeF layout_size;
  if (auto* layout_box =
          DynamicTo<LayoutBox>(canvas_element_->GetLayoutObject())) {
    layout_size.set_width(layout_box->LogicalWidth().ToFloat());
    layout_size.set_height(layout_box->LogicalHeight().ToFloat());
  }
  return layout_size.width() < kMinPixelSizeForTextVisibility ||
         layout_size.height() < kMinPixelSizeForTextVisibility;
}

void HTMLCanvasAccessibilityManager::SetHeuristicResult(
    HeuristicResult result) {
  if (heuristic_result_ == result) {
    return;
  }
  heuristic_result_ = result;

  if (!is_uma_recorded_ && !is_for_ukm_only_) {
    uma_timer_.StartOneShot(kUMATimerDelay, FROM_HERE);
  }

  if (heuristic_result_ == HeuristicResult::kNeedsA11ySupport &&
      base::FeatureList::IsEnabled(::features::kAccessibilityCanvas) &&
      !is_for_ukm_only_) {
    should_capture_rendered_text_ = true;
  } else if (should_capture_rendered_text_) {
    should_capture_rendered_text_ = false;
    ClearRenderedText();
  }
  canvas_element_->UpdateCaptureRenderedText();
  // TODO(crbug.com/475512055): Add UKM for cases that need a11y support for
  // verification.
}

void HTMLCanvasAccessibilityManager::RecordUma(TimerBase*) {
  if (is_uma_recorded_) {
    return;
  }
  is_uma_recorded_ = true;
  base::UmaHistogramEnumeration("Accessibility.Canvas.HeuristicResult",
                                heuristic_result_);
}

void HTMLCanvasAccessibilityManager::FlushUmaIfNeeded() {
  if (uma_timer_.IsActive()) {
    uma_timer_.Stop();
    RecordUma(nullptr);
  }
}

void HTMLCanvasAccessibilityManager::RecordRenderedText(
    const String& text,
    const gfx::RectF& bounds,
    float font_height) {
  if (!should_capture_rendered_text_) {
    return;
  }

  // No need to capture empty text or bounds, or zero font height.
  if (font_height == 0 || text.empty() || bounds.IsEmpty()) {
    return;
  }

  // Check for overwrite.
  for (auto& run : text_runs_) {
    if (SufficientlyOverlapping(run.bounds, bounds)) {
      run.text = text;
      run.font_height = font_height;
      return;
    }
  }
  text_runs_.push_back(RenderedTextRun{text, bounds, font_height});
}

void HTMLCanvasAccessibilityManager::ClearRenderedText(const gfx::RectF& rect) {
  if (!should_capture_rendered_text_) {
    return;
  }

  // Remove runs that are mostly inside `rect`
  EraseIf(text_runs_, [&rect](const RenderedTextRun& run) {
    gfx::RectF intersection = gfx::IntersectRects(run.bounds, rect);
    if (intersection.IsEmpty()) {
      return false;
    }
    float run_area = run.bounds.size().GetArea();
    if (run_area == 0) {
      return true;
    }
    return (intersection.size().GetArea() / run_area) >
           kSufficientlyOverlappingThreshold;
  });
}

void HTMLCanvasAccessibilityManager::ClearRenderedText() {
  text_runs_.clear();
}

void HTMLCanvasAccessibilityManager::UpdateAnnotation() {
  if (text_runs_.empty()) {
    canvas_annotation_ = String();
  } else {
    // Sort runs: Y (top to bottom), then X (left to right)
    // TODO(crbug.com/498093320): Add language direction (LTR/RTL) to TextRuns
    // tests.
    Vector<RenderedTextRun> sorted_runs = text_runs_;
    std::sort(sorted_runs.begin(), sorted_runs.end(),
              [](const RenderedTextRun& a, const RenderedTextRun& b) {
                float y_diff = a.bounds.y() - b.bounds.y();
                float same_line_threshold =
                    std::min(a.font_height, b.font_height) *
                    kSameLineFontRatioThreshold;
                if (std::abs(y_diff) < same_line_threshold) {
                  return a.bounds.x() < b.bounds.x();
                }
                return a.bounds.y() < b.bounds.y();
              });

    StringBuilder builder;
    for (const auto& run : sorted_runs) {
      if (!builder.empty()) {
        // TODO(crbug.com/498093320): This may add space in the middle of words
        // as there can be cases where a word is split into multiple runs.
        // Consider using font height to determine if the gap is large enough to
        // require a space.
        builder.Append(' ');
      }
      builder.Append(run.text);
    }
    canvas_annotation_ = builder.ToString();
  }

  if (AXObjectCache* cache =
          canvas_element_->GetDocument().ExistingAXObjectCache()) {
    cache->MarkElementDirty(canvas_element_);
  }
}

void HTMLCanvasAccessibilityManager::Trace(Visitor* visitor) const {
  visitor->Trace(uma_timer_);
  visitor->Trace(canvas_element_);
}

}  // namespace blink
