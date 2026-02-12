// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
