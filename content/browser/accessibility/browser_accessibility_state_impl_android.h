// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_ANDROID_H_

#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "ui/accessibility/android/accessibility_state.h"

namespace content {

class BrowserContext;

class BrowserAccessibilityStateImplAndroid
    : public BrowserAccessibilityStateImpl,
      public ui::AccessibilityState::Delegate {
 public:
  BrowserAccessibilityStateImplAndroid();
  ~BrowserAccessibilityStateImplAndroid() override;

  void CollectAccessibilityServiceStats();
  void RecordAccessibilityServiceStatsHistogram(int event_type_mask,
                                                int feedback_type_mask,
                                                int flags_mask,
                                                int capabilities_mask,
                                                std::string histogram);

  // ui::AccessibilityState::Delegate overrides
  void OnAnimatorDurationScaleChanged() override;

 protected:
  void UpdateHistogramsOnOtherThread() override;
  void UpdateUniqueUserHistograms() override;
  void SetImageLabelsModeForProfile(bool enabled,
                                    BrowserContext* profile) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_ANDROID_H_
