// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import "chrome/browser/ui/cocoa/apps/app_shim_menu_controller_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#import "ui/base/test/scoped_fake_nswindow_focus.h"

namespace {

class AppShimMenuControllerBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  AppShimMenuControllerBrowserTest(const AppShimMenuControllerBrowserTest&) =
      delete;
  AppShimMenuControllerBrowserTest& operator=(
      const AppShimMenuControllerBrowserTest&) = delete;

 protected:
  // The apps that can be installed and launched by SetUpApps().
  enum AvailableApps { PACKAGED_1 = 0x1, PACKAGED_2 = 0x2, HOSTED = 0x4 };

  AppShimMenuControllerBrowserTest() = default;

  // Start testing apps and wait for them to launch. |flags| is a bitmask of
  // AvailableApps.
  void SetUpApps(int flags) {

    if (flags & PACKAGED_1) {
      ExtensionTestMessageListener listener_1("Launched");
      app_1_ = InstallAndLaunchPlatformApp("minimal_id");
      ASSERT_TRUE(listener_1.WaitUntilSatisfied());
    }

    if (flags & PACKAGED_2) {
      ExtensionTestMessageListener listener_2("Launched");
      app_2_ = InstallAndLaunchPlatformApp("minimal");
      ASSERT_TRUE(listener_2.WaitUntilSatisfied());
    }

    if (flags & HOSTED) {
      hosted_app_ = InstallHostedApp();

      // Explicitly set the launch type to open in a new window.
      extensions::SetLaunchType(profile(), hosted_app_->id(),
                                extensions::LAUNCH_TYPE_WINDOW);
      LaunchHostedApp(hosted_app_);
    }

    initial_menu_item_count_ = [[[NSApp mainMenu] itemArray] count];
  }

  void CheckHasAppMenus(const extensions::Extension* app) const {
    NSArray* item_array = [[NSApp mainMenu] itemArray];
    ASSERT_EQ(initial_menu_item_count_, [item_array count]);

    // The extra app menus are added from the start when NSApp has an activation
    // policy.
    const int kExtraTopLevelItems = 4;
    for (NSUInteger i = 0; i < initial_menu_item_count_ - kExtraTopLevelItems;
         ++i) {
      EXPECT_TRUE([item_array[i] isHidden]);
    }

    NSMenuItem* app_menu =
        item_array[initial_menu_item_count_ - kExtraTopLevelItems];
    EXPECT_EQ(app->id(), base::SysNSStringToUTF8([app_menu title]));
    EXPECT_EQ(app->name(),
              base::SysNSStringToUTF8([[app_menu submenu] title]));
    for (NSUInteger i = initial_menu_item_count_ - kExtraTopLevelItems;
         i < initial_menu_item_count_; ++i) {
      NSMenuItem* menu = item_array[i];
      EXPECT_GT([[menu submenu] numberOfItems], 0);
      EXPECT_FALSE([menu isHidden]);
    }
  }

  void CheckNoAppMenus() const {
    const int kExtraTopLevelItems = 4;
    NSArray* item_array = [[NSApp mainMenu] itemArray];
    EXPECT_EQ(initial_menu_item_count_ - kExtraTopLevelItems,
              [item_array count]);
    for (NSUInteger i = 0; i < initial_menu_item_count_ - kExtraTopLevelItems;
         ++i) {
      EXPECT_FALSE([item_array[i] isHidden]);
    }
  }

  void CheckEditMenu(const extensions::Extension* app) const {
    const int edit_menu_index = initial_menu_item_count_ + 2;

    NSMenuItem* edit_menu = [[NSApp mainMenu] itemArray][edit_menu_index];
    NSMenu* edit_submenu = [edit_menu submenu];
    NSMenuItem* paste_match_style_menu_item =
        [edit_submenu itemWithTag:IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE];
    NSMenuItem* find_menu_item = [edit_submenu itemWithTag:IDC_FIND_MENU];
    if (app->is_hosted_app()) {
      EXPECT_FALSE([paste_match_style_menu_item isHidden]);
      EXPECT_FALSE([find_menu_item isHidden]);
    } else {
      EXPECT_TRUE([paste_match_style_menu_item isHidden]);
      EXPECT_TRUE([find_menu_item isHidden]);
    }
  }

  extensions::AppWindow* FirstWindowForApp(const extensions::Extension* app) {
    extensions::AppWindowRegistry::AppWindowList window_list =
        extensions::AppWindowRegistry::Get(profile())
            ->GetAppWindowsForApp(app->id());
    EXPECT_FALSE(window_list.empty());
    return window_list.front();
  }

  raw_ptr<const extensions::Extension, DanglingUntriaged> app_1_ = nullptr;
  raw_ptr<const extensions::Extension, DanglingUntriaged> app_2_ = nullptr;
  raw_ptr<const extensions::Extension, DanglingUntriaged> hosted_app_ = nullptr;
  NSUInteger initial_menu_item_count_ = 0;
};

// Test that focusing an app window changes the menu bar.
IN_PROC_BROWSER_TEST_F(AppShimMenuControllerBrowserTest,
                       PlatformAppFocusUpdatesMenuBar) {
  SetUpApps(PACKAGED_1 | PACKAGED_2);
  // When an app is focused, all Chrome menu items should be hidden, and a menu
  // item for the app should be added.
  extensions::AppWindow* app_1_app_window = FirstWindowForApp(app_1_);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_1_app_window->GetNativeWindow()
                               .GetNativeNSWindow()];
  CheckHasAppMenus(app_1_);

  // When another app is focused, the menu item for the app should change.
  extensions::AppWindow* app_2_app_window = FirstWindowForApp(app_2_);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_2_app_window->GetNativeWindow()
                               .GetNativeNSWindow()];
  CheckHasAppMenus(app_2_);

  // When a browser window is focused, the menu items for the app should be
  // removed.
  BrowserWindow* chrome_window =
      (*BrowserList::GetInstance()->begin())->window();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:chrome_window->GetNativeWindow()
                               .GetNativeNSWindow()];
  CheckNoAppMenus();

  // When an app window is closed and there are no other app windows, the menu
  // items for the app should be removed.
  app_1_app_window->GetBaseWindow()->Close();
  chrome_window->Close();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_2_app_window->GetNativeWindow()
                               .GetNativeNSWindow()];
  CheckHasAppMenus(app_2_);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidResignMainNotification
                    object:app_2_app_window->GetNativeWindow()
                               .GetNativeNSWindow()];
  app_2_app_window->GetBaseWindow()->Close();
  CheckNoAppMenus();
}

// Test that closing windows without main status do not update the menu.
// Disabled; https://crbug.com/1322740.
IN_PROC_BROWSER_TEST_F(AppShimMenuControllerBrowserTest,
                       DISABLED_ClosingBackgroundWindowLeavesMenuBar) {
  // Start with app1 active.
  SetUpApps(PACKAGED_1);
  extensions::AppWindow* app_1_app_window = FirstWindowForApp(app_1_);

  {
    ui::test::ScopedFakeNSWindowFocus fake_focus;
    [app_1_app_window->GetNativeWindow().GetNativeNSWindow() makeMainWindow];
    CheckHasAppMenus(app_1_);

    // Closing a background window without focusing it should not change menus.
    BrowserWindow* chrome_window =
        (*BrowserList::GetInstance()->begin())->window();
    chrome_window->Close();
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowWillCloseNotification
                      object:chrome_window->GetNativeWindow()
                                 .GetNativeNSWindow()];
    CheckHasAppMenus(app_1_);

    // |fake_focus| going out of scope sends NSWindowWillResignMainNotification.
  }
  app_1_app_window->GetBaseWindow()->Close();
  CheckNoAppMenus();
}

// Test that uninstalling an app restores the main menu.
IN_PROC_BROWSER_TEST_F(AppShimMenuControllerBrowserTest,
                       ExtensionUninstallUpdatesMenuBar) {
  SetUpApps(PACKAGED_1 | PACKAGED_2);

  FirstWindowForApp(app_2_)->GetBaseWindow()->Close();
  (*BrowserList::GetInstance()->begin())->window()->Close();
  NSWindow* app_1_window =
      FirstWindowForApp(app_1_)->GetNativeWindow().GetNativeNSWindow();

  ui::test::ScopedFakeNSWindowFocus fake_focus;
  [app_1_window makeMainWindow];

  CheckHasAppMenus(app_1_);
  extension_service()->UninstallExtension(
      app_1_->id(), extensions::UNINSTALL_REASON_FOR_TESTING,
      nullptr /* nullptr */);

  // OSX will send NSWindowWillResignMainNotification when a main window is
  // closed.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidResignMainNotification
                    object:app_1_window];
  CheckNoAppMenus();
}

}  // namespace
