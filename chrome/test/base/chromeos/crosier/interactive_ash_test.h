// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_INTERACTIVE_ASH_TEST_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_INTERACTIVE_ASH_TEST_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interaction_sequence.h"

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"
#endif

class GURL;
class Profile;

namespace base {
class CommandLine;
}

namespace content {
class NavigationHandle;
}

// Base class for tests of ash-chrome integration with the ChromeOS platform,
// like hardware daemons, graphics, kernel, etc.
//
// Sets up Kombucha for ash testing:
// - Provides 1 Kombucha "context" per display, shared by all views::Widgets
// - Provides a default "context widget" so Kombucha can synthesize mouse events
// - Suppresses creating a browser window on startup, because most ash-chrome
//   tests don't need the window and creating it slows down the test
//
// Tests using this base class can be added to "chromeos_integration_tests" to
// run on devices under test (DUTs) and virtual machines (VMs). Also, if a test
// only communicates with OS daemons via D-Bus then the test can also run in the
// linux-chromeos "emulator" in "interactive_ui_tests". The latter approach
// makes it simpler to write the initial version of a test, which can then be
// added to "chromeos_integration_tests" to also run on DUT/VM.
//
// Because this class derives from InProcessBrowserTest the source files must be
// added to a target that defines HAS_OUT_OF_PROC_TEST_RUNNER. The source files
// cannot be in a shared test support target that lacks that define.
class InteractiveAshTest
    : public InteractiveBrowserTestT<MixinBasedInProcessBrowserTest> {
 public:
  InteractiveAshTest();
  InteractiveAshTest(const InteractiveAshTest&) = delete;
  InteractiveAshTest& operator=(const InteractiveAshTest&) = delete;
  ~InteractiveAshTest() override;

  // Sets up a context widget for Kombucha. Call this at the start of each test
  // body. This is needed because InteractiveAshTest doesn't open a browser
  // window by default, but Kombucha needs a widget to simulate mouse events.
  void SetupContextWidget();

  // Installs system web apps (SWAs) like OS Settings, Files, etc. Can be called
  // in SetUpOnMainThread() or in your test body. SWAs are not installed by
  // default because this speeds up tests that don't need the apps.
  void InstallSystemApps();

  // Returns the active user profile.
  Profile* GetActiveUserProfile();

  // Convenience method to create a new browser window at `url` for the active
  // user profile. Returns the `NavigationHandle` for the started navigation,
  // which might be null if the navigation couldn't be started. Tests requiring
  // more complex browser setup should use `Navigate()` from
  // browser_navigator.h.
  base::WeakPtr<content::NavigationHandle> CreateBrowserWindow(const GURL& url);

  // Sets up the command line and environment variables to support Lacros (by
  // enabling the Wayland server in ash). Call this from SetUpCommandLine() if
  // your test starts Lacros.
  void SetUpCommandLineForLacros(base::CommandLine* command_line);

  // Waits for Ash to be ready for Lacros, including starting the "Exo" Wayland
  // server. Call this method if your test starts Lacros, otherwise Exo may not
  // be ready and Lacros may not start.
  // TODO(http://b/297930282): Ensure we compile ToT Lacros and use it when
  // testing ToT ash. The rootfs Lacros may be too old to run with ToT ash.
  void WaitForAshFullyStarted();

  // MixinBasedInProcessBrowserTest:
  void TearDownOnMainThread() override;

  // Blocks until a window exists with the given title. If a matching window
  // already exists the test will resume immediately.
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForWindowWithTitle(
      aura::Env* env,
      std::u16string title);

  // Waits for an element identified by `query` to exist in the DOM of an
  // instrumented WebUI identified by `element_id`.
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForElementExists(
      const ui::ElementIdentifier& element_id,
      const DeepQuery& query);

  // Waits for an element identified by `query` to not exist in the DOM of an
  // instrumented WebUI identified by `element_id`.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  WaitForElementDoesNotExist(const ui::ElementIdentifier& element_id,
                             const DeepQuery& query);

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  ChromeOSIntegrationLoginMixin& login_mixin() { return login_mixin_; }
#endif

  // Waits until the element or any of its children have the requested text.
  //
  // element_id
  //     The identifier of the WebContents to query.
  //
  // query
  //     The DeepQuery is a path to the element to start with, it can be {} to
  //     query the entire page.
  //
  // expected
  //     The text to search for.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  WaitForElementTextContains(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& query,
      const std::string& expected);

 private:
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  // This test runs on linux-chromeos in interactive_ui_tests and on a DUT in
  // chromeos_integration_tests.
  ChromeOSIntegrationTestMixin chromeos_integration_test_mixin_{&mixin_host_};

  // Login support.
  ChromeOSIntegrationLoginMixin login_mixin_{&mixin_host_};
#endif

  // Directory used by Wayland/Lacros in environment variable XDG_RUNTIME_DIR.
  base::ScopedTempDir scoped_temp_dir_xdg_;
};
#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_INTERACTIVE_ASH_TEST_H_
