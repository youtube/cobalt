// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ASH_REQUIRES_LACROS_BROWSERTESTBASE_H_
#define CHROME_BROWSER_ASH_CROSAPI_ASH_REQUIRES_LACROS_BROWSERTESTBASE_H_

#include "chrome/test/base/in_process_browser_test.h"

#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"

class Profile;

namespace crosapi {

// Base class for Ash browser tests that depend on Lacros and use
// StandaloneBrowserTestController.
class AshRequiresLacrosBrowserTestBase : public InProcessBrowserTest {
 public:
  AshRequiresLacrosBrowserTestBase();
  ~AshRequiresLacrosBrowserTestBase() override;

  // Gets user profile in ash.
  Profile* GetAshProfile() const;

 protected:
  void SetUpInProcessBrowserTestFixture() override;

  // Waits for Lacros to start, and for the StandaloneBrowserTestController to
  // connect.
  void SetUpOnMainThread() override;

  // Returns whether the --lacros-chrome-path is provided.
  // If returns false, we should not do any Lacros related testing
  // because the Lacros instance is not provided.
  bool HasLacrosArgument() const { return ash_starter_.HasLacrosArgument(); }

  // Controller to send commands to the connected Lacros crosapi client.
  mojom::StandaloneBrowserTestController* GetStandaloneBrowserTestController();

 private:
  test::AshBrowserTestStarter ash_starter_;
  std::unique_ptr<crosapi::TestControllerAsh> test_controller_ash_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ASH_REQUIRES_LACROS_BROWSERTESTBASE_H_
