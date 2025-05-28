// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_

#include <memory>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/views/controls/menu/menu_delegate.h"

class BookmarkBarView;
class BookmarkMenuControllerObserver;
class BookmarkMenuDelegate;
class Browser;

namespace ui {
class OSExchangeData;
}

namespace views {
class MenuButton;
class MenuItemView;
class MenuRunner;
class Widget;
}  // namespace views

// BookmarkMenuController is responsible for showing a menu of bookmarks,
// each item in the menu represents a bookmark.
// BookmarkMenuController deletes itself as necessary, although the menu can
// be explicitly hidden by way of the Cancel method.
class BookmarkMenuController : public BookmarkMergedSurfaceServiceObserver,
                               public views::MenuDelegate {
 public:
  // Creates a BookmarkMenuController showing the children of `folder` starting
  // at `start_child_index`.
  BookmarkMenuController(Browser* browser,
                         views::Widget* parent,
                         const BookmarkParentFolder& folder,
                         size_t start_child_index,
                         bool for_drop);

  BookmarkMenuController(const BookmarkMenuController&) = delete;
  BookmarkMenuController& operator=(const BookmarkMenuController&) = delete;

  void RunMenuAt(BookmarkBarView* bookmark_bar);

  void clear_bookmark_bar() { bookmark_bar_ = nullptr; }

  // Hides the menu.
  void Cancel();

  // Returns the node the menu is showing for.
  const BookmarkParentFolder& folder() const { return folder_; }

  // Returns the menu.
  views::MenuItemView* menu() const;

  // Returns the context menu, or nullptr if the context menu isn't showing.
  views::MenuItemView* context_menu() const;

  void set_observer(BookmarkMenuControllerObserver* observer) {
    observer_ = observer;
  }

  // views::MenuDelegate:
  std::u16string GetTooltipText(int id, const gfx::Point& p) const override;
  bool IsTriggerableEvent(views::MenuItemView* view,
                          const ui::Event& e) override;
  void ExecuteCommand(int id, int mouse_event_flags) override;
  bool ShouldExecuteCommandWithoutClosingMenu(int id,
                                              const ui::Event& e) override;
  bool GetDropFormats(views::MenuItemView* menu,
                      int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired(views::MenuItemView* menu) override;
  bool CanDrop(views::MenuItemView* menu,
               const ui::OSExchangeData& data) override;
  ui::mojom::DragOperation GetDropOperation(views::MenuItemView* item,
                                            const ui::DropTargetEvent& event,
                                            DropPosition* position) override;
  views::View::DropCallback GetDropCallback(
      views::MenuItemView* menu,
      DropPosition position,
      const ui::DropTargetEvent& event) override;
  bool ShowContextMenu(views::MenuItemView* source,
                       int id,
                       const gfx::Point& p,
                       ui::mojom::MenuSourceType source_type) override;
  bool CanDrag(views::MenuItemView* menu) override;
  void WriteDragData(views::MenuItemView* sender,
                     ui::OSExchangeData* data) override;
  int GetDragOperations(views::MenuItemView* sender) override;
  bool ShouldCloseOnDragComplete() override;
  void OnMenuClosed(views::MenuItemView* menu) override;
  views::MenuItemView* GetSiblingMenu(views::MenuItemView* menu,
                                      const gfx::Point& screen_point,
                                      views::MenuAnchorPosition* anchor,
                                      bool* has_mnemonics,
                                      views::MenuButton** button) override;
  int GetMaxWidthForMenu(views::MenuItemView* view) override;
  void WillShowMenu(views::MenuItemView* menu) override;
  bool ShouldTryPositioningBesideAnchor() const override;

  // BookmarkMergedSurfaceServiceObserver:
  void BookmarkMergedSurfaceServiceLoaded() override;
  void BookmarkMergedSurfaceServiceBeingDeleted() override;
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override;
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override;
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override;
  void BookmarkAllUserNodesRemoved() override;

  void BookmarkStartIndexChanged(const BookmarkParentFolder& folder,
                                 size_t new_start_index);

 private:
  // BookmarkMenuController deletes itself as necessary.
  ~BookmarkMenuController() override;

  void BookmarkMergedSurfaceServiceChanged();

  std::unique_ptr<views::MenuRunner> menu_runner_;

  std::unique_ptr<BookmarkMenuDelegate> menu_delegate_;

  // The folder we're showing the contents of.
  const BookmarkParentFolder folder_;

  // Data for the drop.
  bookmarks::BookmarkNodeData drop_data_;

  // The observer, may be null.
  raw_ptr<BookmarkMenuControllerObserver> observer_;

  // Is the menu being shown for a drop?
  bool for_drop_;

  // The bookmark bar. This is only non-null if we're showing a menu item for
  // a folder on the bookmark bar and not for drop, or if the BookmarkBarView
  // has been destroyed before the menu.
  raw_ptr<BookmarkBarView> bookmark_bar_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_VIEWS_H_
