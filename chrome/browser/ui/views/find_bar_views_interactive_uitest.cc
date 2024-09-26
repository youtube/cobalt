// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/find_result_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/find_in_page/find_notification_details.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/focus_changed_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;
using content::WebContents;
using ui_test_utils::IsViewFocused;

namespace {
const char kSimplePage[] = "/find_in_page/simple.html";
}  // namespace

class FindInPageTest : public InProcessBrowserTest {
 public:
  FindInPageTest() {
    FindBarHost::disable_animations_during_testing_ = true;
  }

  FindInPageTest(const FindInPageTest&) = delete;
  FindInPageTest& operator=(const FindInPageTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Some bots are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  FindBarHost* GetFindBarHost() {
    FindBar* find_bar = browser()->GetFindBarController()->find_bar();
    return static_cast<FindBarHost*>(find_bar);
  }

  FindBarView* GetFindBarView() { return GetFindBarHost()->find_bar_view(); }

  std::u16string GetFindBarText() { return GetFindBarHost()->GetFindText(); }

  std::u16string GetFindBarSelectedText() {
    return GetFindBarHost()->GetFindBarTesting()->GetFindSelectedText();
  }

  bool IsFindBarVisible() { return GetFindBarHost()->IsFindBarVisible(); }

  void ClickOnView(views::View* view) {
    // EventGenerator and ui_test_utils can't target the find bar (on Windows).
    view->OnMousePressed(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                        gfx::Point(), base::TimeTicks(),
                                        ui::EF_LEFT_MOUSE_BUTTON,
                                        ui::EF_LEFT_MOUSE_BUTTON));
    view->OnMouseReleased(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                         gfx::Point(), base::TimeTicks(),
                                         ui::EF_LEFT_MOUSE_BUTTON,
                                         ui::EF_LEFT_MOUSE_BUTTON));
  }

  void TapOnView(views::View* view) {
    // EventGenerator and ui_test_utils can't target the find bar (on Windows).
    ui::GestureEvent event(0, 0, 0, base::TimeTicks(),
                           ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    view->OnGestureEvent(&event);
  }

  find_in_page::FindNotificationDetails WaitForFindResult() {
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ui_test_utils::FindResultWaiter(web_contents).Wait();
    return find_in_page::FindTabHelper::FromWebContents(web_contents)
        ->find_result();
  }

  find_in_page::FindNotificationDetails WaitForFinalFindResult() {
    while (true) {
      auto details = WaitForFindResult();
      if (details.final_update())
        return details;
    }
  }
};

// Flaky because the test server fails to start? See: http://crbug.com/96594.
IN_PROC_BROWSER_TEST_F(FindInPageTest, CrashEscHandlers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our test page (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::Find(browser());

  // Open another tab (tab B).
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);

  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Select tab A.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  // Close tab B.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);

  // Click on the location bar so that Find box loses focus.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                     VIEW_ID_OMNIBOX));
  // Check the location bar is focused.
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // This used to crash until bug 1303709 was fixed.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, NavigationByKeyEvent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), u"a", true, false,
      nullptr, nullptr);

  // The previous button should still be focused after pressing [Enter] on it.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));

  // The next button should still be focused after pressing [Enter] on it.
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), u"b", true, false,
      nullptr, nullptr);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, ButtonsDoNotAlterFocus) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  const int match_count = ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), u"e", true, false,
      nullptr, nullptr);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // This test requires at least 3 possible matches.
  ASSERT_GE(match_count, 3);
  // Avoid GetViewByID on BrowserView; the find bar is outside its hierarchy.
  FindBarView* find_bar_view = GetFindBarView();
  views::View* next_button =
      find_bar_view->GetViewByID(VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON);
  views::View* previous_button =
      find_bar_view->GetViewByID(VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON);

  // Clicking the next and previous buttons should not alter the focused view.
  ClickOnView(next_button);
  EXPECT_EQ(2, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ClickOnView(previous_button);
  EXPECT_EQ(1, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Tapping the next and previous buttons should not alter the focused view.
  TapOnView(next_button);
  EXPECT_EQ(2, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  TapOnView(previous_button);
  EXPECT_EQ(1, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // The same should be true even when the previous button is focused.
  previous_button->RequestFocus();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));
  ClickOnView(next_button);
  EXPECT_EQ(2, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));
  TapOnView(next_button);
  EXPECT_EQ(3, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, ButtonsDisabledWithoutText) {
  if (browser()
          ->GetFindBarController()
          ->find_bar()
          ->HasGlobalFindPasteboard()) {
    // The presence of a global find pasteboard does not guarantee the find bar
    // will be empty on launch.
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // The buttons should be disabled as there is no text entered in the find bar
  // and no search has been issued yet.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_CLOSE_BUTTON));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, FocusRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Focus the location bar, open and close the find-in-page, focus should
  // return to the location bar.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  // Ensure the creation of the find bar controller.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Focus the location bar, find something on the page, close the find box,
  // focus should go to the page.
  chrome::FocusLocationBar(browser());
  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), u"a", true, false,
      nullptr, nullptr);
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Focus the location bar, open and close the find box, focus should return to
  // the location bar (same as before, just checking that http://crbug.com/23599
  // is fixed).
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));
}

// Flaky on Windows. https://crbug.com/792313
#if BUILDFLAG(IS_WIN)
#define MAYBE_SelectionRestoreOnTabSwitch DISABLED_SelectionRestoreOnTabSwitch
#else
#define MAYBE_SelectionRestoreOnTabSwitch SelectionRestoreOnTabSwitch
#endif
IN_PROC_BROWSER_TEST_F(FindInPageTest, MAYBE_SelectionRestoreOnTabSwitch) {
#if BUILDFLAG(IS_MAC)
  // Mac intentionally changes selection on focus.
  GTEST_SKIP() << "Mac intentionally has different behavior";
#endif
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page in the current tab (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "abc".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_B, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_C, false, false, false, false));
  EXPECT_EQ(u"abc", GetFindBarText());

  // Select "bc".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_LEFT, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_LEFT, false, true, false, false));
  EXPECT_EQ(u"bc", GetFindBarSelectedText());

  // Open another tab (tab B).
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "def".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_D, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_E, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, false, false, false));
  EXPECT_EQ(u"def", GetFindBarText());

  // Select "de".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_HOME, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RIGHT, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RIGHT, false, true, false, false));
  EXPECT_EQ(u"de", GetFindBarSelectedText());

  // Select tab A. Find bar should select "bc".
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_EQ(u"bc", GetFindBarSelectedText());

  // Select tab B. Find bar should select "de".
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_EQ(u"de", GetFindBarSelectedText());
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, FocusRestoreOnTabSwitch) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our test page (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Search for 'a'.
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), u"a", true, false,
      nullptr, nullptr);
  EXPECT_EQ(u"a", GetFindBarSelectedText());

  // Open another tab (tab B).
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Make sure Find box is open.
  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Search for 'b'.
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), u"b", true, false,
      nullptr, nullptr);
  EXPECT_EQ(u"b", GetFindBarSelectedText());

  // Set focus away from the Find bar (to the Location bar).
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Select tab A. Find bar should get focus.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_EQ(u"a", GetFindBarSelectedText());

  // Select tab B. Location bar should get focus.
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_OMNIBOX));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, FocusRestoreOnTabSwitchDismiss) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our test page (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Make sure Find box is not open when starting the new tab.
  EXPECT_FALSE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Select tab A. Find bar should get focus.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Dismiss the Find box. Focus should go to the content view.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  // Wait until the focus settles.
  content::RunUntilInputProcessed(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetRenderWidgetHostView()
                                      ->GetRenderWidgetHost());
  ASSERT_FALSE(IsFindBarVisible());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// FindInPage on Mac doesn't use prepopulated values. Search there is global.
#if !BUILDFLAG(IS_MAC) && !defined(USE_AURA)
// Flaky because the test server fails to start? See: http://crbug.com/96594.
// This tests that whenever you clear values from the Find box and close it that
// it respects that and doesn't show you the last search, as reported in bug:
// http://crbug.com/40121. For Aura see bug http://crbug.com/292299.
IN_PROC_BROWSER_TEST_F(FindInPageTest, PrepopulateRespectBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));

  // We should find "a" here.
  EXPECT_EQ(u"a", GetFindBarText());

  // Delete "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_BACK, false, false, false, false));

  // Validate we have cleared the text.
  EXPECT_EQ(std::u16string(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // After the Find box has been reopened, it should not have been prepopulated
  // with "a" again.
  EXPECT_EQ(std::u16string(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  // Press F3 to trigger FindNext.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F3, false, false, false, false));

  // After the Find box has been reopened, it should still have no prepopulate
  // value.
  EXPECT_EQ(std::u16string(), GetFindBarText());
}
#endif

// Flaky on Win. http://crbug.com/92467
// Flaky on ChromeOS. http://crbug.com/118216
// Flaky on linux aura. http://crbug.com/163931
IN_PROC_BROWSER_TEST_F(FindInPageTest, DISABLED_PasteWithoutTextChange) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Search for "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));

  // We should find "a" here.
  EXPECT_EQ(u"a", GetFindBarText());

  // Reload the page to clear the matching result.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);

  // Focus the Find bar again to make sure the text is selected.
  browser()->GetFindBarController()->Show();

  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // "a" should be selected.
  EXPECT_EQ(u"a", GetFindBarSelectedText());

  // Press Ctrl-C to copy the content.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_C, true, false, false, false));

  std::u16string str;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &str);

  // Make sure the text is copied successfully.
  EXPECT_EQ(u"a", str);

  // Press Ctrl-V to paste the content back, it should start finding even if the
  // content is not changed.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_V, true, false, false, false));
  find_in_page::FindNotificationDetails details = WaitForFindResult();
  EXPECT_TRUE(details.number_of_matches() > 0);
}

// Slow flakiness on Linux. crbug.com/803743
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CtrlEnter DISABLED_CtrlEnter
#else
#define MAYBE_CtrlEnter CtrlEnter
#endif
IN_PROC_BROWSER_TEST_F(FindInPageTest, MAYBE_CtrlEnter) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,This is some text with a "
                      "<a href=\"about:blank\">link</a>.")));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* host = web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost();

  browser()->GetFindBarController()->Show();

  // Search for "link".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_L, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_I, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_N, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_K, false, false, false, false));
  content::RunUntilInputProcessed(host);

  EXPECT_EQ(u"link", GetFindBarText());

  ui_test_utils::UrlLoadObserver observer(
      GURL("about:blank"), content::NotificationService::AllSources());

  // Send Ctrl-Enter, should cause navigation to about:blank.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, true, false, false, false));

  observer.Wait();
}

// This tests the following bug that used to exist:
// 1) Do a find that has 0 results. The search text must contain a space.
// 2) Navigate to a new page (on the same domain) that contains the search text.
// 3) Open the find bar. It should display 0/N (where N is the number of
// matches) and have no active-match highlighting. The bug caused it to display
// 1/N, with the first having active-match highlighting (and the page wouldn't
// scroll to the match if it was off-screen).
IN_PROC_BROWSER_TEST_F(FindInPageTest, ActiveMatchAfterNoResults) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/find_in_page/simple.html")));

  // This bug does not reproduce when using ui_test_utils::FindInPage here;
  // sending keystrokes like this is required. Also note that the text must
  // contain a space.
  browser()->GetFindBarController()->Show();
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_SPACE, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_L, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_I, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_N, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_K, false, false, false, false));
  EXPECT_EQ(u"a link", GetFindBarText());

  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/find_in_page/link.html")));

  browser()->GetFindBarController()->Show();
  auto details = WaitForFindResult();
  EXPECT_EQ(1, details.number_of_matches());
  EXPECT_EQ(0, details.active_match_ordinal());
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, SelectionDuringFind) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/find_in_page/find_from_selection.html")));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* host_view = web_contents->GetRenderWidgetHostView();
  auto* host = host_view->GetRenderWidgetHost();

  content::FocusChangedObserver observer(web_contents);

  // Tab to the input (which selects the text inside)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));

  observer.Wait();

  auto* find_bar_controller = browser()->GetFindBarController();
  find_bar_controller->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Verify the text matches the selection
  EXPECT_EQ(u"text", GetFindBarText());
  find_in_page::FindNotificationDetails details = WaitForFindResult();
  // We don't ever want the page to (potentially) scroll just from opening the
  // find bar, so the active match should always be 0 at this point.
  // See http://crbug.com/1043550
  EXPECT_EQ(0, details.active_match_ordinal());
  EXPECT_EQ(5, details.number_of_matches());

  // Make sure pressing an arrow key doesn't result in a find request.
  // See https://crbug.com/1127666
  auto* helper = find_in_page::FindTabHelper::FromWebContents(web_contents);
  int find_request_id = helper->current_find_request_id();
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_LEFT, false,
                                              false, false, false));
  content::RunUntilInputProcessed(host);
  EXPECT_EQ(find_request_id, helper->current_find_request_id());

  // Make sure calling Show while the findbar is already showing doesn't result
  // in a find request. It's wasted work, could cause some flicker in the
  // results, and was previously triggering another bug that caused an endless
  // loop of searching and flickering results. See http://crbug.com/1129756
  find_bar_controller->Show(false /*find_next*/);
  EXPECT_EQ(find_request_id, helper->current_find_request_id());

  // Find the next match and verify the correct match is highlighted (the
  // one after text that was selected).
  find_bar_controller->Show(true /*find_next*/);
  details = WaitForFindResult();
  EXPECT_EQ(3, details.active_match_ordinal());
  EXPECT_EQ(5, details.number_of_matches());

  // Start a new find without a selection and verify we still get find results.
  // See https://crbug.com/1124605
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  // Wait until the focus settles.
  content::RunUntilInputProcessed(host);

  // Shift-tab back to the input box, then clear the text (and selection).
  // Doing it this way in part because there's a bug with non-input-based
  // selection changes not affecting GetSelectedText().
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DELETE, false,
                                              false, false, false));
  content::RunUntilInputProcessed(host);
  EXPECT_EQ(std::u16string(), host_view->GetSelectedText());

  find_bar_controller->Show();
  details = WaitForFindResult();
  EXPECT_EQ(0, details.active_match_ordinal());
  // One less than before because we deleted the text in the input box.
  EXPECT_EQ(4, details.number_of_matches());
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, GlobalEscapeClosesFind) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));

  // Open find.
  browser()->GetFindBarController()->Show(false, true);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Put focus to the bookmarks' toolbar, which won't consume the escape key.
  chrome::FocusBookmarksToolbar(browser());

  // Close find with escape.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  // Find should be closed.
  EXPECT_FALSE(IsFindBarVisible());
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, ConsumedGlobalEscapeDoesNotCloseFind) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));

  // Open find.
  browser()->GetFindBarController()->Show(false, true);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Put focus into location bar, which will consume escape presses.
  chrome::FocusLocationBar(browser());

  // Press escape.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  // Find should not be closed; the escape should have been consumed by the
  // location bar.
  EXPECT_TRUE(IsFindBarVisible());
}

// See http://crbug.com/1142027
IN_PROC_BROWSER_TEST_F(FindInPageTest, MatchOrdinalStableWhileTyping) {
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("data:text/html,foo foo foo")));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  ui_test_utils::FindResultWaiter waiter1(web_contents, 1 /*request_offset*/);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, false, false, false));
  waiter1.Wait();
  EXPECT_EQ(1, waiter1.active_match_ordinal());
  EXPECT_EQ(3, waiter1.number_of_matches());

  ui_test_utils::FindResultWaiter waiter2(web_contents, 1 /*request_offset*/);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_O, false, false, false, false));
  waiter2.Wait();
  EXPECT_EQ(1, waiter2.active_match_ordinal());
  EXPECT_EQ(3, waiter2.number_of_matches());
}
