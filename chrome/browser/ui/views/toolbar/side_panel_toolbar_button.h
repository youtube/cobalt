// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_SIDE_PANEL_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_SIDE_PANEL_TOOLBAR_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/prefs/pref_change_registrar.h"

class Browser;

class SidePanelToolbarButton : public ToolbarButton {
 public:
  explicit SidePanelToolbarButton(Browser* browser);
  SidePanelToolbarButton(const SidePanelToolbarButton&) = delete;
  SidePanelToolbarButton& operator=(const SidePanelToolbarButton&) = delete;
  ~SidePanelToolbarButton() override;

  // ToolbarButton
  bool ShouldShowInkdropAfterIphInteraction() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SidePanelToolbarButtonTest, SetCorrectIconInLTR);
  FRIEND_TEST_ALL_PREFIXES(SidePanelToolbarButtonTest, SetCorrectIconInRTL);

  void ButtonPressed();

  // Updates the vector icon used when the PrefChangeRegistrar listens to a
  // change. When the side panel should open to the right side of the browser
  // the default vector icon is used. When the side panel should open to the
  // left side of the browser the flipped vector icon is used.
  void UpdateToolbarButtonIcon();

  const raw_ptr<Browser> browser_;

  // Observes and listens to side panel alignment changes.
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_SIDE_PANEL_TOOLBAR_BUTTON_H_
