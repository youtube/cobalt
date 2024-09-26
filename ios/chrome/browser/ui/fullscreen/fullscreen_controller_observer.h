// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_OBSERVER_H_

#import <UIKit/UIKit.h>

class FullscreenController;
@class FullscreenAnimator;

// Interface for listening to fullscreen state.
class FullscreenControllerObserver {
 public:
  FullscreenControllerObserver() = default;

  FullscreenControllerObserver(const FullscreenControllerObserver&) = delete;
  FullscreenControllerObserver& operator=(const FullscreenControllerObserver&) =
      delete;

  virtual ~FullscreenControllerObserver() = default;

  // Invoked when the maximum or minimum viewport insets for `controller` have
  // been updated.
  virtual void FullscreenViewportInsetRangeChanged(
      FullscreenController* controller,
      UIEdgeInsets min_viewport_insets,
      UIEdgeInsets max_viewport_insets) {}

  // Invoked after a scrolling event has caused `controller` to calculate
  // `progress`.  A `progress` value of 1.0 denotes that the toolbar should be
  // completely visible, while a `progress` value of 0.0 denotes that the
  // toolbar should be competely hidden.
  virtual void FullscreenProgressUpdated(FullscreenController* controller,
                                         CGFloat progress) {}

  // Invoked with `controller` is enabled or disabled.
  virtual void FullscreenEnabledStateChanged(FullscreenController* controller,
                                             bool enabled) {}

  // Invoked when `controller` is about to start an animation with `animator`.
  // Observers are expected to add animations to update UI for `animator`'s
  // final progress.
  virtual void FullscreenWillAnimate(FullscreenController* controller,
                                     FullscreenAnimator* animator) {}

  // Invoked before the FullscreenController service is shut down.
  // TODO(crbug.com/1046022): Rename to FullscreenControllerDestroyed.
  virtual void FullscreenControllerWillShutDown(
      FullscreenController* controller) {}

  // Invoked when `controller` needs to resize its horizontal insets.
  // TODO(crbug.com/1114054) remove after fixing multiwindow resizing issue.
  virtual void ResizeHorizontalInsets(FullscreenController* controller) {}
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_OBSERVER_H_
