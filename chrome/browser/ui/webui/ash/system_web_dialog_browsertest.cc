// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/aura/client/aura_constants.h"
#include "url/gurl.h"

namespace ash {

namespace {

class MockSystemWebDialog : public SystemWebDialogDelegate {
 public:
  explicit MockSystemWebDialog(const char* id = nullptr)
      : SystemWebDialogDelegate(GURL(chrome::kChromeUIInternetConfigDialogURL),
                                std::u16string()) {
    if (id)
      id_ = std::string(id);
  }

  MockSystemWebDialog(const MockSystemWebDialog&) = delete;
  MockSystemWebDialog& operator=(const MockSystemWebDialog&) = delete;

  ~MockSystemWebDialog() override = default;

  const std::string& Id() override { return id_; }
  std::string GetDialogArgs() const override { return std::string(); }

 private:
  std::string id_;
};

}  // namespace

class SystemWebDialogLoginTest : public LoginManagerTest {
 public:
  SystemWebDialogLoginTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(1);
  }

  SystemWebDialogLoginTest(const SystemWebDialogLoginTest&) = delete;
  SystemWebDialogLoginTest& operator=(const SystemWebDialogLoginTest&) = delete;

  ~SystemWebDialogLoginTest() override = default;

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
};

using SystemWebDialogOobeTest = OobeBaseTest;

// Verifies that system dialogs are modal before login (e.g. during OOBE).
IN_PROC_BROWSER_TEST_F(SystemWebDialogOobeTest, ModalTest) {
  auto* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  EXPECT_TRUE(ShellTestApi().IsSystemModalWindowOpen());
}

// Verifies that system dialogs are not modal and always-on-top after login.
IN_PROC_BROWSER_TEST_F(SystemWebDialogLoginTest, NonModalTest) {
  LoginUser(login_mixin_.users()[0].account_id);
  auto* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  EXPECT_FALSE(ShellTestApi().IsSystemModalWindowOpen());
  aura::Window* window_to_test = dialog->dialog_window();
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            window_to_test->GetProperty(aura::client::kZOrderingKey));
}

using SystemWebDialogTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, InstanceTest) {
  const char* kDialogId = "dialog_id";
  SystemWebDialogDelegate* dialog = new MockSystemWebDialog(kDialogId);
  dialog->ShowSystemDialog();
  SystemWebDialogDelegate* found_dialog =
      SystemWebDialogDelegate::FindInstance(kDialogId);
  EXPECT_EQ(dialog, found_dialog);
  // Closing (deleting) the dialog causes a crash in WebDialogView when the main
  // loop is run. TODO(stevenjb): Investigate, fix, and test closing the dialog.
  // https://crbug.com/855344.
}

IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, FontSize) {
  const blink::web_pref::WebPreferences kDefaultPrefs;
  const int kDefaultFontSize = kDefaultPrefs.default_font_size;
  const int kDefaultFixedFontSize = kDefaultPrefs.default_fixed_font_size;

  // Set the browser font sizes to non-default values.
  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  profile_prefs->SetInteger(prefs::kWebKitDefaultFontSize,
                            kDefaultFontSize + 2);
  profile_prefs->SetInteger(prefs::kWebKitDefaultFixedFontSize,
                            kDefaultFixedFontSize + 1);

  // Open a system dialog.
  MockSystemWebDialog* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();

  // Dialog font sizes are still the default values.
  blink::web_pref::WebPreferences dialog_prefs =
      dialog->GetWebUIForTest()->GetWebContents()->GetOrCreateWebPreferences();
  EXPECT_EQ(kDefaultFontSize, dialog_prefs.default_font_size);
  EXPECT_EQ(kDefaultFixedFontSize, dialog_prefs.default_fixed_font_size);
}

IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, PageZoom) {
  // Set the default browser page zoom to 150%.
  double level = blink::PageZoomFactorToZoomLevel(1.5);
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(level);

  // Open a system dialog.
  MockSystemWebDialog* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();

  // Dialog page zoom is still 100%.
  auto* web_contents = dialog->GetWebUIForTest()->GetWebContents();
  double dialog_level = content::HostZoomMap::GetZoomLevel(web_contents);
  EXPECT_TRUE(blink::PageZoomValuesEqual(dialog_level,
                                         blink::PageZoomFactorToZoomLevel(1.0)))
      << dialog_level;
}

}  // namespace ash
