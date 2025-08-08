// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/check.h"
#include "build/build_config.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_open_group_helper.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "url/url_constants.h"

namespace tab_groups {
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";

class DataSharingChromeNativeUiTest : public InteractiveBrowserTest {
 protected:
  DataSharingChromeNativeUiTest() = default;
  ~DataSharingChromeNativeUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature,
         tab_groups::kTabGroupsSaveUIUpdate, tab_groups::kTabGroupsSaveV2,
         tab_groups::kTabGroupSyncServiceDesktopMigration},
        {});
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  MultiStep FinishTabstripAnimations() {
    return Steps(
        WaitForShow(kTabStripElementId),
        std::move(WithView(kTabStripElementId, [](TabStrip* tab_strip) {
                    tab_strip->StopAnimating(true);
                  }).SetDescription("FinishTabstripAnimation")));
  }

  MultiStep ShowBookmarksBar() {
    return Steps(PressButton(kToolbarAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
  }

  MultiStep HoverTabAt(int index) {
    const char kTabToHover[] = "Tab to hover";
    return Steps(NameDescendantViewByType<Tab>(kBrowserViewElementId,
                                               kTabToHover, index),
                 MoveMouseTo(kTabToHover));
  }

  MultiStep HoverTabGroupHeader(tab_groups::TabGroupId group_id) {
    const char kTabGroupHeaderToHover[] = "Tab group header to hover";
    return Steps(
        FinishTabstripAnimations(),
        NameDescendantView(
            kBrowserViewElementId, kTabGroupHeaderToHover,
            base::BindRepeating(
                [](tab_groups::TabGroupId group_id, const views::View* view) {
                  const TabGroupHeader* header =
                      views::AsViewClass<TabGroupHeader>(view);
                  if (!header) {
                    return false;
                  }
                  return header->group().value() == group_id;
                },
                group_id)),
        MoveMouseTo(kTabGroupHeaderToHover));
  }

  MultiStep SaveGroupLeaveEditorBubbleOpen(tab_groups::TabGroupId group_id) {
    return Steps(EnsureNotPresent(kTabGroupEditorBubbleId),
                 // Right click on the header to open the editor bubble.
                 HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
                 // Wait for the tab group editor bubble to appear.
                 WaitForShow(kTabGroupEditorBubbleId));
  }

  tab_groups::TabGroupId InstrumentATabGroup() {
    // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
    // open the browser and the added one).
    EXPECT_TRUE(
        AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    return browser()->tab_strip_model()->AddToNewGroup({0, 1});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest, ShowShareBubble) {
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  RunTestSequence(
      FinishTabstripAnimations(), SaveGroupLeaveEditorBubbleOpen(group_id),
      WaitForShow(kTabGroupEditorBubbleShareGroupButtonId),
      PressButton(kTabGroupEditorBubbleShareGroupButtonId),
      WaitForShow(kDataSharingBubbleElementId),
      // Check the share bubble is anchored onto the group header view.
      CheckView(kDataSharingBubbleElementId,
                [&](views::BubbleDialogDelegateView* bubble) {
                  const auto* const browser_view =
                      BrowserView::GetBrowserViewForBrowser(browser());
                  const TabGroupHeader* const group_header =
                      browser_view->tabstrip()->group_header(group_id);
                  return group_header &&
                         bubble->GetAnchorView() == group_header;
                }));
}

// TODO (368057577) This test was disabled after moving to TabGroupSyncService.
// Re-enable this test.
IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest,
                       DISABLED_ShowManageBubble) {
  auto* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_service->GetGroup(group_id);
  std::string fake_collab_id = "fake_collab_id";
  group->SetCollaborationId(fake_collab_id);
  tab_group_service->RemoveGroup(group->saved_guid());
  tab_group_service->AddGroup(group.value());

  RunTestSequence(
      FinishTabstripAnimations(), SaveGroupLeaveEditorBubbleOpen(group_id),
      WaitForShow(kTabGroupEditorBubbleManageSharedGroupButtonId),
      PressButton(kTabGroupEditorBubbleManageSharedGroupButtonId),
      WaitForShow(kDataSharingBubbleElementId),
      CheckView(kDataSharingBubbleElementId, [](views::View* bubble) {
        return bubble->GetWidget()->IsModal();
      }));
}

IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest, ShowJoinBubble) {
  std::string fake_collab_id = "fake_collab_id";
  std::string fake_access_token = "fake_access_token";

  RunTestSequence(
      Do([=, this]() {
        auto share_link = data_sharing::GetShareLink(
            fake_collab_id, fake_access_token, browser()->profile());
        auto* data_sharing_service =
            data_sharing::DataSharingServiceFactory::GetForProfile(
                browser()->profile());
        data_sharing_service->HandleShareURLNavigationIntercepted(share_link,
                                                                  nullptr);
      }),
      WaitForShow(kDataSharingBubbleElementId),
      CheckView(kDataSharingBubbleElementId, [](views::View* bubble) {
        return bubble->GetWidget()->IsModal();
      }));
}

// TODO (368057577) This test was disabled after moving to TabGroupSyncService.
// Re-enable this test.
IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest,
                       DISABLED_GenerateWebUIUrl) {
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  std::string fake_collab_id = "fake_collab_id";
  std::string fake_access_token = "fake_access_token";

  auto expected_share_flow_url =
      GURL(std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
           std::string(data_sharing::kQueryParamFlow) + "=" +
           std::string(data_sharing::kFlowShare) + "&" +
           std::string(data_sharing::kQueryParamTabGroupId) + "=" +
           group_id.ToString());

  auto expected_manage_flow_url = GURL(
      std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
      std::string(data_sharing::kQueryParamFlow) + "=" +
      std::string(data_sharing::kFlowManage) + "&" +
      std::string(data_sharing::kQueryParamGroupId) + "=" + fake_collab_id);

  auto expected_join_flow_url = GURL(
      std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
      std::string(data_sharing::kQueryParamFlow) + "=" +
      std::string(data_sharing::kFlowJoin) + "&" +
      std::string(data_sharing::kQueryParamGroupId) + "=" + fake_collab_id +
      "&" + std::string(data_sharing::kQueryParamTokenSecret) + "=" +
      fake_access_token);

  auto url = data_sharing::GenerateWebUIUrl(group_id, browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_share_flow_url);

  auto* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_service->GetGroup(group_id);
  group->SetCollaborationId(fake_collab_id);
  tab_group_service->RemoveGroup(group->saved_guid());
  tab_group_service->AddGroup(group.value());

  url = data_sharing::GenerateWebUIUrl(group_id, browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_manage_flow_url);

  data_sharing::GroupToken token = data_sharing::GroupToken(
      data_sharing::GroupId(fake_collab_id), fake_access_token);
  url = data_sharing::GenerateWebUIUrl(token, browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_join_flow_url);
}

IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest, OpenGroupHelper) {
  std::string fake_collab_id = "fake_collab_id";
  tab_groups::LocalTabGroupID local_group_id = InstrumentATabGroup();
  auto* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  std::optional<tab_groups::SavedTabGroup> group_copy =
      tab_group_service->GetGroup(local_group_id);

  // Mock up a shared group.
  group_copy->SetCollaborationId(fake_collab_id);

  RunTestSequence(
      SaveGroupLeaveEditorBubbleOpen(local_group_id),
      WaitForShow(kTabGroupHeaderElementId),
      EnsurePresent(kTabGroupEditorBubbleId),
      PressButton(kTabGroupEditorBubbleCloseGroupButtonId),
      FinishTabstripAnimations(), WaitForHide(kTabGroupHeaderElementId),
      Do([=, this]() {
        DataSharingOpenGroupHelper* open_group_helper =
            browser()
                ->browser_window_features()
                ->data_sharing_open_group_helper();
        open_group_helper->OpenTabGroupWhenAvailable(fake_collab_id);
        EXPECT_TRUE(open_group_helper->group_ids_for_testing().contains(
            fake_collab_id));

        // Mock group sync from remote.
        open_group_helper->OnTabGroupAdded(group_copy.value(),
                                           tab_groups::TriggerSource::REMOTE);
        EXPECT_FALSE(open_group_helper->group_ids_for_testing().contains(
            fake_collab_id));
      }),
      // The group is opened into the tab strip.
      WaitForShow(kTabGroupHeaderElementId));
}

using DataSharingChromeNativeUiPixelTest = DataSharingChromeNativeUiTest;

// Take a screenshot of the shared tab group in app menu > tab groups and
// everything menu in the bookmarks bar.
IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiPixelTest,
                       SharedTabGroupInMenus) {
  std::string fake_collab_id = "fake_collab_id";
  tab_groups::LocalTabGroupID local_group_id = InstrumentATabGroup();
  tab_groups::TabGroupSyncServiceImpl* tab_group_service =
      static_cast<tab_groups::TabGroupSyncServiceImpl*>(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              browser()->profile()));

  // Make the group shared.
  tab_group_service->MakeTabGroupSharedForTesting(local_group_id,
                                                  fake_collab_id);

  RunTestSequence(WaitForShow(kTabGroupHeaderElementId),
                  FinishTabstripAnimations(), ShowBookmarksBar(),

                  // Screenshot app menu -> tab groups.
                  PressButton(kToolbarAppMenuButtonElementId),
                  WaitForShow(AppMenuModel::kTabGroupsMenuItem),
                  SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
                  EnsurePresent(tab_groups::STGEverythingMenu::kTabGroup),
                  SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                          kSkipPixelTestsReason),
                  Screenshot(tab_groups::STGEverythingMenu::kTabGroup,
                             "shared_icon_in_app_menu", "5924633"),

                  // Close the app menu.
                  HoverTabAt(0), ClickMouse(),
                  WaitForHide(AppMenuModel::kTabGroupsMenuItem),

                  // Screenshot everything menu.
                  EnsurePresent(kSavedTabGroupOverflowButtonElementId),
                  PressButton(kSavedTabGroupOverflowButtonElementId),
                  EnsurePresent(tab_groups::STGEverythingMenu::kTabGroup),
                  Screenshot(tab_groups::STGEverythingMenu::kTabGroup,
                             "shared_icon_in_everything_menu", "5924633"),

                  // Close the everything menu.
                  HoverTabAt(0), ClickMouse());
}

// Take a screenshot of the shared tab group's TabGroupHeader in the tabstrip.
IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiPixelTest,
                       SharedTabGroupInTabStrip) {
  const char kTabGroupHeaderToScreenshot[] = "Tab group header to hover";

  std::string fake_collab_id = "fake_collab_id";
  tab_groups::LocalTabGroupID local_group_id = InstrumentATabGroup();
  tab_groups::TabGroupSyncServiceImpl* tab_group_service =
      static_cast<tab_groups::TabGroupSyncServiceImpl*>(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              browser()->profile()));

  // Make the group shared.
  tab_group_service->MakeTabGroupSharedForTesting(local_group_id,
                                                  fake_collab_id);

  // Manually call set visual data to repaint the tab group header with the
  // share icon.
  TabGroup* tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(local_group_id);
  tab_group->SetVisualData(*tab_group->visual_data());

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      ShowBookmarksBar(),
      NameDescendantView(
          kBrowserViewElementId, kTabGroupHeaderToScreenshot,
          base::BindRepeating(
              [](tab_groups::TabGroupId group_id, const views::View* view) {
                const TabGroupHeader* header =
                    views::AsViewClass<TabGroupHeader>(view);
                if (!header) {
                  return false;
                }
                return header->group().value() == group_id;
              },
              local_group_id)),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      Screenshot(kTabGroupHeaderToScreenshot, "shared_icon_in_tab_group_header",
                 "5924633"));
}

}  // namespace tab_groups
