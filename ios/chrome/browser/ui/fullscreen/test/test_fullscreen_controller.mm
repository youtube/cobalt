// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestFullscreenController::TestFullscreenController(FullscreenModel* model)
    : FullscreenController(),
      model_(model),
      broadcaster_([[ChromeBroadcaster alloc] init]) {}

TestFullscreenController::~TestFullscreenController() {
  for (auto& observer : observers_) {
    observer.FullscreenControllerWillShutDown(this);
  }
}

ChromeBroadcaster* TestFullscreenController::broadcaster() {
  return broadcaster_;
}

void TestFullscreenController::SetWebStateList(WebStateList* web_state_list) {
  web_state_list_ = web_state_list;
}

const WebStateList* TestFullscreenController::GetWebStateList() const {
  return web_state_list_;
}

WebStateList* TestFullscreenController::GetWebStateList() {
  return web_state_list_;
}

void TestFullscreenController::AddObserver(
    FullscreenControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void TestFullscreenController::RemoveObserver(
    FullscreenControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TestFullscreenController::IsEnabled() const {
  return model_ && model_->enabled();
}

void TestFullscreenController::IncrementDisabledCounter() {
  if (model_)
    model_->IncrementDisabledCounter();
}

void TestFullscreenController::DecrementDisabledCounter() {
  if (model_)
    model_->DecrementDisabledCounter();
}

bool TestFullscreenController::ResizesScrollView() const {
  return model_->ResizesScrollView();
}

void TestFullscreenController::BrowserTraitCollectionChangedBegin() {}

void TestFullscreenController::BrowserTraitCollectionChangedEnd() {}

CGFloat TestFullscreenController::GetProgress() const {
  return model_ ? model_->progress() : 0.0;
}

UIEdgeInsets TestFullscreenController::GetMinViewportInsets() const {
  return model_ ? model_->min_toolbar_insets() : UIEdgeInsetsZero;
}

UIEdgeInsets TestFullscreenController::GetMaxViewportInsets() const {
  return model_ ? model_->max_toolbar_insets() : UIEdgeInsetsZero;
}

UIEdgeInsets TestFullscreenController::GetCurrentViewportInsets() const {
  return model_ ? model_->current_toolbar_insets() : UIEdgeInsetsZero;
}

void TestFullscreenController::EnterFullscreen() {
  if (model_)
    model_->AnimationEndedWithProgress(0.0);
}

void TestFullscreenController::ExitFullscreen() {
  if (model_)
    model_->ResetForNavigation();
}

void TestFullscreenController::OnFullscreenViewportInsetRangeChanged(
    UIEdgeInsets min_viewport_insets,
    UIEdgeInsets max_viewport_insets) {
  for (auto& observer : observers_) {
    observer.FullscreenViewportInsetRangeChanged(this, min_viewport_insets,
                                                 max_viewport_insets);
  }
}

void TestFullscreenController::OnFullscreenProgressUpdated(CGFloat progress) {
  for (auto& observer : observers_) {
    observer.FullscreenProgressUpdated(this, progress);
  }
}

void TestFullscreenController::OnFullscreenEnabledStateChanged(bool enabled) {
  for (auto& observer : observers_) {
    observer.FullscreenEnabledStateChanged(this, enabled);
  }
}

void TestFullscreenController::OnFullscreenWillAnimate(
    FullscreenAnimator* animator) {
  for (auto& observer : observers_) {
    observer.FullscreenWillAnimate(this, animator);
  }
}

void TestFullscreenController::ResizeHorizontalViewport() {
  // NOOP in tests.
}

void TestFullscreenController::FreezeToolbarHeight(bool freeze_toolbar_height) {
  if (model_) {
    model_->SetFreezeToolbarHeight(freeze_toolbar_height);
  }
}

// static
const void* TestFullscreenController::UserDataKeyForTesting() {
  return FullscreenController::UserDataKey();
}
