// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"

namespace {

using TabletModeBrowserTest = InProcessBrowserTest;

// Smoke test for tablet mode. Ensures lacros does not crash when entering and
// exiting tablet mode.
// TODO(https://crbug.com/1157314): This test is not safe to run in parallel
// with other lacros tests as tablet mode applies to all processes.
IN_PROC_BROWSER_TEST_F(TabletModeBrowserTest, Smoke) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::TestController>());
  // This test requires the tablet mode API.
  if (lacros_service->GetInterfaceVersion<crosapi::mojom::TestController>() <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kEnterTabletModeMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Wait for the window to be visible.
  aura::Window* main_window = browser()->window()->GetNativeWindow();
  std::string main_id = lacros_window_utility::GetRootWindowUniqueId(
      main_window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(main_id));

  // Create an incognito window and make it visible.
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
  AddBlankTabAndShow(incognito_browser);
  aura::Window* incognito_window =
      incognito_browser->window()->GetNativeWindow();
  std::string incognito_id = lacros_window_utility::GetRootWindowUniqueId(
      incognito_window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(incognito_id));

  auto& test_controller =
      lacros_service->GetRemote<crosapi::mojom::TestController>();

  // Enter tablet mode.
  base::test::TestFuture<void> future;
  test_controller->EnterTabletMode(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  future.Clear();

  // Close the incognito window by closing all tabs and wait for it to stop
  // existing in ash.
  incognito_browser->tab_strip_model()->CloseAllTabs();
  ASSERT_TRUE(browser_test_util::WaitForWindowDestruction(incognito_id));

  // Exit tablet mode.
  test_controller->ExitTabletMode(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

}  // namespace
