// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_section_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/quick_insert/mock_quick_insert_asset_fetcher.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/views/quick_insert_gif_view.h"
#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble_controller.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrEq;

constexpr int kDefaultSectionWidth = 320;

std::unique_ptr<PickerImageItemView> CreateImageItem() {
  return std::make_unique<PickerImageItemView>(
      std::make_unique<views::ImageView>(ui::ImageModel::FromImageSkia(
          gfx::test::CreateImageSkia(/*size=*/100))),
      u"image", base::DoNothing());
}

std::unique_ptr<PickerImageItemView> CreateGifItem(
    const gfx::Size& gif_dimensions) {
  return std::make_unique<PickerImageItemView>(
      std::make_unique<PickerGifView>(
          /*frames_fetcher=*/base::DoNothing(),
          /*preview_image_fetcher=*/base::DoNothing(), gif_dimensions),
      u"gif", base::DoNothing());
}

using QuickInsertSectionViewTest = views::ViewsTestBase;

TEST_F(QuickInsertSectionViewTest, HasListRole) {
  QuickInsertSectionView section_view(kDefaultSectionWidth,
                                      /*asset_fetcher=*/nullptr,
                                      /*submenu_controller=*/nullptr);

  EXPECT_EQ(section_view.GetAccessibleRole(), ax::mojom::Role::kList);
}

TEST_F(QuickInsertSectionViewTest, CreatesTitleLabel) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  const std::u16string kSectionTitleText = u"Section";
  section_view.AddTitleLabel(kSectionTitleText);

  EXPECT_THAT(section_view.title_label_for_testing(),
              Property(&views::Label::GetText, kSectionTitleText));
}

TEST_F(QuickInsertSectionViewTest, TitleHasHeadingRole) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);
  section_view.AddTitleLabel(u"Section");

  EXPECT_THAT(section_view.title_label_for_testing()->GetAccessibleRole(),
              ax::mojom::Role::kHeading);
}

TEST_F(QuickInsertSectionViewTest, AddsListItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  EXPECT_TRUE(views::IsViewClass<QuickInsertListItemView>(items[0]));
}

TEST_F(QuickInsertSectionViewTest, AddsTwoListItems) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(2));
  EXPECT_TRUE(views::IsViewClass<QuickInsertListItemView>(items[0]));
  EXPECT_TRUE(views::IsViewClass<QuickInsertListItemView>(items[1]));
}

TEST_F(QuickInsertSectionViewTest, AddsGifItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddImageGridItem(CreateGifItem(gfx::Size(100, 100)));

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  EXPECT_TRUE(views::IsViewClass<PickerImageItemView>(items[0]));
}

TEST_F(QuickInsertSectionViewTest, AddsResults) {
  MockPickerAssetFetcher asset_fetcher;
  PickerPreviewBubbleController preview_controller;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddResult(QuickInsertTextResult(u"Result"), &preview_controller,
                         QuickInsertSectionView::LocalFileResultStyle::kList,
                         base::DoNothing());
  section_view.AddResult(
      QuickInsertLocalFileResult(u"title", base::FilePath("abc.png")),
      &preview_controller, QuickInsertSectionView::LocalFileResultStyle::kList,
      base::DoNothing());

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(2));
  EXPECT_TRUE(views::IsViewClass<QuickInsertListItemView>(items[0]));
  EXPECT_TRUE(views::IsViewClass<QuickInsertListItemView>(items[1]));
}

TEST_F(QuickInsertSectionViewTest,
       BrowsingHistoryResultsWithTitleShowsTitleAsPrimary) {
  MockPickerAssetFetcher asset_fetcher;
  PickerPreviewBubbleController preview_controller;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddResult(
      QuickInsertBrowsingHistoryResult(GURL("https://www.example.com/foo"),
                                       u"Example Foo", /*icon=*/{}),
      &preview_controller, QuickInsertSectionView::LocalFileResultStyle::kList,
      base::DoNothing());

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  auto* list_item = views::AsViewClass<QuickInsertListItemView>(items[0]);
  ASSERT_TRUE(list_item);
  EXPECT_EQ(list_item->GetPrimaryTextForTesting(), u"Example Foo");
  EXPECT_EQ(list_item->GetSecondaryTextForTesting(), u"example.com/foo");
}

TEST_F(QuickInsertSectionViewTest,
       BrowsingHistoryResultsWithoutTitleShowsUrlAsPrimary) {
  MockPickerAssetFetcher asset_fetcher;
  PickerPreviewBubbleController preview_controller;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddResult(
      QuickInsertBrowsingHistoryResult(GURL("https://www.example.com/foo"),
                                       /*title=*/u"", /*icon=*/{}),
      &preview_controller, QuickInsertSectionView::LocalFileResultStyle::kList,
      base::DoNothing());

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  auto* list_item = views::AsViewClass<QuickInsertListItemView>(items[0]);
  ASSERT_TRUE(list_item);
  EXPECT_EQ(list_item->GetPrimaryTextForTesting(), u"example.com/foo");
  EXPECT_EQ(list_item->GetSecondaryTextForTesting(), u"example.com/foo");
}

TEST_F(QuickInsertSectionViewTest,
       TextClipboardHistoryResultsUseDefaultIconIfNotLink) {
  MockPickerAssetFetcher asset_fetcher;
  PickerPreviewBubbleController preview_controller;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddResult(QuickInsertClipboardResult(
                             base::UnguessableToken(),
                             QuickInsertClipboardResult::DisplayFormat::kText,
                             /*file_count=*/0,
                             /*display_text=*/u"testing",
                             /*display_image=*/{},
                             /*is_recent=*/false),
                         &preview_controller,
                         QuickInsertSectionView::LocalFileResultStyle::kList,
                         base::DoNothing());

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  auto* list_item = views::AsViewClass<QuickInsertListItemView>(items[0]);
  ASSERT_NE(list_item, nullptr);
  const gfx::VectorIcon* vector_icon =
      list_item->leading_icon_view_for_testing()
          .GetImageModel()
          .GetVectorIcon()
          .vector_icon();
  ASSERT_NE(vector_icon, nullptr);
  EXPECT_THAT(vector_icon->name, StrEq(chromeos::kTextIcon.name));
}

TEST_F(QuickInsertSectionViewTest,
       TextClipboardHistoryResultsUsesLinkIconIfValidLink) {
  MockPickerAssetFetcher asset_fetcher;
  PickerPreviewBubbleController preview_controller;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddResult(QuickInsertClipboardResult(
                             base::UnguessableToken(),
                             QuickInsertClipboardResult::DisplayFormat::kText,
                             /*file_count=*/0,
                             /*display_text=*/u"https://example.com/path",
                             /*display_image=*/{},
                             /*is_recent=*/false),
                         &preview_controller,
                         QuickInsertSectionView::LocalFileResultStyle::kList,
                         base::DoNothing());

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  auto* list_item = views::AsViewClass<QuickInsertListItemView>(items[0]);
  ASSERT_NE(list_item, nullptr);
  const gfx::VectorIcon* vector_icon =
      list_item->leading_icon_view_for_testing()
          .GetImageModel()
          .GetVectorIcon()
          .vector_icon();
  ASSERT_NE(vector_icon, nullptr);
  EXPECT_THAT(vector_icon->name, StrEq(vector_icons::kLinkIcon.name));
}

TEST_F(QuickInsertSectionViewTest,
       SingleFileClipboardHistoryResultsUseIconForFiletype) {
  MockPickerAssetFetcher asset_fetcher;
  PickerPreviewBubbleController preview_controller;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddResult(QuickInsertClipboardResult(
                             base::UnguessableToken(),
                             QuickInsertClipboardResult::DisplayFormat::kFile,
                             /*file_count=*/1,
                             /*display_text=*/u"image.png",
                             /*display_image=*/{},
                             /*is_recent=*/false),
                         &preview_controller,
                         QuickInsertSectionView::LocalFileResultStyle::kList,
                         base::DoNothing());

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  auto* list_item = views::AsViewClass<QuickInsertListItemView>(items[0]);
  ASSERT_NE(list_item, nullptr);
  const gfx::VectorIcon* vector_icon =
      list_item->leading_icon_view_for_testing()
          .GetImageModel()
          .GetVectorIcon()
          .vector_icon();
  ASSERT_NE(vector_icon, nullptr);
  EXPECT_THAT(vector_icon->name, StrEq(chromeos::kFiletypeImageIcon.name));
}

TEST_F(QuickInsertSectionViewTest,
       MultipleFileClipboardHistoryResultsUseIconForFiletype) {
  MockPickerAssetFetcher asset_fetcher;
  PickerPreviewBubbleController preview_controller;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddResult(QuickInsertClipboardResult(
                             base::UnguessableToken(),
                             QuickInsertClipboardResult::DisplayFormat::kFile,
                             /*file_count=*/2,
                             /*display_text=*/u"2 files",
                             /*display_image=*/{},
                             /*is_recent=*/false),
                         &preview_controller,
                         QuickInsertSectionView::LocalFileResultStyle::kList,
                         base::DoNothing());

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  auto* list_item = views::AsViewClass<QuickInsertListItemView>(items[0]);
  ASSERT_NE(list_item, nullptr);
  const gfx::VectorIcon* vector_icon =
      list_item->leading_icon_view_for_testing()
          .GetImageModel()
          .GetVectorIcon()
          .vector_icon();
  ASSERT_NE(vector_icon, nullptr);
  EXPECT_THAT(vector_icon->name, StrEq(vector_icons::kContentCopyIcon.name));
}

TEST_F(QuickInsertSectionViewTest, CapsLockResultShowsShortcutHint) {
  MockPickerAssetFetcher asset_fetcher;
  PickerPreviewBubbleController preview_controller;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddResult(
      QuickInsertCapsLockResult(
          /*enabled=*/true, QuickInsertCapsLockResult::Shortcut::kAltSearch),
      &preview_controller, QuickInsertSectionView::LocalFileResultStyle::kList,
      base::DoNothing());

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  auto* list_item = views::AsViewClass<QuickInsertListItemView>(items[0]);
  ASSERT_TRUE(list_item);
  EXPECT_NE(list_item->shortcut_hint_view_for_testing(), nullptr);
}

TEST_F(QuickInsertSectionViewTest, ClearsItems) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);
  section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  section_view.ClearItems();

  EXPECT_THAT(section_view.item_views_for_testing(), IsEmpty());
}

class QuickInsertSectionViewUrlFormattingTest
    : public QuickInsertSectionViewTest,
      public testing::WithParamInterface<std::pair<GURL, std::u16string>> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    QuickInsertSectionViewUrlFormattingTest,
    testing::Values(
        std::make_pair(GURL("http://foo.com/bar"), u"foo.com/bar"),
        std::make_pair(GURL("https://foo.com/bar"), u"foo.com/bar"),
        std::make_pair(GURL("https://www.foo.com/bar"), u"foo.com/bar"),
        std::make_pair(GURL("chrome://version"), u"chrome://version"),
        std::make_pair(GURL("chrome-extension://aaa"),
                       u"chrome-extension://aaa"),
        std::make_pair(GURL("file://a/b/c"), u"file://a/b/c")));

TEST_P(QuickInsertSectionViewUrlFormattingTest, AddingHistoryResultFormatsUrl) {
  MockPickerAssetFetcher asset_fetcher;
  PickerPreviewBubbleController preview_controller;
  PickerSubmenuController submenu_controller;
  QuickInsertSectionView section_view(kDefaultSectionWidth, &asset_fetcher,
                                      &submenu_controller);

  section_view.AddResult(
      QuickInsertBrowsingHistoryResult(GetParam().first, u"title", /*icon=*/{}),
      &preview_controller, QuickInsertSectionView::LocalFileResultStyle::kList,
      base::DoNothing());

  base::span<const raw_ptr<QuickInsertItemView>> items =
      section_view.item_views_for_testing();
  ASSERT_THAT(items, SizeIs(1));
  EXPECT_TRUE(views::IsViewClass<QuickInsertListItemView>(items[0]));
  EXPECT_EQ(views::AsViewClass<QuickInsertListItemView>(items[0])
                ->GetSecondaryTextForTesting(),
            GetParam().second);
}

TEST_F(QuickInsertSectionViewTest, GetItemsFromListItems) {
  QuickInsertSectionView section_view(kDefaultSectionWidth,
                                      /*asset_fetcher=*/nullptr,
                                      /*submenu_controller=*/nullptr);
  views::View* item1 = section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  views::View* item2 = section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  views::View* item3 = section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_view.GetTopItem(), item1);
  EXPECT_EQ(section_view.GetBottomItem(), item3);
  EXPECT_EQ(section_view.GetItemAbove(item1), nullptr);
  EXPECT_EQ(section_view.GetItemAbove(item2), item1);
  EXPECT_EQ(section_view.GetItemAbove(item3), item2);
  EXPECT_EQ(section_view.GetItemBelow(item1), item2);
  EXPECT_EQ(section_view.GetItemBelow(item2), item3);
  EXPECT_EQ(section_view.GetItemBelow(item3), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item2), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item3), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item1), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item3), nullptr);
}

TEST_F(QuickInsertSectionViewTest, GetItemsFromImageGridItems) {
  QuickInsertSectionView section_view(kDefaultSectionWidth,
                                      /*asset_fetcher=*/nullptr,
                                      /*submenu_controller=*/nullptr);
  views::View* item1 = section_view.AddImageGridItem(CreateImageItem());
  views::View* item2 = section_view.AddImageGridItem(CreateImageItem());
  views::View* item3 = section_view.AddImageGridItem(CreateImageItem());

  EXPECT_EQ(section_view.GetTopItem(), item1);
  EXPECT_EQ(section_view.GetBottomItem(), item3);
  EXPECT_EQ(section_view.GetItemAbove(item1), nullptr);
  EXPECT_EQ(section_view.GetItemAbove(item2), nullptr);
  EXPECT_EQ(section_view.GetItemAbove(item3), item1);
  EXPECT_EQ(section_view.GetItemBelow(item1), item3);
  EXPECT_EQ(section_view.GetItemBelow(item2), nullptr);
  EXPECT_EQ(section_view.GetItemBelow(item3), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item2), item1);
  EXPECT_EQ(section_view.GetItemLeftOf(item3), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item1), item2);
  EXPECT_EQ(section_view.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item3), item2);
}

TEST_F(QuickInsertSectionViewTest, GetItemsFromListAboveImageGridItems) {
  QuickInsertSectionView section_view(kDefaultSectionWidth,
                                      /*asset_fetcher=*/nullptr,
                                      /*submenu_controller=*/nullptr);
  views::View* item1 = section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  views::View* item2 = section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  views::View* item3 = section_view.AddImageGridItem(CreateImageItem());
  views::View* item4 = section_view.AddImageGridItem(CreateImageItem());
  views::View* item5 = section_view.AddImageGridItem(CreateImageItem());

  EXPECT_EQ(section_view.GetTopItem(), item1);
  EXPECT_EQ(section_view.GetBottomItem(), item5);
  EXPECT_EQ(section_view.GetItemAbove(item1), nullptr);
  EXPECT_EQ(section_view.GetItemAbove(item2), item1);
  EXPECT_EQ(section_view.GetItemAbove(item3), item2);
  EXPECT_EQ(section_view.GetItemAbove(item4), item2);
  EXPECT_EQ(section_view.GetItemAbove(item5), item3);
  EXPECT_EQ(section_view.GetItemBelow(item1), item2);
  EXPECT_EQ(section_view.GetItemBelow(item2), item3);
  EXPECT_EQ(section_view.GetItemBelow(item3), item5);
  EXPECT_EQ(section_view.GetItemBelow(item4), nullptr);
  EXPECT_EQ(section_view.GetItemBelow(item5), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item2), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item3), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item4), item3);
  EXPECT_EQ(section_view.GetItemLeftOf(item5), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item1), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item3), item4);
  EXPECT_EQ(section_view.GetItemRightOf(item4), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item5), item4);
}

TEST_F(QuickInsertSectionViewTest, GetItemsFromImageGridAboveListItems) {
  QuickInsertSectionView section_view(kDefaultSectionWidth,
                                      /*asset_fetcher=*/nullptr,
                                      /*submenu_controller=*/nullptr);
  views::View* item1 = section_view.AddImageGridItem(CreateImageItem());
  views::View* item2 = section_view.AddImageGridItem(CreateImageItem());
  views::View* item3 = section_view.AddImageGridItem(CreateImageItem());
  views::View* item4 = section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  views::View* item5 = section_view.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_view.GetTopItem(), item1);
  EXPECT_EQ(section_view.GetBottomItem(), item5);
  EXPECT_EQ(section_view.GetItemAbove(item1), nullptr);
  EXPECT_EQ(section_view.GetItemAbove(item2), nullptr);
  EXPECT_EQ(section_view.GetItemAbove(item3), item1);
  EXPECT_EQ(section_view.GetItemAbove(item4), item3);
  EXPECT_EQ(section_view.GetItemAbove(item5), item4);
  EXPECT_EQ(section_view.GetItemBelow(item1), item3);
  EXPECT_EQ(section_view.GetItemBelow(item2), item4);
  EXPECT_EQ(section_view.GetItemBelow(item3), item4);
  EXPECT_EQ(section_view.GetItemBelow(item4), item5);
  EXPECT_EQ(section_view.GetItemBelow(item5), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item2), item1);
  EXPECT_EQ(section_view.GetItemLeftOf(item3), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item4), nullptr);
  EXPECT_EQ(section_view.GetItemLeftOf(item5), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item1), item2);
  EXPECT_EQ(section_view.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item3), item2);
  EXPECT_EQ(section_view.GetItemRightOf(item4), nullptr);
  EXPECT_EQ(section_view.GetItemRightOf(item5), nullptr);
}

TEST_F(QuickInsertSectionViewTest, GetItemsFromListAboveImageRowItems) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  QuickInsertSectionView* section_view = widget->SetContentsView(
      std::make_unique<QuickInsertSectionView>(kDefaultSectionWidth,
                                               /*asset_fetcher=*/nullptr,
                                               /*submenu_controller=*/nullptr));
  views::View* item1 = section_view->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  views::View* item2 = section_view->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  views::View* item3 = section_view->AddImageRowItem(CreateImageItem());
  views::View* item4 = section_view->AddImageRowItem(CreateImageItem());
  views::View* more_items =
      section_view->GetImageRowMoreItemsButtonForTesting();

  EXPECT_EQ(section_view->GetTopItem(), item1);
  EXPECT_EQ(section_view->GetBottomItem(), item3);
  EXPECT_EQ(section_view->GetItemAbove(item1), nullptr);
  EXPECT_EQ(section_view->GetItemAbove(item2), item1);
  EXPECT_EQ(section_view->GetItemAbove(item3), item2);
  EXPECT_EQ(section_view->GetItemAbove(item4), item2);
  EXPECT_EQ(section_view->GetItemAbove(more_items), item2);
  EXPECT_EQ(section_view->GetItemBelow(item1), item2);
  EXPECT_EQ(section_view->GetItemBelow(item2), item3);
  EXPECT_EQ(section_view->GetItemBelow(item3), nullptr);
  EXPECT_EQ(section_view->GetItemBelow(item4), nullptr);
  EXPECT_EQ(section_view->GetItemBelow(more_items), nullptr);
  EXPECT_EQ(section_view->GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(section_view->GetItemLeftOf(item2), nullptr);
  EXPECT_EQ(section_view->GetItemLeftOf(item3), nullptr);
  EXPECT_EQ(section_view->GetItemLeftOf(item4), item3);
  EXPECT_EQ(section_view->GetItemLeftOf(more_items), item4);
  EXPECT_EQ(section_view->GetItemRightOf(item1), nullptr);
  EXPECT_EQ(section_view->GetItemRightOf(item2), nullptr);
  EXPECT_EQ(section_view->GetItemRightOf(item3), item4);
  EXPECT_EQ(section_view->GetItemRightOf(item4), more_items);
  EXPECT_EQ(section_view->GetItemRightOf(more_items), nullptr);
}

TEST_F(QuickInsertSectionViewTest, GetItemsFromImageRowAboveListItems) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  QuickInsertSectionView* section_view = widget->SetContentsView(
      std::make_unique<QuickInsertSectionView>(kDefaultSectionWidth,
                                               /*asset_fetcher=*/nullptr,
                                               /*submenu_controller=*/nullptr));
  views::View* item1 = section_view->AddImageRowItem(CreateImageItem());
  views::View* item2 = section_view->AddImageRowItem(CreateImageItem());
  views::View* more_items =
      section_view->GetImageRowMoreItemsButtonForTesting();
  views::View* item3 = section_view->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  views::View* item4 = section_view->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_view->GetTopItem(), item1);
  EXPECT_EQ(section_view->GetBottomItem(), item4);
  EXPECT_EQ(section_view->GetItemAbove(item1), nullptr);
  EXPECT_EQ(section_view->GetItemAbove(item2), nullptr);
  EXPECT_EQ(section_view->GetItemAbove(more_items), nullptr);
  EXPECT_EQ(section_view->GetItemAbove(item3), item1);
  EXPECT_EQ(section_view->GetItemAbove(item4), item3);
  EXPECT_EQ(section_view->GetItemBelow(item1), item3);
  EXPECT_EQ(section_view->GetItemBelow(item2), item3);
  EXPECT_EQ(section_view->GetItemBelow(more_items), item3);
  EXPECT_EQ(section_view->GetItemBelow(item3), item4);
  EXPECT_EQ(section_view->GetItemBelow(item4), nullptr);
  EXPECT_EQ(section_view->GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(section_view->GetItemLeftOf(item2), item1);
  EXPECT_EQ(section_view->GetItemLeftOf(more_items), item2);
  EXPECT_EQ(section_view->GetItemLeftOf(item3), nullptr);
  EXPECT_EQ(section_view->GetItemLeftOf(item4), nullptr);
  EXPECT_EQ(section_view->GetItemRightOf(item1), item2);
  EXPECT_EQ(section_view->GetItemRightOf(item2), more_items);
  EXPECT_EQ(section_view->GetItemRightOf(more_items), nullptr);
  EXPECT_EQ(section_view->GetItemRightOf(item3), nullptr);
  EXPECT_EQ(section_view->GetItemRightOf(item4), nullptr);
}

}  // namespace
}  // namespace ash
