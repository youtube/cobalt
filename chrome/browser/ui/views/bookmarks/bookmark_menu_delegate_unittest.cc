// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_utils.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {
const char kBasePath[] = "file:///c:/tmp/";

MATCHER_P(BookmarkVariantMatcher, node, "") {
  if (node->is_url()) {
    return std::holds_alternative<const BookmarkNode*>(arg) &&
           std::get<const BookmarkNode*>(arg) == node;
  } else {
    return std::get<BookmarkParentFolder>(arg) ==
           BookmarkParentFolder::FromFolderNode(node);
  }
}

}  // namespace

class BookmarkMenuDelegateTest : public BrowserWithTestWindowTest {
 public:
  BookmarkMenuDelegateTest() = default;
  BookmarkMenuDelegateTest(const BookmarkMenuDelegateTest&) = delete;
  BookmarkMenuDelegateTest& operator=(const BookmarkMenuDelegateTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Set managed bookmarks.
    sync_preferences::TestingPrefServiceSyncable* prefs =
        profile()->GetTestingPrefService();
    ASSERT_FALSE(prefs->HasPrefPath(bookmarks::prefs::kManagedBookmarks));
    prefs->SetManagedPref(
        bookmarks::prefs::kManagedBookmarks,
        base::Value::List().Append(
            base::Value::Dict()
                .Set("name", "Google")
                .Set("url", GURL("http://google.com/").spec())));

    bookmarks::test::WaitForBookmarkModelToLoad(model());
    CHECK(managed_node());
    AddTestData();
  }

  void TearDown() override {
    DestroyDelegate();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
                BookmarkModelFactory::GetInstance(),
                BookmarkModelFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                ManagedBookmarkServiceFactory::GetInstance(),
                ManagedBookmarkServiceFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                BookmarkMergedSurfaceServiceFactory::GetInstance(),
                BookmarkMergedSurfaceServiceFactory::GetDefaultFactory()}};
  }

 protected:
  bool ShouldCloseOnRemove(const bookmarks::BookmarkNode* node) const {
    return bookmark_menu_delegate_->ShouldCloseOnRemove(
        BookmarkMenuDelegate::BookmarkFolderOrURL(node));
  }

  // Destroys the delegate. Do this rather than directly deleting
  // |bookmark_menu_delegate_| as otherwise the menu is leaked.
  void DestroyDelegate() {
    if (!bookmark_menu_delegate_.get()) {
      return;
    }

    views::MenuItemView* menu = bookmark_menu_delegate_->menu();
    bookmark_menu_delegate_.reset();
    // Since we never show the menu we need to pass the MenuItemView to
    // MenuRunner so that the MenuItemView is destroyed.
    if (menu) {
      views::MenuRunner menu_runner(base::WrapUnique(menu), 0);
    }
  }

  void NewDelegate() {
    DestroyDelegate();

    bookmark_menu_delegate_ = std::make_unique<BookmarkMenuDelegate>(
        browser(), nullptr, &test_delegate_, BookmarkLaunchLocation::kNone);
  }

  void NewAndBuildFullMenu() {
    root_menu_ = std::make_unique<views::MenuItemView>();
    // Add a placeholder here because in practice the full menu is never
    // empty.
    root_menu_->AppendTitle(std::u16string());
    root_menu_->CreateSubmenu();
    NewDelegate();
    bookmark_menu_delegate_->BuildFullMenu(root_menu_.get());
  }

  void NewAndBuildFullMenuWithBookmarksTitle() {
    // Remove the managed bookmarks node.
    profile()->GetTestingPrefService()->SetManagedPref(
        bookmarks::prefs::kManagedBookmarks, base::Value::List());
    root_menu_ = std::make_unique<views::MenuItemView>();
    root_menu_->CreateSubmenu();
    // Add a placeholder to ensure the bookmarks title is added.
    root_menu_->AppendTitle(std::u16string());
    NewDelegate();
    bookmark_menu_delegate_->BuildFullMenu(root_menu_.get());
  }

  std::variant<const BookmarkNode*, BookmarkParentFolder> GetNodeForMenuItem(
      views::MenuItemView* menu) {
    const auto& node_map = bookmark_menu_delegate_->menu_id_to_node_map_;
    auto iter = node_map.find(menu->GetCommand());
    if (iter == node_map.end()) {
      return nullptr;
    }

    if (const BookmarkParentFolder* folder = iter->second.GetIfBookmarkFolder();
        folder) {
      return *folder;
    }

    return iter->second.GetIfBookmarkURL();
  }

  int next_menu_id() { return bookmark_menu_delegate_->next_menu_id_; }

  // Forces all the menus to load by way of invoking WillShowMenu() on all menu
  // items of tyep SUBMENU.
  void LoadAllMenus(views::MenuItemView* menu) {
    EXPECT_EQ(views::MenuItemView::Type::kSubMenu, menu->GetType());

    for (views::MenuItemView* item : menu->GetSubmenu()->GetMenuItems()) {
      if (item->GetType() == views::MenuItemView::Type::kSubMenu) {
        bookmark_menu_delegate_->WillShowMenu(item);
        LoadAllMenus(item);
      }
    }
  }

  BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(profile());
  }

  const BookmarkNode* managed_node() {
    return ManagedBookmarkServiceFactory::GetForProfile(profile())
        ->managed_node();
  }

  // Returns the menu being used for the test.
  views::MenuItemView* menu() {
    return root_menu_.get() ? root_menu_.get()
                            : bookmark_menu_delegate_->menu();
  }

  std::unique_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

  std::unique_ptr<views::MenuItemView> root_menu_;

 private:
  // Creates the following structure:
  // bookmark bar node
  //   a
  //   F1
  //    f1a
  //    F11
  //     f11a
  //   F2
  // other node
  //   oa
  //   OF1
  //     of1a
  // mobile node
  //   ma
  //   mF1
  //     mf1a
  void AddTestData() {
    const BookmarkNode* bb_node = model()->bookmark_bar_node();
    std::string test_base(kBasePath);
    model()->AddURL(bb_node, 0, u"a", GURL(test_base + "a"));
    const BookmarkNode* f1 = model()->AddFolder(bb_node, 1, u"F1");
    model()->AddURL(f1, 0, u"f1a", GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model()->AddFolder(f1, 1, u"F11");
    model()->AddURL(f11, 0, u"f11a", GURL(test_base + "f11a"));
    model()->AddFolder(bb_node, 2, u"F2");

    // Children of the other node.
    model()->AddURL(model()->other_node(), 0, u"oa", GURL(test_base + "oa"));
    const BookmarkNode* of1 =
        model()->AddFolder(model()->other_node(), 1, u"OF1");
    model()->AddURL(of1, 0, u"of1a", GURL(test_base + "of1a"));

    // Children of the mobile node.
    model()->AddURL(model()->mobile_node(), 0, u"ma", GURL(test_base + "ma"));
    const BookmarkNode* mf1 =
        model()->AddFolder(model()->mobile_node(), 1, u"mF1");
    model()->AddURL(mf1, 0, u"mf1a", GURL(test_base + "mf1a"));
  }

  views::MenuDelegate test_delegate_;
};

TEST_F(BookmarkMenuDelegateTest, VerifyLazyLoad) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  ASSERT_TRUE(root_item->HasSubmenu());
  EXPECT_EQ(8u, root_item->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(10u, root_item->GetSubmenu()->children().size());  // + separators
  views::MenuItemView* f1_item = root_item->GetSubmenu()->GetMenuItemAt(4);
  ASSERT_TRUE(f1_item->HasSubmenu());
  // f1 hasn't been loaded yet.
  EXPECT_EQ(0u, f1_item->GetSubmenu()->GetMenuItems().size());
  // Will show triggers a load.
  int next_id_before_load = next_menu_id();
  bookmark_menu_delegate_->WillShowMenu(f1_item);
  // f1 should have loaded its children.
  EXPECT_EQ(next_id_before_load + 2 * AppMenuModel::kNumUnboundedMenuTypes,
            next_menu_id());
  ASSERT_EQ(2u, f1_item->GetSubmenu()->GetMenuItems().size());
  const BookmarkNode* f1_node =
      model()->bookmark_bar_node()->children()[1].get();
  EXPECT_THAT(GetNodeForMenuItem(f1_item->GetSubmenu()->GetMenuItemAt(0)),
              BookmarkVariantMatcher(f1_node->children()[0].get()));
  EXPECT_THAT(GetNodeForMenuItem(f1_item->GetSubmenu()->GetMenuItemAt(1)),
              BookmarkVariantMatcher(f1_node->children()[1].get()));

  // F11 shouldn't have loaded yet.
  views::MenuItemView* f11_item = f1_item->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_TRUE(f11_item->HasSubmenu());
  EXPECT_EQ(0u, f11_item->GetSubmenu()->GetMenuItems().size());

  next_id_before_load = next_menu_id();
  bookmark_menu_delegate_->WillShowMenu(f11_item);
  // Invoke WillShowMenu() twice to make sure the second call doesn't cause
  // problems.
  bookmark_menu_delegate_->WillShowMenu(f11_item);
  // F11 should have loaded its single child (f11a).
  EXPECT_EQ(next_id_before_load + AppMenuModel::kNumUnboundedMenuTypes,
            next_menu_id());

  ASSERT_EQ(1u, f11_item->GetSubmenu()->GetMenuItems().size());
  const BookmarkNode* f11_node = f1_node->children()[1].get();
  EXPECT_THAT(GetNodeForMenuItem(f11_item->GetSubmenu()->GetMenuItemAt(0)),
              BookmarkVariantMatcher(f11_node->children()[0].get()));
}

// Verifies WillRemoveBookmarks() doesn't attempt to access MenuItemViews that
// have since been deleted.
TEST_F(BookmarkMenuDelegateTest, RemoveBookmarks) {
  const BookmarkNode* node = model()->bookmark_bar_node()->children()[1].get();
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(node), 0);
  LoadAllMenus(menu());
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes_to_remove =
      {
          node->children()[1].get(),
      };
  bookmark_menu_delegate_->WillRemoveBookmarks(nodes_to_remove);
  nodes_to_remove.clear();
  bookmark_menu_delegate_->DidRemoveBookmarks();
}

// Verifies WillRemoveBookmarks() doesn't attempt to access MenuItemViews that
// have since been deleted.
TEST_F(BookmarkMenuDelegateTest, CloseOnRemove) {
  const BookmarkNode* node = model()->bookmark_bar_node()->children()[1].get();
  const BookmarkParentFolder folder =
      BookmarkParentFolder::FromFolderNode(node);
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(folder, 0);
  // Any nodes on the bookmark bar should close on remove.
  EXPECT_TRUE(
      ShouldCloseOnRemove(model()->bookmark_bar_node()->children()[2].get()));

  // Descendants of the bookmark should not close on remove.
  EXPECT_FALSE(ShouldCloseOnRemove(
      model()->bookmark_bar_node()->children()[1]->children()[0].get()));

  EXPECT_FALSE(ShouldCloseOnRemove(model()->other_node()->children()[0].get()));

  // Make it so the other node only has one child.
  // Destroy the current delegate so that it doesn't have any references to
  // deleted nodes.
  DestroyDelegate();
  while (model()->other_node()->children().size() > 1) {
    model()->Remove(model()->other_node()->children()[1].get(),
                    bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  }

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(folder, 0);
  // Any nodes on the bookmark bar should close on remove.
  EXPECT_TRUE(ShouldCloseOnRemove(model()->other_node()->children()[0].get()));
}

// Tests that the "Bookmarks" title and separator are removed from the parent
// menu when the children of the bookmark bar node are removed.
TEST_F(BookmarkMenuDelegateTest, UpdateBookmarksTitleAfterNodeRemoved) {
  NewAndBuildFullMenuWithBookmarksTitle();
  views::MenuItemView* root_menu = menu();

  ASSERT_TRUE(root_menu->HasSubmenu());
  EXPECT_EQ(7u, root_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(9u, root_menu->GetSubmenu()->children().size());  // + separators

  // Remove all bookmark bar nodes.
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes_to_remove =
      {};
  for (const std::unique_ptr<BookmarkNode>& node :
       bookmark_bar_node->children()) {
    nodes_to_remove.emplace_back(node.get());
  }
  bookmark_menu_delegate_->WillRemoveBookmarks(nodes_to_remove);
  nodes_to_remove.clear();
  while (!bookmark_bar_node->children().empty()) {
    model()->Remove(bookmark_bar_node->children()[0].get(),
                    bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  }
  bookmark_menu_delegate_->DidRemoveBookmarks();

  // The placeholder, "other" and mobile bookmark folders, and their separator
  // remain.
  EXPECT_EQ(3u, root_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(4u, root_menu->GetSubmenu()->children().size());  // separator
}

// Tests that the separator is removed from the "other" bookmarks menu item
// when its child bookmarks are removed.
TEST_F(BookmarkMenuDelegateTest, UpdateOtherNodeMenuAfterNodeRemoved) {
  const BookmarkNode* other_node = model()->other_node();
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(BookmarkParentFolder::OtherFolder(),
                                         0);
  views::MenuItemView* other_node_menu = menu();

  ASSERT_TRUE(other_node_menu->HasSubmenu());
  EXPECT_EQ(3u, other_node_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(4u, other_node_menu->GetSubmenu()->children().size());  // separator

  // Remove all "other" node children.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes_to_remove =
      {};
  for (const std::unique_ptr<BookmarkNode>& node : other_node->children()) {
    nodes_to_remove.emplace_back(node.get());
  }
  bookmark_menu_delegate_->WillRemoveBookmarks(nodes_to_remove);
  nodes_to_remove.clear();
  while (!other_node->children().empty()) {
    model()->Remove(other_node->children()[0].get(),
                    bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  }
  bookmark_menu_delegate_->DidRemoveBookmarks();

  EXPECT_EQ(1u, other_node_menu->GetSubmenu()->children().size());
}

TEST_F(BookmarkMenuDelegateTest, DragAndDropAfterNode) {
  const BookmarkNode* f1 = model()->bookmark_bar_node()->children()[1].get();
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f1a_item = root_item->GetSubmenu()->GetMenuItemAt(0);
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f1a_item, drop_data));
  EXPECT_EQ(f1->children().size(), 2u);

  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kAfter;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(f1a_item, target_event,
                                                      &drop_position),
            ui::mojom::DragOperation::kCopy);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f1a_item, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(f1->children().size(), 3u);
  // New bookmark added at `f1a_item` index + 1.
  EXPECT_EQ(f1->children()[1]->GetTitle(), std::u16string(u"z"));
}

TEST_F(BookmarkMenuDelegateTest, DragAndDropOnNode) {
  const BookmarkNode* f1 = model()->bookmark_bar_node()->children()[1].get();
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f11_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  const BookmarkNode* f11_node = f1->children()[1].get();
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f11_item, drop_data));
  EXPECT_EQ(f11_node->children().size(), 1u);

  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kOn;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(f11_item, target_event,
                                                      &drop_position),
            ui::mojom::DragOperation::kCopy);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f11_item, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(f11_node->children().size(), 2u);
  // New bookmark added at `f11_item` old index.
  EXPECT_EQ(f11_node->children()[1]->GetTitle(), std::u16string(u"z"));
}

TEST_F(BookmarkMenuDelegateTest, DragAndDropBeforeNode) {
  const BookmarkNode* f1 = model()->bookmark_bar_node()->children()[1].get();
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1), 0);
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f11_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f11_item, drop_data));
  EXPECT_EQ(f1->children().size(), 2u);

  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kBefore;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(f11_item, target_event,
                                                      &drop_position),
            ui::mojom::DragOperation::kCopy);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f11_item, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(f1->children().size(), 3u);
  // New bookmark added at `f11_item` old index.
  EXPECT_EQ(f1->children()[1]->GetTitle(), std::u16string(u"z"));
}

TEST_F(BookmarkMenuDelegateTest, DropCallbackModelChanged) {
  const BookmarkNode* node = model()->bookmark_bar_node()->children()[1].get();
  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(node), 0);
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  gfx::Point menu_loc;
  views::View::ConvertPointToScreen(root_item, &menu_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(menu_loc),
                                   gfx::PointF(menu_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  auto* f1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_TRUE(bookmark_menu_delegate_->CanDrop(f1_item, drop_data));
  EXPECT_EQ(model()->bookmark_bar_node()->children()[1]->children().size(), 2u);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      f1_item, views::MenuDelegate::DropPosition::kAfter, target_event);
  model()->AddURL(model()->bookmark_bar_node(), 2, u"z1",
                  GURL(std::string(kBasePath) + "z1"));
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kNone);
  EXPECT_EQ(model()->bookmark_bar_node()->children()[1]->children().size(), 2u);
}

TEST_F(BookmarkMenuDelegateTest, DragAndDropInvalid) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_COPY);

  // Drop before managed node.

  auto* managed_folder_menu = root_item->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_EQ(managed_folder_menu->title(), managed_node()->GetTitle());
  // Calling `CanDrop()` is required as it sets `drop_data_`.
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(managed_folder_menu, drop_data));

  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kBefore;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                managed_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kNone);

  // Drop before mobile node.

  size_t mobile_folder_menu_index =
      4u +  // placeholder + bookmarks title + managed + other node.
      model()->bookmark_bar_node()->children().size();
  auto* mobile_folder_menu =
      root_item->GetSubmenu()->GetMenuItemAt(mobile_folder_menu_index);
  ASSERT_EQ(mobile_folder_menu->title(), model()->mobile_node()->GetTitle());
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(mobile_folder_menu, drop_data));

  drop_position = views::MenuDelegate::DropPosition::kBefore;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                mobile_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kNone);

  // Drop after mobile node.

  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(mobile_folder_menu, drop_data));

  drop_position = views::MenuDelegate::DropPosition::kAfter;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                mobile_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kNone);

  // Drop after other node.

  auto* other_folder_menu =
      root_item->GetSubmenu()->GetMenuItemAt(mobile_folder_menu_index - 1);
  ASSERT_EQ(other_folder_menu->title(), model()->other_node()->GetTitle());
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(other_folder_menu, drop_data));

  drop_position = views::MenuDelegate::DropPosition::kAfter;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                other_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kNone);

  // Drop on url.

  auto* url_item = root_item->GetSubmenu()->GetMenuItemAt(3);
  ASSERT_EQ(url_item->title(),
            model()->bookmark_bar_node()->children()[0]->GetTitle());
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(url_item, drop_data));

  drop_position = views::MenuDelegate::DropPosition::kOn;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(url_item, target_event,
                                                      &drop_position),
            ui::mojom::DragOperation::kNone);
}

TEST_F(BookmarkMenuDelegateTest, DragAndDropAfterManagedNode) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  auto* managed_folder_menu = root_item->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_EQ(managed_folder_menu->title(), managed_node()->GetTitle());

  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_LINK);
  // Calling `CanDrop()` is required as it sets `drop_data_`.
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(managed_folder_menu, drop_data));
  size_t bookmark_bar_nodes_size =
      model()->bookmark_bar_node()->children().size();

  // Drop after managed node.
  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kAfter;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                managed_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kLink);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      managed_folder_menu, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(model()->bookmark_bar_node()->children().size(),
            bookmark_bar_nodes_size + 1);
  // New bookmark added at the beginning of bookmark bar children.
  EXPECT_EQ(model()->bookmark_bar_node()->children()[0]->GetTitle(),
            std::u16string(u"z"));
}

TEST_F(BookmarkMenuDelegateTest, DragAndDropBeforeOtherNode) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  LoadAllMenus(root_item);

  size_t bookmark_bar_nodes_size =
      model()->bookmark_bar_node()->children().size();
  auto* other_folder_menu = root_item->GetSubmenu()->GetMenuItemAt(
      bookmark_bar_nodes_size + 3u);  // add managed folder + bookmarks title.
  ASSERT_EQ(other_folder_menu->title(), model()->other_node()->GetTitle());

  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_LINK);
  // Calling `CanDrop()` is required as it sets `drop_data_`.
  ASSERT_TRUE(bookmark_menu_delegate_->CanDrop(other_folder_menu, drop_data));

  // Drop before other node.
  views::MenuDelegate::DropPosition drop_position =
      views::MenuDelegate::DropPosition::kBefore;
  EXPECT_EQ(bookmark_menu_delegate_->GetDropOperation(
                other_folder_menu, target_event, &drop_position),
            ui::mojom::DragOperation::kLink);

  auto drop_cb = bookmark_menu_delegate_->GetDropCallback(
      other_folder_menu, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(model()->bookmark_bar_node()->children().size(),
            bookmark_bar_nodes_size + 1);
  // New node added at the end of the bookmark bar children.
  EXPECT_EQ(model()
                ->bookmark_bar_node()
                ->children()[bookmark_bar_nodes_size]
                ->GetTitle(),
            std::u16string(u"z"));
}

// Tests moving a bookmark between two normal bookmark folders.
TEST_F(BookmarkMenuDelegateTest, MovingBookmarksBetweenNormalFolders) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  views::MenuItemView* f1_item = root_item->GetSubmenu()->GetMenuItemAt(4);
  views::MenuItemView* f2_item = root_item->GetSubmenu()->GetMenuItemAt(5);

  // Folders haven't been loaded yet.
  ASSERT_TRUE(f1_item->HasSubmenu());
  ASSERT_TRUE(f2_item->HasSubmenu());
  EXPECT_TRUE(f1_item->GetSubmenu()->GetMenuItems().empty());
  EXPECT_TRUE(f2_item->GetSubmenu()->GetMenuItems().empty());

  const BookmarkNode* f1_node =
      model()->bookmark_bar_node()->children()[1].get();
  const BookmarkNode* f1a_node = f1_node->children()[0].get();
  const BookmarkNode* f11_node = f1_node->children()[1].get();
  const BookmarkNode* f2_node =
      model()->bookmark_bar_node()->children()[2].get();

  // Move to a folder that doesn't have a menu. There should be no visible
  // changed.
  model()->Move(f1a_node, f11_node, 0);
  EXPECT_TRUE(f1_item->GetSubmenu()->GetMenuItems().empty());
  EXPECT_TRUE(f2_item->GetSubmenu()->GetMenuItems().empty());

  // Move to F2, which has a menu but hasn't been loaded yet.
  model()->Move(f1a_node, f2_node, 0);
  EXPECT_TRUE(f1_item->GetSubmenu()->GetMenuItems().empty());
  EXPECT_TRUE(f2_item->GetSubmenu()->GetMenuItems().empty());

  // Load the two menus. The move should now be reflected.
  bookmark_menu_delegate_->WillShowMenu(f1_item);
  bookmark_menu_delegate_->WillShowMenu(f2_item);
  EXPECT_EQ(1u, f1_item->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(1u, f2_item->GetSubmenu()->GetMenuItems().size());

  // Move from F2 to F1.
  model()->Move(f1a_node, f1_node, 0);
  EXPECT_EQ(2u, f1_item->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(0u, f2_item->GetSubmenu()->GetMenuItems().size());
}

// Tests moving a bookmark whose menu doesn't have a parent.
TEST_F(BookmarkMenuDelegateTest, MoveBookmarkWithoutParentMenu) {
  const BookmarkNode* const bookmark_bar_node = model()->bookmark_bar_node();
  ASSERT_EQ(3u, bookmark_bar_node->children().size());

  const BookmarkNode* const f1_node =
      model()->bookmark_bar_node()->children()[1].get();

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f1_node), 0);
  // In practice, additional menus created by `SetActiveMenu` are registered as siblings of
  // the menu runner, which handles deletion.
  const std::unique_ptr<views::MenuItemView> f1_menu(menu());
  ASSERT_NE(nullptr, f1_menu);
  EXPECT_EQ(nullptr, f1_menu->GetParentMenuItem());

  const BookmarkNode* const f2_node =
      model()->bookmark_bar_node()->children()[2].get();

  // Move f1_node, which doesn't have a parent menu, to f2_node.
  // f1_node's menu should be a child of f2_node.
  model()->Move(f1_node, f2_node, 0);

  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::FromFolderNode(f2_node), 0);
  ASSERT_NE(nullptr, menu());
  ASSERT_TRUE(menu()->HasSubmenu());
  ASSERT_FALSE(menu()->GetSubmenu()->GetMenuItems().empty());
  EXPECT_EQ(f1_node->GetTitle(),
            menu()->GetSubmenu()->GetMenuItemAt(0)->title());
}

// Tests that the bookmarks title is appropriately added and removed when moving
// bookmarks into/out of the bookmarks bar for an embedded menu.
TEST_F(BookmarkMenuDelegateTest, MovingBookmarkUpdatesBookmarksTitle) {
  NewAndBuildFullMenuWithBookmarksTitle();
  views::MenuItemView* root_menu = menu();
  EXPECT_EQ(7u, root_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(9u, root_menu->GetSubmenu()->children().size());  // + separators

  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  const BookmarkNode* other_node = model()->other_node();

  model()->Move(bookmark_bar_node->children()[2].get(), other_node, 0);
  model()->Move(bookmark_bar_node->children()[1].get(), other_node, 0);
  EXPECT_EQ(5u, root_menu->GetSubmenu()->GetMenuItems().size());

  // Removing the last bookmark bar node should remove both the bookmark and
  // the bookmarks title from the menu.
  const BookmarkNode* a_node = bookmark_bar_node->children()[0].get();
  model()->Move(a_node, other_node, 0);
  EXPECT_EQ(3u, root_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(4u, root_menu->GetSubmenu()->children().size());  // + separators

  // Adding the bookmark back to the bookmark bar should add the title above
  // permanent nodes. The moved bookmark's menu should appear after the title.
  model()->Move(a_node, bookmark_bar_node, 0);
  EXPECT_EQ(5u, root_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(7u, root_menu->GetSubmenu()->children().size());  // + separators

  views::MenuItemView* bookmarks_title =
      root_menu->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BOOKMARKS_LIST_TITLE),
            bookmarks_title->title());

  views::MenuItemView* a_node_item = root_menu->GetSubmenu()->GetMenuItemAt(2);
  EXPECT_EQ(u"a", a_node_item->title());
}

// Tests that the separator in the "other" bookmarks menu is appropriately added
// and removed when moving bookmarks into/out of it.
TEST_F(BookmarkMenuDelegateTest, MovingBookmarkUpdatesOtherNodeHeader) {
  NewAndBuildFullMenu();
  views::MenuItemView* root_item = menu();
  views::MenuItemView* other_node_menu =
      root_item->GetSubmenu()->GetMenuItemAt(6);
  bookmark_menu_delegate_->WillShowMenu(other_node_menu);

  EXPECT_EQ(3u, other_node_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(4u, other_node_menu->GetSubmenu()->children().size());

  const BookmarkNode* other_node = model()->other_node();
  const BookmarkNode* oa_node = other_node->children()[0].get();
  const BookmarkNode* of1_node = other_node->children()[1].get();

  model()->Move(oa_node, model()->bookmark_bar_node(), 0);
  EXPECT_EQ(2u, other_node_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(3u, other_node_menu->GetSubmenu()->children().size());

  // Moving the last node should remove the separator too.
  model()->Move(of1_node, model()->bookmark_bar_node(), 1);
  EXPECT_EQ(1u, other_node_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(1u, other_node_menu->GetSubmenu()->children().size());

  // Adding the bookmark back to the other folder should add the separator.
  model()->Move(oa_node, other_node, 0);
  EXPECT_EQ(2u, other_node_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(3u, other_node_menu->GetSubmenu()->children().size());

  EXPECT_TRUE(views::IsViewClass<views::MenuSeparator>(
      other_node_menu->GetSubmenu()->children()[1]));

  views::MenuItemView* oa_item =
      other_node_menu->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_EQ(u"oa", oa_item->title());
}

// Tests that moving bookmarks into/out of a folder built with a "start index"
// respescts the initially provided start index.
TEST_F(BookmarkMenuDelegateTest, MovingBookmarkRespectsStartIndex) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  ASSERT_EQ(3u, bookmark_bar_node->children().size());

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::BookmarkBarFolder(), 1);

  views::MenuItemView* root_menu = menu();
  EXPECT_EQ(2u, root_menu->GetSubmenu()->GetMenuItems().size());

  const BookmarkNode* f1_node = bookmark_bar_node->children()[1].get();
  const BookmarkNode* f2_node = bookmark_bar_node->children()[2].get();
  model()->Move(f1_node, model()->other_node(), 0);
  model()->Move(f2_node, model()->other_node(), 0);
  EXPECT_TRUE(root_menu->GetSubmenu()->GetMenuItems().empty());

  model()->Move(f1_node, bookmark_bar_node, 1);
  model()->Move(f2_node, bookmark_bar_node, 2);
  EXPECT_EQ(2u, root_menu->GetSubmenu()->GetMenuItems().size());
}

// Tests that moving a bookmark into the hidden section of a menu does nothing.
TEST_F(BookmarkMenuDelegateTest, MovingBookmarkBeforeStartIndexDoesNothing) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  ASSERT_EQ(3u, bookmark_bar_node->children().size());

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::BookmarkBarFolder(), 1);

  views::MenuItemView* root_menu = menu();
  // The menu has items for nodes F1 and F2.
  EXPECT_EQ(2u, root_menu->GetSubmenu()->GetMenuItems().size());

  // Moving another node to the first index should do nothing.
  model()->Move(model()->other_node()->children()[0].get(), bookmark_bar_node,
                0);
  EXPECT_EQ(2u, root_menu->GetSubmenu()->GetMenuItems().size());
}

TEST_F(BookmarkMenuDelegateTest, IncreaseStartIndex) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  ASSERT_EQ(3u, bookmark_bar_node->children().size());

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::BookmarkBarFolder(), 0);
  views::MenuItemView* root_menu = menu();
  // The menu has items for nodes, a, F1 and F2.
  EXPECT_EQ(3u, root_menu->GetSubmenu()->GetMenuItems().size());

  // Increasing the start index should remove the first nodes.
  bookmark_menu_delegate_->SetMenuStartIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 2);
  ASSERT_TRUE(root_menu->HasSubmenu());
  ASSERT_EQ(1u, root_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(u"F2", root_menu->GetSubmenu()->GetMenuItemAt(0)->title());
}

TEST_F(BookmarkMenuDelegateTest, DecreaseStartIndex) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  ASSERT_EQ(3u, bookmark_bar_node->children().size());

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::BookmarkBarFolder(), 2);
  views::MenuItemView* root_menu = menu();
  ASSERT_TRUE(root_menu->HasSubmenu());
  EXPECT_EQ(1u, root_menu->GetSubmenu()->GetMenuItems().size());

  // Decreasing the starting should add the missing nodes.
  bookmark_menu_delegate_->SetMenuStartIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 1);
  ASSERT_TRUE(root_menu->HasSubmenu());
  ASSERT_EQ(2u, root_menu->GetSubmenu()->GetMenuItems().size());
  EXPECT_EQ(u"F1", root_menu->GetSubmenu()->GetMenuItemAt(0)->title());
  EXPECT_EQ(u"F2", root_menu->GetSubmenu()->GetMenuItemAt(1)->title());
}

TEST_F(BookmarkMenuDelegateTest, SetMenuStartIndexUnchanged) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  ASSERT_EQ(3u, bookmark_bar_node->children().size());

  NewDelegate();
  bookmark_menu_delegate_->SetActiveMenu(
      BookmarkParentFolder::BookmarkBarFolder(), 2);
  views::MenuItemView* root_menu = menu();
  ASSERT_TRUE(root_menu->HasSubmenu());
  EXPECT_EQ(1u, root_menu->GetSubmenu()->GetMenuItems().size());

  // Nothing should happen if the index is unchanged.
  bookmark_menu_delegate_->SetMenuStartIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 2);
  ASSERT_TRUE(root_menu->HasSubmenu());
  EXPECT_EQ(1u, root_menu->GetSubmenu()->GetMenuItems().size());
}

TEST_F(BookmarkMenuDelegateTest, SetMenuStartIndexForMissingMenu) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  ASSERT_EQ(3u, bookmark_bar_node->children().size());

  NewDelegate();

  // Nothing should happen if the menu wasn't built yet.
  bookmark_menu_delegate_->SetMenuStartIndex(
      BookmarkParentFolder::BookmarkBarFolder(), 2);
  EXPECT_EQ(nullptr, menu());
}
