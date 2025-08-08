// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#endif

namespace {

class AppMenuBrowserTest : public UiBrowserTest {
 public:
  // UiBrowserTest:
  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;

 protected:
  // Changes the return value of `browser()` as long as the returned object is
  // alive. This resetting behavior is necessary to null `browser_` before its
  // destruction, lest the allocator complain about dangling refs.
  [[nodiscard]] base::AutoReset<raw_ptr<Browser>> SetBrowser(Browser* browser) {
    return base::AutoReset<raw_ptr<Browser>>(&browser_, browser);
  }

  Browser* browser() {
    return browser_ ? browser_.get() : UiBrowserTest::browser();
  }

  BrowserAppMenuButton* menu_button() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->app_menu_button();
  }

 private:
  raw_ptr<Browser> browser_ = nullptr;
  absl::optional<int> command_id_;
};

void AppMenuBrowserTest::ShowUi(const std::string& name) {
  // Include mnemonics in screenshots so that we detect changes to them.
  menu_button()->ShowMenu(views::MenuRunner::SHOULD_SHOW_MNEMONICS);

  if (base::StartsWith(name, "main")) {
    return;
  }

  constexpr auto kSubmenus = base::MakeFixedFlatMap<base::StringPiece, int>({
    // Submenus present in all versions.
    {"history", IDC_RECENT_TABS_MENU}, {"bookmarks", IDC_BOOKMARKS_MENU},
        {"more_tools", IDC_MORE_TOOLS_MENU},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        {"help", IDC_HELP_MENU},
#endif

        // Submenus only present after Chrome Refresh.
        {"passwords_and_autofill", IDC_PASSWORDS_AND_AUTOFILL_MENU},
        {"reading_list", IDC_READING_LIST_MENU},  // Inside the bookmarks menu.
        {"extensions", IDC_EXTENSIONS_SUBMENU},
        {"find_and_edit", IDC_FIND_AND_EDIT_MENU},
        {"save_and_share", IDC_SAVE_AND_SHARE_MENU},
        {"profile_menu_in_app", IDC_PROFILE_MENU_IN_APP_MENU},
        {"signin_not_allowed", IDC_PROFILE_MENU_IN_APP_MENU},
  });
  const auto* const id_entry = kSubmenus.find(name);
  if (id_entry == kSubmenus.end()) {
    ADD_FAILURE() << "Unknown submenu " << name;
    return;
  }
  command_id_ = id_entry->second;
  views::MenuItemView* const menu_root =
      menu_button()->app_menu()->root_menu_item();
  menu_root->GetMenuController()->SelectItemAndOpenSubmenu(
      menu_root->GetMenuItemByID(command_id_.value()));
}

bool AppMenuBrowserTest::VerifyUi() {
  if (!menu_button()->IsMenuShowing()) {
    return false;
  }
  views::MenuItemView* menu_item = menu_button()->app_menu()->root_menu_item();
  if (command_id_.has_value()) {
    menu_item = menu_item->GetMenuItemByID(command_id_.value());
  }
  if (!menu_item->SubmenuIsShowing()) {
    return false;
  }

  const auto* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  return VerifyPixelUi(menu_item->GetSubmenu()->GetScrollViewContainer(),
                       test_info->test_case_name(),
                       test_info->name()) != ui::test::ActionResult::kFailed;
}

void AppMenuBrowserTest::WaitForUserDismissal() {
  base::RunLoop run_loop;

  class CloseWaiter : public AppMenuButtonObserver {
   public:
    explicit CloseWaiter(base::RepeatingClosure quit_closure)
        : quit_closure_(std::move(quit_closure)) {}

    // AppMenuButtonObserver:
    void AppMenuClosed() override { quit_closure_.Run(); }

   private:
    const base::RepeatingClosure quit_closure_;
  } waiter(run_loop.QuitClosure());

  base::ScopedObservation<BrowserAppMenuButton, CloseWaiter> observation(
      &waiter);
  observation.Observe(menu_button());

  run_loop.Run();
}

// Test case for menus that only appear after Chrome Refresh.
class AppMenuBrowserTestRefreshOnly : public AppMenuBrowserTest {
 public:
  AppMenuBrowserTestRefreshOnly() {
    // TODO(pkasting): It would be better if the tests below merely
    // GTEST_SKIP()ed if the appropriate features weren't set, but in local
    // testing that seemed to result in them always being skipped when the
    // default feature state wasn't correct, even when setting the correct state
    // via command-line flags. Probably I was doing something wrong...
    scoped_feature_list_.InitWithFeatures({features::kChromeRefresh2023,
                                           // Needed for the "extensions" test
                                           features::kExtensionsMenuInAppMenu},
                                          {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test shows the app-menu with a closed window added to the
// TabRestoreService. This is a regression test to ensure menu code handles this
// properly (this was triggering a crash in AppMenu where it was trying to make
// use of RecentTabsMenuModelDelegate before created). See
// https://crbug.com/1249741 for more.
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, ShowWithRecentlyClosedWindow) {
  // Create an additional browser, close it, and ensure it is added to the
  // TabRestoreService.
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  TabRestoreServiceLoadWaiter tab_restore_service_load_waiter(
      tab_restore_service);
  tab_restore_service_load_waiter.Wait();
  Browser* second_browser = CreateBrowser(browser()->profile());
  content::WebContents* new_contents = chrome::AddSelectedTabWithURL(
      second_browser,
      ui_test_utils::GetTestUrl(base::FilePath(),
                                base::FilePath().AppendASCII("simple.html")),
      ui::PAGE_TRANSITION_TYPED);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));
  chrome::CloseWindow(second_browser);
  ui_test_utils::WaitForBrowserToClose(second_browser);
  EXPECT_TRUE(base::Contains(tab_restore_service->entries(),
                             sessions::TabRestoreService::WINDOW,
                             &sessions::TabRestoreService::Entry::type));

  // Show the AppMenu.
  menu_button()->ShowMenu(views::MenuRunner::NO_FLAGS);
}

// There should be at least one subtest below for every distinct submenu of the
// app menu; note that the "main" menu also counts as a submenu. More tests are
// needed if a submenu can have distinct appearances that should all be tested,
// e.g. if different profile data alters the menu appearance.

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_main) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly, InvokeUi_main) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_main_tablet_mode) {
  display::Screen::GetScreen()->OverrideTabletStateForTesting(
      display::TabletState::kInTabletMode);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly,
                       InvokeUi_main_tablet_mode) {
  display::Screen::GetScreen()->OverrideTabletStateForTesting(
      display::TabletState::kInTabletMode);
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly, InvokeUi_main_guest) {
// TODO(crbug.com/1427667): ChromeOS specific profile logic still needs to be
// updated, setup this test for a Guest user session with appropriate command
// line switches afterwards.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  auto browser_resetter = SetBrowser(CreateGuestBrowser());
  ShowAndVerifyUi();
#endif
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly, InvokeUi_main_incognito) {
  auto browser_resetter = SetBrowser(CreateIncognitoBrowser());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_history) {
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_bookmarks) {
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_more_tools) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTest, InvokeUi_help) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly,
                       InvokeUi_passwords_and_autofill) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly, InvokeUi_reading_list) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly, InvokeUi_extensions) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly, InvokeUi_find_and_edit) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly, InvokeUi_save_and_share) {
  ShowAndVerifyUi();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly,
                       InvokeUi_profile_menu_in_app) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::SetPrimaryAccount(identity_manager, "user@example.com",
                            signin::ConsentLevel::kSignin);

  // Create an additional profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  profiles::testing::CreateProfileSync(profile_manager, new_path);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppMenuBrowserTestRefreshOnly,
                       InvokeUi_signin_not_allowed) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  ShowAndVerifyUi();
}

#endif
}  // namespace
