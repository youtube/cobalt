// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_PICKER_VIEW_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_PICKER_VIEW_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"

class Browser;

// TODO(musalmaan): Move this file, along with other intent picker
// and tab helper files, to a dedicated directory in a separate CL.

/**
 * IntentPickerViewPageActionController manages the page action associated with
 * the intent picker. It is responsible for creating and updating the
 * intent picker and it ensures that the intent picker icon is displayed
 * correctly based on the current state of the active web contents and user's
 * browsing context.
 */
class IntentPickerViewPageActionController {
 public:
  explicit IntentPickerViewPageActionController(
      tabs::TabInterface* tab_interface);

  ~IntentPickerViewPageActionController() = default;

  // Disallow copies
  IntentPickerViewPageActionController(
      const IntentPickerViewPageActionController&) = delete;
  IntentPickerViewPageActionController& operator=(
      const IntentPickerViewPageActionController&) = delete;

  // Updates the visibility of the Intent Picker icon and suggestion chip.
  // If should_show_icon is true, the icon and chip are shown.
  // Otherwise, they are hidden by calling HideIcon().
  void UpdatePageActionVisibility(bool should_show_icon);

 private:
  // Hides the Intent Picker page action icon and closes any associated bubble.
  void HideIcon();

  raw_ref<tabs::TabInterface> tab_interface_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_PICKER_VIEW_PAGE_ACTION_CONTROLLER_H_
