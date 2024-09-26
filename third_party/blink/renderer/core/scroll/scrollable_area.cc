/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/scroll_utils.h"
#include "cc/input/scrollbar.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/scroll/mac_scrollbar_animator.h"
#include "third_party/blink/renderer/core/scroll/programmatic_scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

int ScrollableArea::PixelsPerLineStep(LocalFrame* frame) {
  if (!frame)
    return cc::kPixelsPerLineStep;
  return frame->GetPage()->GetChromeClient().WindowToViewportScalar(
      frame, cc::kPixelsPerLineStep);
}

float ScrollableArea::MinFractionToStepWhenPaging() {
  return cc::kMinFractionToStepWhenPaging;
}

int ScrollableArea::MaxOverlapBetweenPages() const {
  return GetPageScrollbarTheme().MaxOverlapBetweenPages();
}

// static
float ScrollableArea::DirectionBasedScrollDelta(
    ui::ScrollGranularity granularity) {
  return (granularity == ui::ScrollGranularity::kScrollByPercentage)
             ? cc::kPercentDeltaForDirectionalScroll
             : 1;
}

// static
mojom::blink::ScrollBehavior ScrollableArea::DetermineScrollBehavior(
    mojom::blink::ScrollBehavior behavior_from_param,
    mojom::blink::ScrollBehavior behavior_from_style) {
  if (behavior_from_param == mojom::blink::ScrollBehavior::kSmooth)
    return mojom::blink::ScrollBehavior::kSmooth;

  if (behavior_from_param == mojom::blink::ScrollBehavior::kAuto &&
      behavior_from_style == mojom::blink::ScrollBehavior::kSmooth) {
    return mojom::blink::ScrollBehavior::kSmooth;
  }

  return mojom::blink::ScrollBehavior::kInstant;
}

ScrollableArea::ScrollableArea(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
    : scrollbar_overlay_color_theme_(kScrollbarOverlayColorThemeDark),
      horizontal_scrollbar_needs_paint_invalidation_(false),
      vertical_scrollbar_needs_paint_invalidation_(false),
      scroll_corner_needs_paint_invalidation_(false),
      scrollbars_hidden_if_overlay_(true),
      scrollbar_captured_(false),
      mouse_over_scrollbar_(false),
      has_been_disposed_(false),
      compositor_task_runner_(std::move(compositor_task_runner)) {
  DCHECK(compositor_task_runner_);
}

ScrollableArea::~ScrollableArea() = default;

void ScrollableArea::Dispose() {
  if (HasBeenDisposed())
    return;
  DisposeImpl();
  fade_overlay_scrollbars_timer_ = nullptr;
  has_been_disposed_ = true;
}

void ScrollableArea::ClearScrollableArea() {
  if (mac_scrollbar_animator_)
    mac_scrollbar_animator_->Dispose();
  if (scroll_animator_) {
    scroll_animator_->DetachElement();
    scroll_animator_.Clear();
  }
  if (programmatic_scroll_animator_) {
    programmatic_scroll_animator_->DetachElement();
    programmatic_scroll_animator_.Clear();
  }
  if (fade_overlay_scrollbars_timer_)
    fade_overlay_scrollbars_timer_->Value().Stop();
}

MacScrollbarAnimator* ScrollableArea::GetMacScrollbarAnimator() const {
#if BUILDFLAG(IS_MAC)
  if (!mac_scrollbar_animator_) {
    mac_scrollbar_animator_ =
        MacScrollbarAnimator::Create(const_cast<ScrollableArea*>(this));
  }
#endif
  return mac_scrollbar_animator_;
}

ScrollAnimatorBase& ScrollableArea::GetScrollAnimator() const {
  if (!scroll_animator_)
    scroll_animator_ =
        ScrollAnimatorBase::Create(const_cast<ScrollableArea*>(this));

  return *scroll_animator_;
}

ProgrammaticScrollAnimator& ScrollableArea::GetProgrammaticScrollAnimator()
    const {
  if (!programmatic_scroll_animator_) {
    programmatic_scroll_animator_ =
        MakeGarbageCollected<ProgrammaticScrollAnimator>(
            const_cast<ScrollableArea*>(this));
  }

  return *programmatic_scroll_animator_;
}

ScrollbarOrientation ScrollableArea::ScrollbarOrientationFromDirection(
    ScrollDirectionPhysical direction) const {
  return (direction == kScrollUp || direction == kScrollDown)
             ? kVerticalScrollbar
             : kHorizontalScrollbar;
}

float ScrollableArea::ScrollStep(ui::ScrollGranularity granularity,
                                 ScrollbarOrientation orientation) const {
  switch (granularity) {
    case ui::ScrollGranularity::kScrollByLine:
      return LineStep(orientation);
    case ui::ScrollGranularity::kScrollByPage:
      return PageStep(orientation);
    case ui::ScrollGranularity::kScrollByDocument:
      return DocumentStep(orientation);
    case ui::ScrollGranularity::kScrollByPixel:
    case ui::ScrollGranularity::kScrollByPrecisePixel:
      return PixelStep(orientation);
    case ui::ScrollGranularity::kScrollByPercentage:
      return PercentageStep(orientation);
    default:
      NOTREACHED();
      return 0.0f;
  }
}

ScrollOffset ScrollableArea::ResolveScrollDelta(
    ui::ScrollGranularity granularity,
    const ScrollOffset& delta) {
  gfx::SizeF step(ScrollStep(granularity, kHorizontalScrollbar),
                  ScrollStep(granularity, kVerticalScrollbar));

  if (granularity == ui::ScrollGranularity::kScrollByPercentage) {
    LocalFrame* local_frame = GetLayoutBox()->GetFrame();
    DCHECK(local_frame);
    gfx::SizeF viewport(local_frame->GetPage()->GetVisualViewport().Size());

    // Convert to screen coordinates (physical pixels).
    float page_scale_factor = local_frame->GetPage()->PageScaleFactor();
    step.Scale(page_scale_factor);

    gfx::Vector2dF pixel_delta =
        cc::ScrollUtils::ResolveScrollPercentageToPixels(delta, step, viewport);

    // Rescale back to rootframe coordinates.
    pixel_delta.Scale(1 / page_scale_factor);

    return pixel_delta;
  }

  return gfx::ScaleVector2d(delta, step.width(), step.height());
}

ScrollResult ScrollableArea::UserScroll(ui::ScrollGranularity granularity,
                                        const ScrollOffset& delta,
                                        ScrollCallback on_finish) {
  TRACE_EVENT2("input", "ScrollableArea::UserScroll", "x", delta.x(), "y",
               delta.y());

  // This callback runs ScrollableArea::RunScrollCompleteCallbacks which
  // will run all the callbacks in the Vector pending_scroll_complete_callbacks_
  // and ScrollAnimator::UserScroll may run this callback for a previous scroll
  // animation. Delay queuing up this |on_finish| so that it is run when the
  // callback for this scroll animation is run and not when the callback
  // for a previous scroll animation is run.
  ScrollCallback run_scroll_complete_callbacks(BindOnce(
      [](WeakPersistent<ScrollableArea> area, ScrollCallback callback,
         ScrollCompletionMode mode) {
        if (area) {
          if (callback) {
            area->RegisterScrollCompleteCallback(std::move(callback));
          }
          area->RunScrollCompleteCallbacks(mode);
        }
      },
      WrapWeakPersistent(this), std::move(on_finish)));

  ScrollOffset pixel_delta = ResolveScrollDelta(granularity, delta);

  ScrollOffset scrollable_axis_delta(
      UserInputScrollable(kHorizontalScrollbar) ? pixel_delta.x() : 0,
      UserInputScrollable(kVerticalScrollbar) ? pixel_delta.y() : 0);
  ScrollOffset delta_to_consume =
      GetScrollAnimator().ComputeDeltaToConsume(scrollable_axis_delta);

  if (delta_to_consume.IsZero()) {
    std::move(run_scroll_complete_callbacks)
        .Run(ScrollCompletionMode::kZeroDelta);
    return ScrollResult(false, false, pixel_delta.x(), pixel_delta.y());
  }

  CancelProgrammaticScrollAnimation();
  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer())
    sequencer->AbortAnimations();

  ScrollResult result =
      GetScrollAnimator().UserScroll(granularity, scrollable_axis_delta,
                                     std::move(run_scroll_complete_callbacks));

  // Delta that wasn't scrolled because the axis is !userInputScrollable
  // should count as unusedScrollDelta.
  ScrollOffset unscrollable_axis_delta = pixel_delta - scrollable_axis_delta;
  result.unused_scroll_delta_x += unscrollable_axis_delta.x();
  result.unused_scroll_delta_y += unscrollable_axis_delta.y();

  return result;
}

ScrollOffset ScrollableArea::PendingScrollAnchorAdjustment() const {
  return pending_scroll_anchor_adjustment_;
}

void ScrollableArea::ClearPendingScrollAnchorAdjustment() {
  pending_scroll_anchor_adjustment_ = ScrollOffset();
}

void ScrollableArea::SetScrollOffset(const ScrollOffset& offset,
                                     mojom::blink::ScrollType scroll_type,
                                     mojom::blink::ScrollBehavior behavior,
                                     ScrollCallback on_finish) {
  if (on_finish)
    RegisterScrollCompleteCallback(std::move(on_finish));

  ScrollableArea::ScrollCallback run_scroll_complete_callbacks(WTF::BindOnce(
      [](WeakPersistent<ScrollableArea> area, ScrollCompletionMode mode) {
        if (area) {
          area->RunScrollCompleteCallbacks(mode);
        }
      },
      WrapWeakPersistent(this)));

  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer()) {
    if (sequencer->FilterNewScrollOrAbortCurrent(scroll_type)) {
      std::move(run_scroll_complete_callbacks)
          .Run(ScrollCompletionMode::kFinished);
      return;
    }
  }

  ScrollOffset previous_offset = GetScrollOffset();

  ScrollOffset clamped_offset = ClampScrollOffset(offset);
  if (clamped_offset == previous_offset &&
      scroll_type != mojom::blink::ScrollType::kProgrammatic) {
    std::move(run_scroll_complete_callbacks)
        .Run(ScrollCompletionMode::kZeroDelta);
    return;
  }

  TRACE_EVENT2("blink", "ScrollableArea::SetScrollOffset", "cur_x",
               GetScrollOffset().x(), "cur_y", GetScrollOffset().y());
  TRACE_EVENT_INSTANT1("blink", "Type", TRACE_EVENT_SCOPE_THREAD, "type",
                       scroll_type);
  TRACE_EVENT_INSTANT1("blink", "Behavior", TRACE_EVENT_SCOPE_THREAD,
                       "behavior", behavior);

  if (behavior == mojom::blink::ScrollBehavior::kAuto)
    behavior = ScrollBehaviorStyle();

  gfx::Vector2d animation_adjustment = gfx::ToRoundedVector2d(clamped_offset) -
                                       gfx::ToRoundedVector2d(previous_offset);

  switch (scroll_type) {
    case mojom::blink::ScrollType::kCompositor:
      ScrollOffsetChanged(clamped_offset, scroll_type);
      break;
    case mojom::blink::ScrollType::kClamping:
      ScrollOffsetChanged(clamped_offset, scroll_type);
      GetScrollAnimator().AdjustAnimation(animation_adjustment);
      break;
    case mojom::blink::ScrollType::kAnchoring:
      ScrollOffsetChanged(clamped_offset, scroll_type);
      GetScrollAnimator().AdjustAnimation(animation_adjustment);
      pending_scroll_anchor_adjustment_ += clamped_offset - previous_offset;
      break;
    case mojom::blink::ScrollType::kProgrammatic:
      ProgrammaticScrollHelper(clamped_offset, behavior,
                               /* is_sequenced_scroll */ false,
                               animation_adjustment,
                               std::move(run_scroll_complete_callbacks));
      return;
    case mojom::blink::ScrollType::kSequenced:
      ProgrammaticScrollHelper(clamped_offset, behavior,
                               /* is_sequenced_scroll */ true,
                               animation_adjustment,
                               std::move(run_scroll_complete_callbacks));
      return;
    case mojom::blink::ScrollType::kUser:
      UserScrollHelper(clamped_offset, behavior);
      break;
    default:
      NOTREACHED();
  }
  std::move(run_scroll_complete_callbacks).Run(ScrollCompletionMode::kFinished);
}

void ScrollableArea::SetScrollOffset(const ScrollOffset& offset,
                                     mojom::blink::ScrollType type,
                                     mojom::blink::ScrollBehavior behavior) {
  SetScrollOffset(offset, type, behavior, ScrollCallback());
}

void ScrollableArea::ScrollBy(const ScrollOffset& delta,
                              mojom::blink::ScrollType type,
                              mojom::blink::ScrollBehavior behavior) {
  SetScrollOffset(GetScrollOffset() + delta, type, behavior);
}

void ScrollableArea::ProgrammaticScrollHelper(
    const ScrollOffset& offset,
    mojom::blink::ScrollBehavior scroll_behavior,
    bool is_sequenced_scroll,
    gfx::Vector2d animation_adjustment,
    ScrollCallback on_finish) {
  bool should_use_animation =
      scroll_behavior == mojom::blink::ScrollBehavior::kSmooth &&
      ScrollAnimatorEnabled();
  if (should_use_animation) {
    // If the programmatic scroll will be animated, cancel any user scroll
    // animation already in progress. We don't want two scroll animations
    // running at the same time.
    CancelScrollAnimation();
  }

  if (offset == GetScrollOffset()) {
    CancelProgrammaticScrollAnimation();
    if (on_finish)
      std::move(on_finish).Run(ScrollCompletionMode::kZeroDelta);
    return;
  }

  ScrollCallback callback = std::move(on_finish);
  callback = ScrollCallback(WTF::BindOnce(
      [](ScrollCallback original_callback, WeakPersistent<ScrollableArea> area,
         ScrollCompletionMode mode) {
        if (area) {
          area->OnScrollFinished(/*enqueue_scrollend=*/mode ==
                                 ScrollCompletionMode::kFinished);
        }
        if (original_callback)
          std::move(original_callback).Run(mode);
      },
      std::move(callback), WrapWeakPersistent(this)));

  if (should_use_animation) {
    GetProgrammaticScrollAnimator().AnimateToOffset(offset, is_sequenced_scroll,
                                                    std::move(callback));
  } else {
    GetProgrammaticScrollAnimator().ScrollToOffsetWithoutAnimation(
        offset, is_sequenced_scroll);

    // If the programmatic scroll was NOT animated, we should adjust (but not
    // cancel) a user scroll animation already in progress (crbug.com/1264266).
    GetScrollAnimator().AdjustAnimation(animation_adjustment);

    if (callback)
      std::move(callback).Run(ScrollCompletionMode::kFinished);
  }
}

void ScrollableArea::UserScrollHelper(
    const ScrollOffset& offset,
    mojom::blink::ScrollBehavior scroll_behavior) {
  CancelProgrammaticScrollAnimation();
  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer())
    sequencer->AbortAnimations();

  float x = UserInputScrollable(kHorizontalScrollbar)
                ? offset.x()
                : GetScrollAnimator().CurrentOffset().x();
  float y = UserInputScrollable(kVerticalScrollbar)
                ? offset.y()
                : GetScrollAnimator().CurrentOffset().y();

  // Smooth user scrolls (keyboard, wheel clicks) are handled via the userScroll
  // method.
  // TODO(bokan): The userScroll method should probably be modified to call this
  //              method and ScrollAnimatorBase to have a simpler
  //              animateToOffset method like the ProgrammaticScrollAnimator.
  DCHECK_EQ(scroll_behavior, mojom::blink::ScrollBehavior::kInstant);
  GetScrollAnimator().ScrollToOffsetWithoutAnimation(ScrollOffset(x, y));
}

PhysicalRect ScrollableArea::ScrollIntoView(
    const PhysicalRect& rect_in_absolute,
    const mojom::blink::ScrollIntoViewParamsPtr& params) {
  // TODO(bokan): This should really be implemented here but ScrollAlignment is
  // in Core which is a dependency violation.
  NOTREACHED();
  return PhysicalRect();
}

void ScrollableArea::ScrollOffsetChanged(const ScrollOffset& offset,
                                         mojom::blink::ScrollType scroll_type) {
  TRACE_EVENT2("input", "ScrollableArea::scrollOffsetChanged", "x", offset.x(),
               "y", offset.y());
  TRACE_EVENT_INSTANT1("input", "Type", TRACE_EVENT_SCOPE_THREAD, "type",
                       scroll_type);

  ScrollOffset old_offset = GetScrollOffset();
  ScrollOffset truncated_offset =
      ShouldUseIntegerScrollOffset()
          ? ScrollOffset(gfx::ToFlooredVector2d(offset))
          : offset;

  // Tell the derived class to scroll its contents.
  UpdateScrollOffset(truncated_offset, scroll_type);

  // If the layout object has been detached as a result of updating the scroll
  // this object will be cleaned up shortly.
  if (HasBeenDisposed())
    return;

  // Tell the scrollbars to update their thumb postions.
  // If the scrollbar does not have its own layer, it must always be
  // invalidated to reflect the new thumb offset, even if the theme did not
  // invalidate any individual part.
  if (Scrollbar* horizontal_scrollbar = HorizontalScrollbar())
    horizontal_scrollbar->OffsetDidChange(scroll_type);
  if (Scrollbar* vertical_scrollbar = VerticalScrollbar())
    vertical_scrollbar->OffsetDidChange(scroll_type);

  ScrollOffset delta = GetScrollOffset() - old_offset;
  // TODO(skobes): Should we exit sooner when the offset has not changed?
  bool offset_changed = !delta.IsZero();

  if (GetMacScrollbarAnimator() && offset_changed &&
      IsExplicitScrollType(scroll_type) && ScrollbarsCanBeActive()) {
    GetMacScrollbarAnimator()->DidChangeUserVisibleScrollOffset(delta);
  }

  if (GetLayoutBox()) {
    if (offset_changed && GetLayoutBox()->GetFrameView() &&
        GetLayoutBox()
            ->GetFrameView()
            ->GetPaintTimingDetector()
            .NeedToNotifyInputOrScroll()) {
      GetLayoutBox()->GetFrameView()->GetPaintTimingDetector().NotifyScroll(
          scroll_type);
    }
  }

  if (offset_changed && GetLayoutBox() && GetLayoutBox()->GetFrameView()) {
    GetLayoutBox()->GetFrameView()->GetLayoutShiftTracker().NotifyScroll(
        scroll_type, delta);
  }

  GetScrollAnimator().SetCurrentOffset(offset);
}

bool ScrollableArea::ScrollBehaviorFromString(
    const String& behavior_string,
    mojom::blink::ScrollBehavior& behavior) {
  if (behavior_string == "auto")
    behavior = mojom::blink::ScrollBehavior::kAuto;
  else if (behavior_string == "instant")
    behavior = mojom::blink::ScrollBehavior::kInstant;
  else if (behavior_string == "smooth")
    behavior = mojom::blink::ScrollBehavior::kSmooth;
  else
    return false;

  return true;
}

void ScrollableArea::RegisterScrollCompleteCallback(ScrollCallback callback) {
  DCHECK(!HasBeenDisposed());
  pending_scroll_complete_callbacks_.push_back(std::move(callback));
}

void ScrollableArea::RunScrollCompleteCallbacks(ScrollCompletionMode mode) {
  Vector<ScrollCallback> callbacks(
      std::move(pending_scroll_complete_callbacks_));
  for (auto& callback : callbacks) {
    std::move(callback).Run(mode);
  }
}

void ScrollableArea::ContentAreaWillPaint() const {
  if (mac_scrollbar_animator_)
    mac_scrollbar_animator_->ContentAreaWillPaint();
}

void ScrollableArea::MouseEnteredContentArea() const {
  if (mac_scrollbar_animator_)
    mac_scrollbar_animator_->MouseEnteredContentArea();
}

void ScrollableArea::MouseExitedContentArea() const {
  if (mac_scrollbar_animator_)
    mac_scrollbar_animator_->MouseExitedContentArea();
}

void ScrollableArea::MouseMovedInContentArea() const {
  if (mac_scrollbar_animator_)
    mac_scrollbar_animator_->MouseMovedInContentArea();
}

void ScrollableArea::MouseEnteredScrollbar(Scrollbar& scrollbar) {
  mouse_over_scrollbar_ = true;

  if (GetMacScrollbarAnimator())
    GetMacScrollbarAnimator()->MouseEnteredScrollbar(scrollbar);
  ShowNonMacOverlayScrollbars();
  if (fade_overlay_scrollbars_timer_)
    fade_overlay_scrollbars_timer_->Value().Stop();
}

void ScrollableArea::MouseExitedScrollbar(Scrollbar& scrollbar) {
  mouse_over_scrollbar_ = false;

  if (GetMacScrollbarAnimator())
    GetMacScrollbarAnimator()->MouseExitedScrollbar(scrollbar);
  if (HasOverlayScrollbars() && !scrollbars_hidden_if_overlay_) {
    // This will kick off the fade out timer.
    ShowNonMacOverlayScrollbars();
  }
}

void ScrollableArea::MouseCapturedScrollbar() {
  scrollbar_captured_ = true;
  ShowNonMacOverlayScrollbars();
  if (fade_overlay_scrollbars_timer_)
    fade_overlay_scrollbars_timer_->Value().Stop();
}

void ScrollableArea::MouseReleasedScrollbar() {
  scrollbar_captured_ = false;
  // This will kick off the fade out timer.
  ShowNonMacOverlayScrollbars();
}

void ScrollableArea::DidAddScrollbar(Scrollbar& scrollbar,
                                     ScrollbarOrientation orientation) {
  if (GetMacScrollbarAnimator()) {
    if (orientation == kVerticalScrollbar)
      GetMacScrollbarAnimator()->DidAddVerticalScrollbar(scrollbar);
    else
      GetMacScrollbarAnimator()->DidAddHorizontalScrollbar(scrollbar);
  }

  // <rdar://problem/9797253> AppKit resets the scrollbar's style when you
  // attach a scrollbar
  SetScrollbarOverlayColorTheme(GetScrollbarOverlayColorTheme());
}

void ScrollableArea::WillRemoveScrollbar(Scrollbar& scrollbar,
                                         ScrollbarOrientation orientation) {
  if (mac_scrollbar_animator_) {
    if (orientation == kVerticalScrollbar)
      mac_scrollbar_animator_->WillRemoveVerticalScrollbar(scrollbar);
    else
      mac_scrollbar_animator_->WillRemoveHorizontalScrollbar(scrollbar);
  }
}

void ScrollableArea::ContentsResized() {
  if (mac_scrollbar_animator_)
    mac_scrollbar_animator_->ContentsResized();
}

bool ScrollableArea::HasOverlayScrollbars() const {
  Scrollbar* v_scrollbar = VerticalScrollbar();
  if (v_scrollbar && v_scrollbar->IsOverlayScrollbar())
    return true;
  Scrollbar* h_scrollbar = HorizontalScrollbar();
  return h_scrollbar && h_scrollbar->IsOverlayScrollbar();
}

void ScrollableArea::SetScrollbarOverlayColorTheme(
    ScrollbarOverlayColorTheme overlay_theme) {
  scrollbar_overlay_color_theme_ = overlay_theme;

  if (Scrollbar* scrollbar = HorizontalScrollbar()) {
    GetPageScrollbarTheme().UpdateScrollbarOverlayColorTheme(*scrollbar);
    scrollbar->SetNeedsPaintInvalidation(kAllParts);
  }

  if (Scrollbar* scrollbar = VerticalScrollbar()) {
    GetPageScrollbarTheme().UpdateScrollbarOverlayColorTheme(*scrollbar);
    scrollbar->SetNeedsPaintInvalidation(kAllParts);
  }
}

void ScrollableArea::RecalculateScrollbarOverlayColorTheme() {
  ScrollbarOverlayColorTheme old_overlay_theme =
      GetScrollbarOverlayColorTheme();

  // Start with a scrollbar overlay theme based on the used color scheme.
  ScrollbarOverlayColorTheme overlay_theme =
      UsedColorSchemeScrollbars() == mojom::blink::ColorScheme::kDark
          ? kScrollbarOverlayColorThemeLight
          : kScrollbarOverlayColorThemeDark;

  // If there is a background color set on the scroller, use the lightness of
  // the background color for the scrollbar overlay color theme.
  if (GetLayoutBox()) {
    Color background_color = GetLayoutBox()->StyleRef().VisitedDependentColor(
        GetCSSPropertyBackgroundColor());
    if (background_color.Alpha()) {
      double hue, saturation, lightness;
      background_color.GetHSL(hue, saturation, lightness);
      overlay_theme = lightness <= 0.5 ? kScrollbarOverlayColorThemeLight
                                       : kScrollbarOverlayColorThemeDark;
    }
  }

  if (old_overlay_theme != overlay_theme)
    SetScrollbarOverlayColorTheme(overlay_theme);
}

void ScrollableArea::SetScrollbarNeedsPaintInvalidation(
    ScrollbarOrientation orientation) {
  if (orientation == kHorizontalScrollbar)
    horizontal_scrollbar_needs_paint_invalidation_ = true;
  else
    vertical_scrollbar_needs_paint_invalidation_ = true;

  // Invalidate the scrollbar directly if it's already composited.
  // GetLayoutBox() may be null in some unit tests.
  if (auto* box = GetLayoutBox()) {
    auto* frame_view = GetLayoutBox()->GetFrameView();
    if (auto* compositor = frame_view->GetPaintArtifactCompositor()) {
      if (compositor->SetScrollbarNeedsDisplay(
              GetScrollbarElementId(orientation))) {
        if (auto* scrollbar = GetScrollbar(orientation)) {
          scrollbar->ClearNeedsUpdateDisplay();
        }
      }
    }
  }

  // TODO(crbug.com/1414885): we don't need to invalidate paint of scrollbar
  // for changes inside of the scrollbar. We'll invalidate raster if needed
  // after paint. We can remove some of paint invalidation code in this class,
  // and move remaining paint invalidation code into
  // PaintLayerScrollableArea and Scrollbar.
  ScrollControlWasSetNeedsPaintInvalidation();
}

void ScrollableArea::SetScrollCornerNeedsPaintInvalidation() {
  if (cc::Layer* layer = LayerForScrollCorner())
    layer->SetNeedsDisplay();
  scroll_corner_needs_paint_invalidation_ = true;
  ScrollControlWasSetNeedsPaintInvalidation();
}

void ScrollableArea::SetScrollControlsNeedFullPaintInvalidation() {
  if (auto* horizontal_scrollbar = HorizontalScrollbar())
    horizontal_scrollbar->SetNeedsPaintInvalidation(kAllParts);
  if (auto* vertical_scrollbar = VerticalScrollbar())
    vertical_scrollbar->SetNeedsPaintInvalidation(kAllParts);
  SetScrollCornerNeedsPaintInvalidation();
}

bool ScrollableArea::HasLayerForHorizontalScrollbar() const {
  return LayerForHorizontalScrollbar();
}

bool ScrollableArea::HasLayerForVerticalScrollbar() const {
  return LayerForVerticalScrollbar();
}

bool ScrollableArea::HasLayerForScrollCorner() const {
  return LayerForScrollCorner();
}

void ScrollableArea::MainThreadScrollingDidChange() {
  if (auto* programmatic_scroll_animator = ExistingProgrammaticScrollAnimator())
    programmatic_scroll_animator->MainThreadScrollingDidChange();
  if (auto* scroll_animator = ExistingScrollAnimator())
    scroll_animator->MainThreadScrollingDidChange();
}

void ScrollableArea::ServiceScrollAnimations(double monotonic_time) {
  bool requires_animation_service = false;
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator()) {
    scroll_animator->TickAnimation(base::Seconds(monotonic_time) +
                                   base::TimeTicks());
    if (scroll_animator->HasAnimationThatRequiresService())
      requires_animation_service = true;
  }
  if (ProgrammaticScrollAnimator* programmatic_scroll_animator =
          ExistingProgrammaticScrollAnimator()) {
    programmatic_scroll_animator->TickAnimation(base::Seconds(monotonic_time) +
                                                base::TimeTicks());
    if (programmatic_scroll_animator->HasAnimationThatRequiresService())
      requires_animation_service = true;
  }
  if (!requires_animation_service)
    DeregisterForAnimation();
}

void ScrollableArea::UpdateCompositorScrollAnimations() {
  if (ProgrammaticScrollAnimator* programmatic_scroll_animator =
          ExistingProgrammaticScrollAnimator())
    programmatic_scroll_animator->UpdateCompositorAnimations();

  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->UpdateCompositorAnimations();
}

void ScrollableArea::CancelScrollAnimation() {
  if (ScrollAnimatorBase* scroll_animator = ExistingScrollAnimator())
    scroll_animator->CancelAnimation();
}

void ScrollableArea::CancelProgrammaticScrollAnimation() {
  if (ProgrammaticScrollAnimator* programmatic_scroll_animator =
          ExistingProgrammaticScrollAnimator())
    programmatic_scroll_animator->CancelAnimation();
}

bool ScrollableArea::ScrollbarsHiddenIfOverlay() const {
  return HasOverlayScrollbars() && scrollbars_hidden_if_overlay_;
}

void ScrollableArea::SetScrollbarsHiddenForTesting(bool hidden) {
  // If scrollable area has been disposed, we can not get the page scrollbar
  // theme setting. Should early return here.
  if (HasBeenDisposed())
    return;

  SetScrollbarsHiddenIfOverlayInternal(hidden);
}

void ScrollableArea::SetScrollbarsHiddenFromExternalAnimator(bool hidden) {
  // If scrollable area has been disposed, we can not get the page scrollbar
  // theme setting. Should early return here.
  if (HasBeenDisposed())
    return;

  DCHECK(!GetPageScrollbarTheme().BlinkControlsOverlayVisibility());
  SetScrollbarsHiddenIfOverlayInternal(hidden);
}

void ScrollableArea::SetScrollbarsHiddenIfOverlay(bool hidden) {
  // If scrollable area has been disposed, we can not get the page scrollbar
  // theme setting. Should early return here.
  if (HasBeenDisposed())
    return;

  DCHECK(GetPageScrollbarTheme().BlinkControlsOverlayVisibility());
  SetScrollbarsHiddenIfOverlayInternal(hidden);
}

void ScrollableArea::SetScrollbarsHiddenIfOverlayInternal(bool hidden) {
  if (!GetPageScrollbarTheme().UsesOverlayScrollbars())
    return;

  if (scrollbars_hidden_if_overlay_ == static_cast<unsigned>(hidden))
    return;

  scrollbars_hidden_if_overlay_ = hidden;
  ScrollbarVisibilityChanged();
}

void ScrollableArea::FadeOverlayScrollbarsTimerFired(TimerBase*) {
  SetScrollbarsHiddenIfOverlay(true);
}

void ScrollableArea::ShowNonMacOverlayScrollbars() {
  if (!GetPageScrollbarTheme().UsesOverlayScrollbars() ||
      !GetPageScrollbarTheme().BlinkControlsOverlayVisibility())
    return;

  // Don't do this for composited scrollbars. These scrollbars are handled
  // by separate code in cc::ScrollbarAnimationController.
  if (UsesCompositedScrolling())
    return;

  SetScrollbarsHiddenIfOverlay(false);

  const base::TimeDelta time_until_disable =
      GetPageScrollbarTheme().OverlayScrollbarFadeOutDelay() +
      GetPageScrollbarTheme().OverlayScrollbarFadeOutDuration();

  // If the overlay scrollbars don't fade out, don't do anything. This is the
  // case for the mock overlays used in tests (and also Mac but its scrollbars
  // are animated by OS APIs and so we've already early-out'ed above).  We also
  // don't fade out overlay scrollbar for popup since we don't create
  // compositor for popup and thus they don't appear on hover so users without
  // a wheel can't scroll if they fade out.
  if (time_until_disable.is_zero() || GetChromeClient()->IsPopup())
    return;

  if (!fade_overlay_scrollbars_timer_) {
    fade_overlay_scrollbars_timer_ = MakeGarbageCollected<
        DisallowNewWrapper<HeapTaskRunnerTimer<ScrollableArea>>>(
        GetCompositorTaskRunner(), this,
        &ScrollableArea::FadeOverlayScrollbarsTimerFired);
  }

  if (!scrollbar_captured_ && !mouse_over_scrollbar_) {
    fade_overlay_scrollbars_timer_->Value().StartOneShot(time_until_disable,
                                                         FROM_HERE);
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
ScrollableArea::GetCompositorTaskRunner() {
  return compositor_task_runner_;
}

Node* ScrollableArea::EventTargetNode() const {
  const LayoutBox* box = GetLayoutBox();
  Node* node = box->GetNode();
  if (!node && box->Parent() && box->Parent()->IsFieldset()) {
    node = box->Parent()->GetNode();
  }
  if (node && IsA<Element>(node)) {
    const LayoutBox* layout_box_for_scrolling =
        To<Element>(node)->GetLayoutBoxForScrolling();
    if (layout_box_for_scrolling)
      DCHECK_EQ(box, layout_box_for_scrolling);
    else
      return nullptr;
  }
  return node;
}

const Document* ScrollableArea::GetDocument() const {
  if (auto* box = GetLayoutBox())
    return &box->GetDocument();
  return nullptr;
}

gfx::Vector2d ScrollableArea::ClampScrollOffset(
    const gfx::Vector2d& scroll_offset) const {
  gfx::Vector2d result = scroll_offset;
  result.SetToMin(MaximumScrollOffsetInt());
  result.SetToMax(MinimumScrollOffsetInt());
  return result;
}

ScrollOffset ScrollableArea::ClampScrollOffset(
    const ScrollOffset& scroll_offset) const {
  ScrollOffset result = scroll_offset;
  result.SetToMin(MaximumScrollOffset());
  result.SetToMax(MinimumScrollOffset());
  return result;
}

int ScrollableArea::LineStep(ScrollbarOrientation) const {
  return PixelsPerLineStep(GetLayoutBox()->GetFrame());
}

int ScrollableArea::PageStep(ScrollbarOrientation orientation) const {
  // Paging scroll operations should take scroll-padding into account [1]. So we
  // use the snapport rect to calculate the page step instead of the visible
  // rect.
  // [1] https://drafts.csswg.org/css-scroll-snap/#scroll-padding
  gfx::Size snapport_size =
      VisibleScrollSnapportRect(kExcludeScrollbars).PixelSnappedSize();
  int length = (orientation == kHorizontalScrollbar) ? snapport_size.width()
                                                     : snapport_size.height();
  int min_page_step =
      static_cast<float>(length) * MinFractionToStepWhenPaging();
  int page_step = std::max(min_page_step, length - MaxOverlapBetweenPages());

  return std::max(page_step, 1);
}

int ScrollableArea::DocumentStep(ScrollbarOrientation orientation) const {
  return ScrollSize(orientation);
}

float ScrollableArea::PixelStep(ScrollbarOrientation) const {
  return 1;
}

float ScrollableArea::PercentageStep(ScrollbarOrientation orientation) const {
  int percent_basis =
      (orientation == ScrollbarOrientation::kHorizontalScrollbar)
          ? VisibleWidth()
          : VisibleHeight();
  return static_cast<float>(percent_basis);
}

int ScrollableArea::VerticalScrollbarWidth(
    OverlayScrollbarClipBehavior behavior) const {
  DCHECK_EQ(behavior, kIgnoreOverlayScrollbarSize);
  if (Scrollbar* vertical_bar = VerticalScrollbar())
    return !vertical_bar->IsOverlayScrollbar() ? vertical_bar->Width() : 0;
  return 0;
}

int ScrollableArea::HorizontalScrollbarHeight(
    OverlayScrollbarClipBehavior behavior) const {
  DCHECK_EQ(behavior, kIgnoreOverlayScrollbarSize);
  if (Scrollbar* horizontal_bar = HorizontalScrollbar())
    return !horizontal_bar->IsOverlayScrollbar() ? horizontal_bar->Height() : 0;
  return 0;
}

gfx::QuadF ScrollableArea::LocalToVisibleContentQuad(const gfx::QuadF& quad,
                                                     const LayoutObject*,
                                                     unsigned) const {
  return quad - GetScrollOffset();
}

gfx::Size ScrollableArea::ExcludeScrollbars(const gfx::Size& size) const {
  return gfx::Size(std::max(0, size.width() - VerticalScrollbarWidth()),
                   std::max(0, size.height() - HorizontalScrollbarHeight()));
}

void ScrollableArea::DidCompositorScroll(const gfx::PointF& position) {
  ScrollOffset new_offset(ScrollPositionToOffset(position));
  SetScrollOffset(new_offset, mojom::blink::ScrollType::kCompositor);
}

Scrollbar* ScrollableArea::GetScrollbar(
    ScrollbarOrientation orientation) const {
  return orientation == kHorizontalScrollbar ? HorizontalScrollbar()
                                             : VerticalScrollbar();
}

CompositorElementId ScrollableArea::GetScrollbarElementId(
    ScrollbarOrientation orientation) {
  CompositorElementId scrollable_element_id = GetScrollElementId();
  DCHECK(scrollable_element_id);
  CompositorElementIdNamespace element_id_namespace =
      orientation == kHorizontalScrollbar
          ? CompositorElementIdNamespace::kHorizontalScrollbar
          : CompositorElementIdNamespace::kVerticalScrollbar;
  return CompositorElementIdWithNamespace(scrollable_element_id,
                                          element_id_namespace);
}

void ScrollableArea::OnScrollFinished(bool enqueue_scrollend) {
  if (GetLayoutBox()) {
    if (RuntimeEnabledFeatures::ScrollEndEventsEnabled()) {
      if (enqueue_scrollend) {
        if (Node* node = EventTargetNode()) {
          node->GetDocument().EnqueueScrollEndEventForNode(node);
        }
      }
    }
    GetLayoutBox()
        ->GetFrame()
        ->LocalFrameRoot()
        .GetEventHandler()
        .MarkHoverStateDirty();
  }
}

void ScrollableArea::SnapAfterScrollbarScrolling(
    ScrollbarOrientation orientation) {
  SnapAtCurrentPosition(orientation == kHorizontalScrollbar,
                        orientation == kVerticalScrollbar);
}

bool ScrollableArea::SnapAtCurrentPosition(
    bool scrolled_x,
    bool scrolled_y,
    base::ScopedClosureRunner on_finish) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  gfx::PointF current_position = ScrollPosition();
  return SnapForEndPosition(current_position, scrolled_x, scrolled_y,
                            std::move(on_finish));
}

bool ScrollableArea::SnapForEndPosition(const gfx::PointF& end_position,
                                        bool scrolled_x,
                                        bool scrolled_y,
                                        base::ScopedClosureRunner on_finish) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(end_position, scrolled_x,
                                                      scrolled_y);
  return PerformSnapping(*strategy, mojom::blink::ScrollBehavior::kSmooth,
                         std::move(on_finish));
}

bool ScrollableArea::SnapForDirection(const ScrollOffset& delta,
                                      base::ScopedClosureRunner on_finish) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  gfx::PointF current_position = ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForDirection(
          current_position, delta,
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
  return PerformSnapping(*strategy, mojom::blink::ScrollBehavior::kSmooth,
                         std::move(on_finish));
}

bool ScrollableArea::SnapForEndAndDirection(const ScrollOffset& delta) {
  DCHECK(IsRootFrameViewport() || !GetLayoutBox()->IsGlobalRootScroller());
  gfx::PointF current_position = ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndAndDirection(
          current_position, delta,
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
  return PerformSnapping(*strategy);
}

void ScrollableArea::SnapAfterLayout() {
  const cc::SnapContainerData* container_data = GetSnapContainerData();
  if (!container_data || !container_data->size())
    return;

  gfx::PointF current_position = ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForTargetElement(current_position);
  PerformSnapping(*strategy, mojom::blink::ScrollBehavior::kInstant);
}

bool ScrollableArea::PerformSnapping(
    const cc::SnapSelectionStrategy& strategy,
    mojom::blink::ScrollBehavior scroll_behavior,
    base::ScopedClosureRunner on_finish) {
  absl::optional<gfx::PointF> snap_point =
      GetSnapPositionAndSetTarget(strategy);
  if (!snap_point)
    return false;
  CancelScrollAnimation();
  CancelProgrammaticScrollAnimation();
  SetScrollOffset(
      ScrollPositionToOffset(snap_point.value()),
      mojom::blink::ScrollType::kProgrammatic, scroll_behavior,
      IgnoreArgs<ScrollableArea::ScrollCompletionMode>(on_finish.Release()));
  return true;
}

void ScrollableArea::Trace(Visitor* visitor) const {
  visitor->Trace(scroll_animator_);
  visitor->Trace(mac_scrollbar_animator_);
  visitor->Trace(programmatic_scroll_animator_);
  visitor->Trace(fade_overlay_scrollbars_timer_);
}

void ScrollableArea::InjectGestureScrollEvent(
    WebGestureDevice device,
    ScrollOffset delta,
    ui::ScrollGranularity granularity,
    WebInputEvent::Type gesture_type) const {
  // All ScrollableArea's have a layout box, except for the VisualViewport.
  // We shouldn't be injecting scrolls for the visual viewport scrollbar, since
  // it is not hit-testable.
  DCHECK(GetLayoutBox());

  // Speculative fix for crash reports (crbug.com/1307510).
  if (!GetLayoutBox() || !GetLayoutBox()->GetFrame())
    return;

  if (granularity == ui::ScrollGranularity::kScrollByPrecisePixel ||
      granularity == ui::ScrollGranularity::kScrollByPixel) {
    // Pixel-based deltas need to be scaled up by the input event scale factor,
    // since the GSUs will be scaled down by that factor when being handled.
    float scale = 1;
    LocalFrameView* root_view =
        GetLayoutBox()->GetFrame()->LocalFrameRoot().View();
    if (root_view)
      scale = root_view->InputEventsScaleFactor();
    delta.Scale(scale);
  }

  GetChromeClient()->InjectGestureScrollEvent(
      *GetLayoutBox()->GetFrame(), device, delta, granularity,
      GetScrollElementId(), gesture_type);
}

ScrollableArea* ScrollableArea::GetForScrolling(const LayoutBox* layout_box) {
  if (!layout_box)
    return nullptr;

  if (!layout_box->IsGlobalRootScroller()) {
    if (const auto* element = DynamicTo<Element>(layout_box->GetNode())) {
      if (auto* scrolling_box = element->GetLayoutBoxForScrolling())
        return scrolling_box->GetScrollableArea();
    }
    return layout_box->GetScrollableArea();
  }

  // The global root scroller should be scrolled by the root frame view's
  // ScrollableArea.
  LocalFrame& root_frame = layout_box->GetFrame()->LocalFrameRoot();
  return root_frame.View()->GetScrollableArea();
}

float ScrollableArea::ScaleFromDIP() const {
  auto* client = GetChromeClient();
  auto* document = GetDocument();
  if (client && document)
    return client->WindowToViewportScalar(document->GetFrame(), 1.0f);
  return 1.0f;
}

}  // namespace blink
