// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_LIST_ITEM_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_LIST_ITEM_H_

#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_view.h"

namespace arc::input_overlay {

// ActionViewListItem shows in EditingList and is associated with each of
// Action.
// ----------------------------
// | |Name tag|        |keys| |
// ----------------------------

class ActionViewListItem : public ActionEditView {
 public:
  ActionViewListItem(DisplayOverlayController* controller, Action* action);
  ActionViewListItem(const ActionViewListItem&) = delete;
  ActionViewListItem& operator=(const ActionViewListItem&) = delete;
  ~ActionViewListItem() override;

  // ActionEditView:
  void OnActionNameUpdated() override;

  void ShowEduNudgeForEditingTip();

 private:
  friend class EditLabelTest;

  // ActionEditView:
  void ClickCallback() override;

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_LIST_ITEM_H_
