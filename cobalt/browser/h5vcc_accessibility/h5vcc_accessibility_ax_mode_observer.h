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

#ifndef COBALT_BROWSER_H5VCC_ACCESSIBILITY_H5VCC_ACCESSIBILITY_AX_MODE_OBSERVER_H_
#define COBALT_BROWSER_H5VCC_ACCESSIBILITY_H5VCC_ACCESSIBILITY_AX_MODE_OBSERVER_H_

#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "ui/accessibility/platform/ax_mode_observer.h"

namespace ui {

class AXPlatform;
enum class AssistiveTech;

}  // namespace ui

namespace h5vcc_accessibility {

// This class implements TTS status monitoring and reporting by integrating
// with the //ui/accessibility code in Chromium. It is used by the tvOS port.
class H5vccAccessibilityAXModeObserver final : public ui::AXModeObserver {
 public:
  static void CreateSingleton();

  H5vccAccessibilityAXModeObserver(const H5vccAccessibilityAXModeObserver&) =
      delete;
  H5vccAccessibilityAXModeObserver& operator=(
      const H5vccAccessibilityAXModeObserver&) = delete;

  // ui::AXModeObserver overrides.
  void OnAssistiveTechChanged(ui::AssistiveTech assistive_tech) override;

 private:
  friend class base::NoDestructor<H5vccAccessibilityAXModeObserver>;

  H5vccAccessibilityAXModeObserver();
  ~H5vccAccessibilityAXModeObserver() override;

  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      ax_mode_observation_{this};
};

}  // namespace h5vcc_accessibility

#endif  // COBALT_BROWSER_H5VCC_ACCESSIBILITY_H5VCC_ACCESSIBILITY_AX_MODE_OBSERVER_H_
