// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
