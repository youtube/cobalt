// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_TEST_DISPLAY_LAYOUT_MANAGER_H_
#define UI_DISPLAY_MANAGER_TEST_TEST_DISPLAY_LAYOUT_MANAGER_H_

#include <memory>
#include <vector>

#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_layout_manager.h"

namespace display::test {

class TestDisplayLayoutManager : public DisplayLayoutManager {
 public:
  TestDisplayLayoutManager(
      std::vector<std::unique_ptr<DisplaySnapshot>> displays,
      MultipleDisplayState display_state);

  TestDisplayLayoutManager(const TestDisplayLayoutManager&) = delete;
  TestDisplayLayoutManager& operator=(const TestDisplayLayoutManager&) = delete;

  ~TestDisplayLayoutManager() override;

  void set_displays(std::vector<std::unique_ptr<DisplaySnapshot>> displays) {
    displays_ = std::move(displays);
  }

  void set_display_state(MultipleDisplayState display_state) {
    display_state_ = display_state;
  }

  // DisplayLayoutManager:
  DisplayConfigurator::StateController* GetStateController() const override;
  DisplayConfigurator::SoftwareMirroringController*
  GetSoftwareMirroringController() const override;
  MultipleDisplayState GetDisplayState() const override;
  chromeos::DisplayPowerState GetPowerState() const override;
  bool GetDisplayLayout(
      const std::vector<DisplaySnapshot*>& displays,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state,
      RefreshRateThrottleState new_throttle_state,
      bool new_vrr_enabled_state,
      std::vector<DisplayConfigureRequest>* requests) const override;
  std::vector<DisplaySnapshot*> GetDisplayStates() const override;
  bool IsMirroring() const override;

 private:
  std::vector<std::unique_ptr<DisplaySnapshot>> displays_;
  MultipleDisplayState display_state_;
};

}  // namespace display::test

#endif  // UI_DISPLAY_MANAGER_TEST_TEST_DISPLAY_LAYOUT_MANAGER_H_
