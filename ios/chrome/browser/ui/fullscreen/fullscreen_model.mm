// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"

#import <algorithm>

#import "base/check_op.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model_observer.h"
#import "ios/chrome/common/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Object that increments `counter` by 1 for its lifetime.
class ScopedIncrementer {
 public:
  explicit ScopedIncrementer(size_t* counter) : counter_(counter) {
    ++(*counter_);
  }
  ~ScopedIncrementer() { --(*counter_); }

 private:
  size_t* counter_;
};
}

FullscreenModel::FullscreenModel() = default;
FullscreenModel::~FullscreenModel() = default;

void FullscreenModel::IncrementDisabledCounter() {
  if (++disabled_counter_ == 1U) {
    ScopedIncrementer disabled_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelEnabledStateChanged(this);
    }
    // Fullscreen observers are expected to show the toolbar when fullscreen is
    // disabled. Update the internal state to match this.
    SetProgress(1.0);
    UpdateBaseOffset();
  }
}

void FullscreenModel::DecrementDisabledCounter() {
  DCHECK_GT(disabled_counter_, 0U);
  if (!--disabled_counter_) {
    ScopedIncrementer enabled_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelEnabledStateChanged(this);
    }
  }
}

void FullscreenModel::ResetForNavigation() {
  progress_ = 1.0;
  scrolling_ = false;
  base_offset_ = NAN;
  ScopedIncrementer reset_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelWasReset(this);
  }
}

void FullscreenModel::IgnoreRemainderOfCurrentScroll() {
  if (!scrolling_)
    return;
  ignoring_current_scroll_ = true;
}

void FullscreenModel::AnimationEndedWithProgress(CGFloat progress) {
  DCHECK_GE(progress, 0.0);
  DCHECK_LE(progress, 1.0);
  // Since this is being set by the animator instead of by scroll events, do not
  // broadcast the new progress value.
  progress_ = progress;
}

void FullscreenModel::SetCollapsedToolbarHeight(CGFloat height) {
  if (AreCGFloatsEqual(GetCollapsedToolbarHeight(), height))
    return;
  DCHECK_GE(height, 0.0);
  collapsed_toolbar_height_ = height;
  base_offset_ = NAN;
  ScopedIncrementer toolbar_height_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelToolbarHeightsUpdated(this);
  }
}

CGFloat FullscreenModel::GetCollapsedToolbarHeight() const {
  return GetFreezeToolbarHeight() ? 0 : collapsed_toolbar_height_;
}

void FullscreenModel::SetExpandedToolbarHeight(CGFloat height) {
  if (AreCGFloatsEqual(GetExpandedToolbarHeight(), height))
    return;
  DCHECK_GE(height, 0.0);
  expanded_toolbar_height_ = height;
  base_offset_ = NAN;
  ScopedIncrementer toolbar_height_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelToolbarHeightsUpdated(this);
  }
}

CGFloat FullscreenModel::GetExpandedToolbarHeight() const {
  return GetFreezeToolbarHeight() ? 0 : expanded_toolbar_height_;
}

void FullscreenModel::SetBottomToolbarHeight(CGFloat height) {
  if (AreCGFloatsEqual(bottom_toolbar_height_, height))
    return;
  DCHECK_GE(height, 0.0);
  bottom_toolbar_height_ = height;
  base_offset_ = NAN;
  ScopedIncrementer toolbar_height_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelToolbarHeightsUpdated(this);
  }
}

CGFloat FullscreenModel::GetBottomToolbarHeight() const {
  return bottom_toolbar_height_;
}

void FullscreenModel::SetScrollViewHeight(CGFloat scroll_view_height) {
  scroll_view_height_ = scroll_view_height;
  UpdateDisabledCounterForContentHeight();
}

CGFloat FullscreenModel::GetScrollViewHeight() const {
  return scroll_view_height_;
}

void FullscreenModel::SetContentHeight(CGFloat content_height) {
  content_height_ = content_height;
  UpdateDisabledCounterForContentHeight();
}

CGFloat FullscreenModel::GetContentHeight() const {
  return content_height_;
}

void FullscreenModel::SetTopContentInset(CGFloat top_inset) {
  top_inset_ = top_inset;
}

CGFloat FullscreenModel::GetTopContentInset() const {
  return top_inset_;
}

void FullscreenModel::SetYContentOffset(CGFloat y_content_offset) {
  CGFloat from_offset = y_content_offset_;
  y_content_offset_ = y_content_offset;
  switch (ActionForScrollFromOffset(from_offset)) {
    case ScrollAction::kUpdateBaseOffset:
      UpdateBaseOffset();
      break;
    case ScrollAction::kUpdateProgress:
      UpdateProgress();
      break;
    case ScrollAction::kUpdateBaseOffsetAndProgress:
      UpdateBaseOffset();
      UpdateProgress();
      break;
    case ScrollAction::kIgnore:
      // no op.
      break;
  }
}

CGFloat FullscreenModel::GetYContentOffset() const {
  return y_content_offset_;
}

void FullscreenModel::SetScrollViewIsScrolling(bool scrolling) {
  if (scrolling_ == scrolling)
    return;
  scrolling_ = scrolling;
  if (scrolling_) {
    // Notify observers that the scroll event has begun.
    ScopedIncrementer scroll_started_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelScrollEventStarted(this);
    }
  } else {
    // Stop ignoring the current scroll.
    ignoring_current_scroll_ = false;
    // Notify observers that the scroll event has ended.
    ScopedIncrementer scroll_ended_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelScrollEventEnded(this);
    }
  }
}

bool FullscreenModel::IsScrollViewScrolling() const {
  return scrolling_;
}

void FullscreenModel::SetScrollViewIsZooming(bool zooming) {
  zooming_ = zooming;
}

bool FullscreenModel::IsScrollViewZooming() const {
  return zooming_;
}

void FullscreenModel::SetScrollViewIsDragging(bool dragging) {
  if (dragging_ == dragging)
    return;
  dragging_ = dragging;
  if (dragging_) {
    // Update the base offset for each new scroll event.
    UpdateBaseOffset();
    // Re-rendering events are ignored during scrolls since disabling the model
    // mid-scroll leads to choppy animations.  If the content was re-rendered
    // to be too short to collapse the toolbars, the model should be disabled
    // to prevent the subsequent scroll.
    UpdateDisabledCounterForContentHeight();
  }
}

bool FullscreenModel::IsScrollViewDragging() const {
  return dragging_;
}

void FullscreenModel::SetResizesScrollView(bool resizes_scroll_view) {
  resizes_scroll_view_ = resizes_scroll_view;
}

bool FullscreenModel::ResizesScrollView() const {
  return resizes_scroll_view_;
}

void FullscreenModel::SetWebViewSafeAreaInsets(UIEdgeInsets safe_area_insets) {
  if (UIEdgeInsetsEqualToEdgeInsets(safe_area_insets_, safe_area_insets))
    return;
  safe_area_insets_ = safe_area_insets;
  UpdateDisabledCounterForContentHeight();
}

UIEdgeInsets FullscreenModel::GetWebViewSafeAreaInsets() const {
  return safe_area_insets_;
}

void FullscreenModel::SetFreezeToolbarHeight(bool freeze_toolbar_height) {
  if (freeze_toolbar_height_ == freeze_toolbar_height) {
    return;
  }
  freeze_toolbar_height_ = freeze_toolbar_height;
  base_offset_ = NAN;
  ScopedIncrementer toolbar_height_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelToolbarHeightsUpdated(this);
  }
}

bool FullscreenModel::GetFreezeToolbarHeight() const {
  return freeze_toolbar_height_;
}

FullscreenModel::ScrollAction FullscreenModel::ActionForScrollFromOffset(
    CGFloat from_offset) const {
  // Update the base offset but don't recalculate progress if:
  // - the model is disabled,
  // - the scroll is not triggered by a user action,
  // - the sroll view is zooming,
  // - the scroll is triggered from a FullscreenModelObserver callback,
  // - there is no toolbar,
  // - the scroll offset doesn't change.
  if (!enabled() || !scrolling_ || zooming_ || observer_callback_count_ ||
      AreCGFloatsEqual(toolbar_height_delta(), 0.0) ||
      AreCGFloatsEqual(y_content_offset_, from_offset)) {
    return ScrollAction::kUpdateBaseOffset;
  }

  // Ignore if:
  // - explicitly requested via IgnoreRemainderOfCurrentScroll(),
  // - the scroll is a bounce-up animation at the top,
  // - the scroll is attempting to scroll content up when it already fits,
  // - the scroll is attempting to scroll past the bottom of the content when
  //   the scroll view is being resized (the rebound scroll animation
  //   interferes with the frame resizing).
  bool scrolling_content_down = y_content_offset_ - from_offset < 0.0;
  bool scrolling_past_top = y_content_offset_ <= -top_inset_;
  bool content_fits = content_height_ <= scroll_view_height_ - top_inset_;
  bool scrolling_past_bottom =
      y_content_offset_ + scroll_view_height_ + top_inset_ >= content_height_;
  if (ignoring_current_scroll_ ||
      (scrolling_past_top && !scrolling_content_down) ||
      (content_fits && !scrolling_content_down) ||
      (resizes_scroll_view_ && scrolling_past_bottom)) {
    return ScrollAction::kIgnore;
  }

  // All other scrolls should result in an updated progress value.  If the model
  // doesn't have a base offset, it should also be updated.
  return has_base_offset() ? ScrollAction::kUpdateProgress
                           : ScrollAction::kUpdateBaseOffsetAndProgress;
}

void FullscreenModel::UpdateBaseOffset() {
  base_offset_ = y_content_offset_ - (1.0 - progress_) * toolbar_height_delta();
}

void FullscreenModel::UpdateProgress() {
  CGFloat delta = base_offset_ - y_content_offset_;
  SetProgress(1.0 + delta / toolbar_height_delta());
}

void FullscreenModel::UpdateDisabledCounterForContentHeight() {
  // Sometimes the content size and scroll view sizes are updated mid-scroll
  // such that the scroll view height is updated before the content is re-
  // rendered, causing the model to be disabled.  These changes should be
  // ignored while the content is scrolling.
  if (scrolling_)
    return;
  // The model should be disabled when the content fits.
  CGFloat disabling_threshold = scroll_view_height_;
  if (resizes_scroll_view_) {
    // When Smooth Scrolling is disabled, the scroll view can sometimes be
    // resized to account for the viewport insets after the page has been
    // rendered, so account for the maximum toolbar insets in the threshold.
    disabling_threshold += GetExpandedToolbarHeight() + bottom_toolbar_height_;
  } else {
    // After reloads, pages whose viewports fit the screen are sometimes resized
    // to account for the safe area insets.  Adding these to the threshold helps
    // prevent fullscreen from beeing re-enabled in this case.
    // TODO(crbug.com/924807): This logic can potentially disable fullscreen for
    // short pages in which this bug does not occur.  It should be removed once
    // the page can be reloaded without resizing.
    disabling_threshold += safe_area_insets_.top + safe_area_insets_.bottom;
  }

  // Don't disable fullscreen if both heights have not been received.
  bool areBothHeightsSet = !AreCGFloatsEqual(content_height_, 0.0) &&
                           !AreCGFloatsEqual(scroll_view_height_, 0.0);

  bool disable = areBothHeightsSet && content_height_ <= disabling_threshold;
  if (disabled_for_short_content_ == disable)
    return;
  disabled_for_short_content_ = disable;
  if (disable)
    IncrementDisabledCounter();
  else
    DecrementDisabledCounter();
}

void FullscreenModel::SetProgress(CGFloat progress) {
  progress = std::min(static_cast<CGFloat>(1.0), progress);
  progress = std::max(static_cast<CGFloat>(0.0), progress);
  if (AreCGFloatsEqual(progress_, progress))
    return;
  progress_ = progress;

  ScopedIncrementer progress_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelProgressUpdated(this);
  }
}

void FullscreenModel::OnScrollViewSizeBroadcasted(CGSize scroll_view_size) {
  SetScrollViewHeight(scroll_view_size.height);
}

void FullscreenModel::OnScrollViewContentSizeBroadcasted(CGSize content_size) {
  SetContentHeight(content_size.height);
}

void FullscreenModel::OnScrollViewContentInsetBroadcasted(
    UIEdgeInsets content_inset) {
  SetTopContentInset(content_inset.top);
}

void FullscreenModel::OnContentScrollOffsetBroadcasted(CGFloat offset) {
  SetYContentOffset(offset);
}

void FullscreenModel::OnScrollViewIsScrollingBroadcasted(bool scrolling) {
  SetScrollViewIsScrolling(scrolling);
}

void FullscreenModel::OnScrollViewIsZoomingBroadcasted(bool zooming) {
  SetScrollViewIsZooming(zooming);
}

void FullscreenModel::OnScrollViewIsDraggingBroadcasted(bool dragging) {
  SetScrollViewIsDragging(dragging);
}

void FullscreenModel::OnCollapsedToolbarHeightBroadcasted(CGFloat height) {
  SetCollapsedToolbarHeight(height);
}

void FullscreenModel::OnExpandedToolbarHeightBroadcasted(CGFloat height) {
  SetExpandedToolbarHeight(height);
}

void FullscreenModel::OnBottomToolbarHeightBroadcasted(CGFloat height) {
  SetBottomToolbarHeight(height);
}
