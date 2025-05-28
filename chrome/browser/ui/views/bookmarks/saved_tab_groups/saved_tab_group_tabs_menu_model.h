// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_TABS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_TABS_MENU_MODEL_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

class Browser;

namespace favicon_base {
struct FaviconImageResult;
}

namespace tab_groups {

// Provides the menu model for the saved tab group tabs sub menu / context menu
class STGTabsMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDeleteGroupMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLeaveGroupMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMoveGroupToNewWindowMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOpenGroup);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToggleGroupPinStateMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabsTitleItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTab);

  explicit STGTabsMenuModel(Browser* browser);
  STGTabsMenuModel(ui::SimpleMenuModel::Delegate* delegate, Browser* browser);

  STGTabsMenuModel(const STGTabsMenuModel&) = delete;
  STGTabsMenuModel& operator=(const STGTabsMenuModel&) = delete;

  ~STGTabsMenuModel() override;

  void Build(const SavedTabGroup& saved_group,
             base::RepeatingCallback<int()> get_next_command_id);

  bool HasCommandId(int command_id) const;

  // override ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // The action users can perform on a saved tab group's submenu.
  struct Action {
    enum class Type {
      DEFAULT = -1,
      OPEN_IN_BROWSER,
      OPEN_OR_MOVE_TO_NEW_WINDOW,
      PIN_OR_UNPIN_GROUP,
      DELETE_GROUP,
      LEAVE_GROUP,
      OPEN_URL,
    };

    Action(Type type, std::variant<base::Uuid, GURL> element);
    Action(const Action&);
    ~Action();

    Type type = Type::DEFAULT;

    // The action needs either a Uuid (e.g. Open group in new window) or a Url
    // (e.g. Open tab) to perform.
    std::variant<base::Uuid, GURL> element;
  };

  void OnFaviconDataAvailable(
      int command_id,
      const favicon_base::FaviconImageResult& image_result);

  raw_ptr<Browser> browser_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  bool should_enable_move_menu_item_;

  // The key is a submenu command id, i.e. one of the following:
  //        Open Group
  //        Open/Move to New Window
  //        Pin/Unpin Group
  //        Delete Group
  //        Tab
  // App menu is the delegate to be notified when a submenu item is invoked, it
  // will then delegate to `this` to execute. See `AppMenu::ExecuteCommand`.
  // `this` only gets a command id from AppMenu, so `this` needs to know which
  // action should be performed. That is why this map exists.
  std::map<int, Action> command_id_to_action_;

  base::WeakPtrFactory<STGTabsMenuModel> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_TABS_MENU_MODEL_H_
