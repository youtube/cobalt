// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/menus/simple_menu_model.h"

class Browser;
class Profile;

class BookmarkMergedSurfaceService;
struct BookmarkParentFolder;

// An interface implemented by an object that performs actions on the actual
// menu for the controller.
class BookmarkContextMenuControllerDelegate {
 public:
  virtual ~BookmarkContextMenuControllerDelegate() = default;

  // Closes the bookmark context menu.
  virtual void CloseMenu() = 0;

  // Sent before any command from the menu is executed.
  virtual void WillExecuteCommand(
      int command_id,
      const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                VectorExperimental>>& bookmarks) {}

  // Sent after any command from the menu is executed.
  virtual void DidExecuteCommand(int command_id) {}
};

// BookmarkContextMenuController creates and manages state for the context menu
// shown for any bookmark item.
class BookmarkContextMenuController
    : public bookmarks::BaseBookmarkModelObserver,
      public ui::SimpleMenuModel::Delegate {
 public:
  // Creates the bookmark context menu.
  // `browser` is used to open the bookmark manager and is null in tests.
  // `profile` is used for opening urls as well as enabling 'open incognito'.
  // Uses a callback since this can be asynchronous. See crbug.com/1161144.
  // `selection` is the nodes the context menu operates on and must be not
  // empty. The parent for newly created nodes is `selection[0]` if `selection`
  // has one element and it is a folder, otherwise it is `selection[0]->parent`.
  BookmarkContextMenuController(
      gfx::NativeWindow parent_window,
      BookmarkContextMenuControllerDelegate* delegate,
      Browser* browser,
      Profile* profile,
      BookmarkLaunchLocation opened_from,
      const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                VectorExperimental>>& selection);

  BookmarkContextMenuController(const BookmarkContextMenuController&) = delete;
  BookmarkContextMenuController& operator=(
      const BookmarkContextMenuController&) = delete;

  ~BookmarkContextMenuController() override;

  ui::SimpleMenuModel* menu_model() { return menu_model_.get(); }

  // Public for testing.
  // Returns the parent for newly created folders/bookmarks. If `selection` has
  // one element and it is a folder, `selection[0]` is returned, otherwise
  // `selection[0]->parent` is returned.
  static std::unique_ptr<BookmarkParentFolder> GetParentForNewNodes(
      const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                VectorExperimental>>& selection);

  // ui::SimpleMenuModel::Delegate implementation:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;

  // Public for testing.
  // Returns index at which the newly added nodes will be added.
  size_t GetIndexForNewNodes() const;

 private:
  void BuildMenu();

  // Adds a IDC_* style command to the menu with a string16.
  void AddItem(int id, const std::u16string str);
  // Adds a IDC_* style command to the menu with a localized string.
  void AddItem(int id, int localization_id);
  // Adds a separator to the menu.
  void AddSeparator();
  // Adds a checkable item to the menu.
  void AddCheckboxItem(int id, int localization_id);

  // Overridden from bookmarks::BaseBookmarkModelObserver:
  // Any change to the model results in closing the menu.
  void BookmarkModelChanged() override;

  gfx::NativeWindow parent_window_;
  raw_ptr<BookmarkContextMenuControllerDelegate> delegate_;
  const raw_ptr<Browser> browser_;
  raw_ptr<Profile> profile_;
  const BookmarkLaunchLocation opened_from_;
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      selection_;
  const raw_ptr<BookmarkMergedSurfaceService> bookmark_service_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  const std::unique_ptr<BookmarkParentFolder> new_nodes_parent_;
  // Used to detect deletion of |this| executing a command.
  base::WeakPtrFactory<BookmarkContextMenuController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_
