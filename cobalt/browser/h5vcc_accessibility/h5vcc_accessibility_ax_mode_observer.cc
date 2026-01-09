// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_ax_mode_observer.h"

#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_manager.h"
#include "ui/accessibility/platform/assistive_tech.h"
#include "ui/accessibility/platform/ax_platform.h"

namespace h5vcc_accessibility {

H5vccAccessibilityAXModeObserver::H5vccAccessibilityAXModeObserver() {
  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
}

H5vccAccessibilityAXModeObserver::~H5vccAccessibilityAXModeObserver() = default;

// static
void H5vccAccessibilityAXModeObserver::CreateSingleton() {
  static const base::NoDestructor<H5vccAccessibilityAXModeObserver> instance;
}

void H5vccAccessibilityAXModeObserver::OnAssistiveTechChanged(
    ui::AssistiveTech assistive_tech) {
  cobalt::browser::H5vccAccessibilityManager::GetInstance()
      ->OnTextToSpeechStateChanged(ui::IsScreenReader(assistive_tech));
}

}  // namespace h5vcc_accessibility
