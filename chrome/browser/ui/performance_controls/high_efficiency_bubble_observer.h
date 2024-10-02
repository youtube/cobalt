// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_BUBBLE_OBSERVER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_BUBBLE_OBSERVER_H_

// This observer interface for the high efficiency bubble dialog.
class HighEfficiencyBubbleObserver {
 public:
  // Called when the high efficiency dialog is opened.
  virtual void OnBubbleShown() = 0;
  // Called when the high efficiency dialog is closed.
  virtual void OnBubbleHidden() = 0;

 protected:
  virtual ~HighEfficiencyBubbleObserver() = default;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_BUBBLE_OBSERVER_H_
