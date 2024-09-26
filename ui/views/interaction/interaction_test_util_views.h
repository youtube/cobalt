// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_VIEWS_H_
#define UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_VIEWS_H_

#include <string>
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"

namespace ui {
class TrackedElement;
}

namespace views {
class Button;
class View;
}

namespace views::test {

// Views implementation of InteractionTestUtil::Simulator.
// Add one to your InteractionTestUtil instance to get Views support.
class InteractionTestUtilSimulatorViews
    : public ui::test::InteractionTestUtil::Simulator {
 public:
  InteractionTestUtilSimulatorViews();
  ~InteractionTestUtilSimulatorViews() override;

  // ui::test::InteractionTestUtil::Simulator:
  ui::test::ActionResult PressButton(ui::TrackedElement* element,
                                     InputType input_type) override;
  ui::test::ActionResult SelectMenuItem(ui::TrackedElement* element,
                                        InputType input_type) override;
  ui::test::ActionResult DoDefaultAction(ui::TrackedElement* element,
                                         InputType input_type) override;
  ui::test::ActionResult SelectTab(ui::TrackedElement* tab_collection,
                                   size_t index,
                                   InputType input_type) override;
  ui::test::ActionResult SelectDropdownItem(ui::TrackedElement* dropdown,
                                            size_t index,
                                            InputType input_type) override;
  ui::test::ActionResult EnterText(ui::TrackedElement* element,
                                   std::u16string text,
                                   TextEntryMode mode) override;
  ui::test::ActionResult ActivateSurface(ui::TrackedElement* element) override;
  ui::test::ActionResult SendAccelerator(ui::TrackedElement* element,
                                         ui::Accelerator accelerator) override;
  ui::test::ActionResult Confirm(ui::TrackedElement* element) override;

  // Convenience method for tests that need to simulate a button press and have
  // direct access to the button.
  static void PressButton(Button* button,
                          InputType input_type = InputType::kDontCare);

  // As above, but for non-button Views.
  static bool DoDefaultAction(View* view,
                              InputType input_type = InputType::kDontCare);
};

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_VIEWS_H_
