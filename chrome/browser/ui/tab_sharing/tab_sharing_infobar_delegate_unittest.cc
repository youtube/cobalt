// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

using FocusTarget = TabSharingInfoBarDelegate::FocusTarget;

const std::u16string kSharedTabName = u"example.com";
const std::u16string kAppName = u"sharing.com";
const std::u16string kSinkName = u"Living Room TV";

class MockTabSharingUIViews : public TabSharingUI {
 public:
  MockTabSharingUIViews() {}
  MOCK_METHOD1(StartSharing, void(infobars::InfoBar* infobar));
  MOCK_METHOD0(StopSharing, void());

  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override {
    return 0;
  }

  void OnRegionCaptureRectChanged(
      const absl::optional<gfx::Rect>& region_capture_rect) override {}
};

}  // namespace

class TabSharingInfoBarDelegateTest
    : public BrowserWithTestWindowTest,
      public ::testing::WithParamInterface<bool> {
 public:
  struct Preferences {
    std::u16string shared_tab_name;
    std::u16string capturer_name;
    bool shared_tab;
    bool can_share_instead;
    int tab_index = 0;
    absl::optional<FocusTarget> focus_target;
    TabSharingInfoBarDelegate::TabShareType capture_type =
        TabSharingInfoBarDelegate::TabShareType::CAPTURE;
  };

  TabSharingInfoBarDelegateTest()
      : favicons_used_for_switch_to_tab_button_(GetParam()) {}

  infobars::InfoBar* CreateInfobar(const Preferences& prefs) {
    return TabSharingInfoBarDelegate::Create(
        infobars::ContentInfoBarManager::FromWebContents(
            browser()->tab_strip_model()->GetWebContentsAt(prefs.tab_index)),
        prefs.shared_tab_name, prefs.capturer_name, prefs.shared_tab,
        prefs.can_share_instead
            ? TabSharingInfoBarDelegate::ButtonState::ENABLED
            : TabSharingInfoBarDelegate::ButtonState::NOT_SHOWN,
        prefs.focus_target, tab_sharing_mock_ui(), prefs.capture_type,
        favicons_used_for_switch_to_tab_button_);
  }

  ConfirmInfoBarDelegate* CreateDelegate(const Preferences& prefs) {
    infobars::InfoBar* infobar = CreateInfobar(prefs);
    return static_cast<ConfirmInfoBarDelegate*>(infobar->delegate());
  }

  content::WebContents* GetWebContents(int tab) {
    return browser()->tab_strip_model()->GetWebContentsAt(tab);
  }

  content::GlobalRenderFrameHostId GetGlobalId(int tab) {
    auto* const main_frame = GetWebContents(tab)->GetPrimaryMainFrame();
    return main_frame ? main_frame->GetGlobalId()
                      : content::GlobalRenderFrameHostId();
  }

  std::u16string GetExpectedSwitchToMessageForTargetTab(int tab) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
        url_formatter::FormatOriginForSecurityDisplay(
            GetWebContents(tab)
                ->GetPrimaryMainFrame()
                ->GetLastCommittedOrigin(),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  }

  MockTabSharingUIViews* tab_sharing_mock_ui() { return &mock_ui; }

 protected:
  const bool favicons_used_for_switch_to_tab_button_;

 private:
  MockTabSharingUIViews mock_ui;
};

// Templatize test on whether a favicon is expected or not.
INSTANTIATE_TEST_SUITE_P(All, TabSharingInfoBarDelegateTest, testing::Bool());

TEST_P(TabSharingInfoBarDelegateTest, StartSharingOnCancel) {
  AddTab(browser(), GURL("about:blank"));
  infobars::InfoBar* const infobar =
      CreateInfobar({.shared_tab_name = kSharedTabName,
                     .capturer_name = kAppName,
                     .shared_tab = false,
                     .can_share_instead = true});
  ConfirmInfoBarDelegate* delegate =
      static_cast<ConfirmInfoBarDelegate*>(infobar->delegate());
  EXPECT_CALL(*tab_sharing_mock_ui(), StartSharing(infobar)).Times(1);
  EXPECT_FALSE(delegate->Cancel());
}

TEST_P(TabSharingInfoBarDelegateTest, StopSharingOnAccept) {
  AddTab(browser(), GURL("about:blank"));
  ConfirmInfoBarDelegate* const delegate =
      CreateDelegate({.shared_tab_name = kSharedTabName,
                      .capturer_name = kAppName,
                      .shared_tab = false,
                      .can_share_instead = true});
  EXPECT_CALL(*tab_sharing_mock_ui(), StopSharing).Times(1);
  EXPECT_FALSE(delegate->Accept());
}

// Test that the infobar on the shared tab has the correct layout:
// "|icon| Sharing this tab to |app| [Switch to captured]"
TEST_P(TabSharingInfoBarDelegateTest, InfobarOnCapturingTab) {
  AddTab(browser(), GURL("about:blank"));  // Captured; index = 0.
  AddTab(browser(), GURL("about:blank"));  // Capturing; index = 1.

  const ui::ImageModel favicon =
      favicons_used_for_switch_to_tab_button_
          ? ui::ImageModel::FromImage(
                gfx::Image::CreateFrom1xBitmap(favicon::GenerateMonogramFavicon(
                    GURL("https://example.com"), gfx::kFaviconSize,
                    gfx::kFaviconSize)))
          : ui::ImageModel();

  ConfirmInfoBarDelegate* const delegate =
      CreateDelegate({.shared_tab_name = std::u16string(),
                      .capturer_name = kAppName,
                      .shared_tab = true,
                      .can_share_instead = false,
                      .tab_index = 1,
                      .focus_target = FocusTarget{GetGlobalId(0), favicon}});

  EXPECT_STREQ(delegate->GetVectorIcon().name,
               vector_icons::kScreenShareIcon.name);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, kAppName));
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK |
                                        ConfirmInfoBarDelegate::BUTTON_CANCEL);
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_STOP_BUTTON));
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
            GetExpectedSwitchToMessageForTargetTab(1));
  EXPECT_EQ(delegate->GetButtonImage(ConfirmInfoBarDelegate::BUTTON_CANCEL),
            favicon);
  EXPECT_FALSE(delegate->IsCloseable());
}

// Test that the infobar on the shared tab has the correct layout:
// "|icon| Sharing this tab to |app| [Switch to capturer]"
TEST_P(TabSharingInfoBarDelegateTest, InfobarOnCapturedTab) {
  AddTab(browser(), GURL("about:blank"));  // Captured; index = 0.
  AddTab(browser(), GURL("about:blank"));  // Capturing; index = 1.

  const ui::ImageModel favicon =
      favicons_used_for_switch_to_tab_button_
          ? ui::ImageModel::FromImage(
                gfx::Image::CreateFrom1xBitmap(favicon::GenerateMonogramFavicon(
                    GURL("https://example.com"), gfx::kFaviconSize,
                    gfx::kFaviconSize)))
          : ui::ImageModel();

  ConfirmInfoBarDelegate* const delegate =
      CreateDelegate({.shared_tab_name = std::u16string(),
                      .capturer_name = kAppName,
                      .shared_tab = true,
                      .can_share_instead = false,
                      .tab_index = 0,
                      .focus_target = FocusTarget{GetGlobalId(1), favicon}});

  EXPECT_STREQ(delegate->GetVectorIcon().name,
               vector_icons::kScreenShareIcon.name);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, kAppName));
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK |
                                        ConfirmInfoBarDelegate::BUTTON_CANCEL);
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_STOP_BUTTON));
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
            GetExpectedSwitchToMessageForTargetTab(0));
  EXPECT_EQ(delegate->GetButtonImage(ConfirmInfoBarDelegate::BUTTON_CANCEL),
            favicon);
  EXPECT_FALSE(delegate->IsCloseable());
}

// Test that the infobar on another not share tab has the correct layout:
// |icon| Sharing |shared_tab| to |app| [Stop sharing] [Share this tab instead]
TEST_P(TabSharingInfoBarDelegateTest, InfobarOnNotSharedTab) {
  AddTab(browser(), GURL("about:blank"));
  ConfirmInfoBarDelegate* const delegate =
      CreateDelegate({.shared_tab_name = kSharedTabName,
                      .capturer_name = kAppName,
                      .shared_tab = false,
                      .can_share_instead = true});
  EXPECT_STREQ(delegate->GetVectorIcon().name,
               vector_icons::kScreenShareIcon.name);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL,
                kSharedTabName, kAppName));
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK |
                                        ConfirmInfoBarDelegate::BUTTON_CANCEL);
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_STOP_BUTTON));
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
            l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_SHARE_BUTTON));
  EXPECT_FALSE(delegate->IsCloseable());
}

// Test that when dynamic surface switching is not allowed, the infobar only
// has one button (the "Stop" button) on both shared and not shared tabs.
TEST_P(TabSharingInfoBarDelegateTest, InfobarWhenSharingNotAllowed) {
  // Create infobar for shared tab.
  AddTab(browser(), GURL("about:blank"));
  ConfirmInfoBarDelegate* const delegate_shared_tab =
      CreateDelegate({.shared_tab_name = std::u16string(),
                      .capturer_name = kAppName,
                      .shared_tab = true,
                      .can_share_instead = false,
                      .tab_index = 0});
  EXPECT_EQ(delegate_shared_tab->GetButtons(),
            ConfirmInfoBarDelegate::BUTTON_OK);

  // Create infobar for another not shared tab.
  AddTab(browser(), GURL("about:blank"));
  ConfirmInfoBarDelegate* const delegate =
      CreateDelegate({.shared_tab_name = kSharedTabName,
                      .capturer_name = kAppName,
                      .shared_tab = false,
                      .can_share_instead = false,
                      .tab_index = 1});
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK);
}

// Test that if the app preferred self-capture, but the user either chose
// another tab, or chose the current tab but then switched to sharing another,
// then the infobar has the correct layout:
// |icon| Sharing |shared_tab| to |app| [Stop] [STTI] [Qick-nav]
// (Where STTI = share-this-tab-instead, and quick-nav changes the focused tab.)
TEST_P(TabSharingInfoBarDelegateTest,
       InfobarOnCapturingTabIfCapturedAnotherTabButSelfCapturePreferred) {
  AddTab(browser(), GURL("about:blank"));  // Captured; index = 0.
  AddTab(browser(), GURL("about:blank"));  // Capturing; index = 1.

  const ui::ImageModel favicon =
      favicons_used_for_switch_to_tab_button_
          ? ui::ImageModel::FromImage(
                gfx::Image::CreateFrom1xBitmap(favicon::GenerateMonogramFavicon(
                    GURL("https://example.com"), gfx::kFaviconSize,
                    gfx::kFaviconSize)))
          : ui::ImageModel();

  // The key part of this test, is that both `can_share_instead` as well as
  // `focus_target` are set.
  ConfirmInfoBarDelegate* const delegate =
      CreateDelegate({.shared_tab_name = std::u16string(),
                      .capturer_name = kAppName,
                      .shared_tab = true,
                      .can_share_instead = true,
                      .tab_index = 0,
                      .focus_target = FocusTarget{GetGlobalId(1), favicon}});

  EXPECT_STREQ(delegate->GetVectorIcon().name,
               vector_icons::kScreenShareIcon.name);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, kAppName));

  // Correct number of buttons.
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK |
                                        ConfirmInfoBarDelegate::BUTTON_CANCEL |
                                        ConfirmInfoBarDelegate::BUTTON_EXTRA);

  // Validate the [Stop] button.
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_STOP_BUTTON));

  // Validate the [Share this tab instead] button.
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
            l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_SHARE_BUTTON));

  // Validate the [Quick-nav] button.
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_EXTRA),
            GetExpectedSwitchToMessageForTargetTab(0));
  EXPECT_EQ(delegate->GetButtonImage(ConfirmInfoBarDelegate::BUTTON_EXTRA),
            favicon);

  EXPECT_FALSE(delegate->IsCloseable());
}

// Test that multiple infobars can be created on the same tab.
TEST_P(TabSharingInfoBarDelegateTest, MultipleInfobarsOnSameTab) {
  AddTab(browser(), GURL("about:blank"));
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_EQ(infobar_manager->infobar_count(), 0u);
  CreateInfobar({.shared_tab_name = kSharedTabName,
                 .capturer_name = kAppName,
                 .shared_tab = false,
                 .can_share_instead = true});
  EXPECT_EQ(infobar_manager->infobar_count(), 1u);
  CreateInfobar({.shared_tab_name = kSharedTabName,
                 .capturer_name = kAppName,
                 .shared_tab = false,
                 .can_share_instead = true});
  EXPECT_EQ(infobar_manager->infobar_count(), 2u);
}

TEST_P(TabSharingInfoBarDelegateTest, InfobarNotDismissedOnNavigation) {
  AddTab(browser(), GURL("http://foo"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  CreateInfobar({.shared_tab_name = kSharedTabName,
                 .capturer_name = kAppName,
                 .shared_tab = false,
                 .can_share_instead = true});
  EXPECT_EQ(infobar_manager->infobar_count(), 1u);
  NavigateAndCommit(web_contents, GURL("http://bar"));
  EXPECT_EQ(infobar_manager->infobar_count(), 1u);
}

// Test that the infobar on another not cast tab has the correct layout:
// "|icon| Casting |tab_being_cast| to |sink| [Stop casting] [Cast this tab
// instead]"
TEST_P(TabSharingInfoBarDelegateTest, InfobarOnNotCastTab) {
  AddTab(browser(), GURL("about:blank"));
  Preferences preferences = {
      .shared_tab_name = kSharedTabName,
      .capturer_name = kSinkName,
      .shared_tab = false,
      .can_share_instead = true,
      .capture_type = TabSharingInfoBarDelegate::TabShareType::CAST};
  ConfirmInfoBarDelegate* const delegate = CreateDelegate(preferences);
  EXPECT_STREQ(delegate->GetVectorIcon().name,
               vector_icons::kScreenShareIcon.name);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringFUTF16(
                IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_LABEL,
                kSharedTabName, kSinkName));
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK |
                                        ConfirmInfoBarDelegate::BUTTON_CANCEL);
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_TAB_CASTING_INFOBAR_STOP_BUTTON));
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
            l10n_util::GetStringUTF16(IDS_TAB_CASTING_INFOBAR_CAST_BUTTON));
  EXPECT_FALSE(delegate->IsCloseable());

  // Without sink name.
  preferences.capturer_name = std::u16string();
  ConfirmInfoBarDelegate* const delegate2 = CreateDelegate(preferences);
  EXPECT_EQ(
      delegate2->GetMessageText(),
      l10n_util::GetStringFUTF16(
          IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_NO_DEVICE_NAME_LABEL,
          kSharedTabName));
}

// Test that the infobar on the tab being cast has the correct layout:
// "|icon| Casting this tab to |sink| [Stop casting]"
TEST_P(TabSharingInfoBarDelegateTest, InfobarOnCastTab) {
  AddTab(browser(), GURL("about:blank"));
  Preferences preferences = {
      .shared_tab_name = std::u16string(),
      .capturer_name = kSinkName,
      .shared_tab = true,
      .can_share_instead = false,
      .capture_type = TabSharingInfoBarDelegate::TabShareType::CAST};
  ConfirmInfoBarDelegate* const delegate = CreateDelegate(preferences);
  EXPECT_STREQ(delegate->GetVectorIcon().name,
               vector_icons::kScreenShareIcon.name);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringFUTF16(
                IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_LABEL, kSinkName));
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_TAB_CASTING_INFOBAR_STOP_BUTTON));
  EXPECT_FALSE(delegate->IsCloseable());

  // Without sink name.
  preferences.capturer_name = std::u16string();
  ConfirmInfoBarDelegate* const delegate2 = CreateDelegate(preferences);
  EXPECT_EQ(
      delegate2->GetMessageText(),
      l10n_util::GetStringUTF16(
          IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_NO_DEVICE_NAME_LABEL));
}
