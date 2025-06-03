// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "apps/launcher.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"  // nogncheck
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif

using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace app_runtime = extensions::api::app_runtime;

namespace extensions {

namespace {

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
bool ExpectChromeAppsDefaultEnabled() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
  return false;
#else
  return true;
#endif
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Non-abstract RenderViewContextMenu class.
class PlatformAppContextMenu : public RenderViewContextMenu {
 public:
  PlatformAppContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params)
      : RenderViewContextMenu(render_frame_host, params) {}

  bool HasCommandWithId(int command_id) {
    return menu_model_.GetIndexOfCommandId(command_id).has_value();
  }

  void Show() override {}
};

// This class keeps track of tabs as they are added to the browser. It will be
// "done" (i.e. won't block on Wait()) once |observations| tabs have been added.
class TabsAddedNotificationObserver : public TabStripModelObserver {
 public:
  TabsAddedNotificationObserver(Browser* browser, size_t observations)
      : observations_(observations) {
    browser->tab_strip_model()->AddObserver(this);
  }
  TabsAddedNotificationObserver(const TabsAddedNotificationObserver&) = delete;
  TabsAddedNotificationObserver& operator=(
      const TabsAddedNotificationObserver&) = delete;
  ~TabsAddedNotificationObserver() override = default;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted)
      return;

    for (auto& tab : change.GetInsert()->contents)
      observed_tabs_.push_back(tab.contents);

    if (observed_tabs_.size() >= observations_)
      run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  const std::vector<content::WebContents*>& tabs() { return observed_tabs_; }

 private:
  base::RunLoop run_loop_;
  size_t observations_;
  std::vector<content::WebContents*> observed_tabs_;
};

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class ScopedPreviewTestDelegate : printing::PrintPreviewUI::TestDelegate {
 public:
  ScopedPreviewTestDelegate() {
    printing::PrintPreviewUI::SetDelegateForTesting(this);
  }

  ~ScopedPreviewTestDelegate() override {
    printing::PrintPreviewUI::SetDelegateForTesting(nullptr);
  }

  // PrintPreviewUI::TestDelegate implementation.
  void DidGetPreviewPageCount(uint32_t page_count) override {
    total_page_count_ = page_count;
  }

  // PrintPreviewUI::TestDelegate implementation.
  void DidRenderPreviewPage(content::WebContents* preview_dialog) override {
    dialog_size_ = preview_dialog->GetContainerBounds().size();
    ++rendered_page_count_;
    CHECK(rendered_page_count_ <= total_page_count_);
    if (rendered_page_count_ == total_page_count_ && run_loop_) {
      run_loop_->Quit();
    }
  }

  void WaitUntilPreviewIsReady() {
    if (rendered_page_count_ >= total_page_count_)
      return;

    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
    run_loop.Run();
  }

  gfx::Size dialog_size() { return dialog_size_; }

 private:
  uint32_t total_page_count_ = 1;
  uint32_t rendered_page_count_ = 0;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION base::RunLoop* run_loop_ = nullptr;
  gfx::Size dialog_size_;
};

#endif  // ENABLE_PRINT_PREVIEW

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_WIN)
bool CopyTestDataAndGetTestFilePath(const base::FilePath& test_data_file,
                                    const base::FilePath& temp_dir,
                                    const char* filename,
                                    base::FilePath* file_path) {
  base::FilePath path =
      temp_dir.AppendASCII(filename).NormalizePathSeparators();
  if (!(base::CopyFile(test_data_file, path)))
    return false;

  *file_path = path;
  return true;
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_WIN)

class PlatformAppWithFileBrowserTest : public PlatformAppBrowserTest {
 public:
  PlatformAppWithFileBrowserTest()
      : enable_chrome_apps_(
            &extensions::testing::g_enable_chrome_apps_for_testing,
            true) {
    set_open_about_blank_on_browser_launch(false);
  }

 protected:
  bool RunPlatformAppTestWithFileInTestDataDir(
      const std::string& extension_name,
      const std::string& test_file) {
    base::FilePath test_doc(test_data_dir_.AppendASCII(test_file));
    test_doc = test_doc.NormalizePathSeparators();
    return RunPlatformAppTestWithCommandLine(
        extension_name, MakeCommandLineWithTestFilePath(test_doc));
  }

  bool RunPlatformAppTestWithFile(const std::string& extension_name,
                                  const base::FilePath& test_file_path) {
    return RunPlatformAppTestWithCommandLine(
        extension_name, MakeCommandLineWithTestFilePath(test_file_path));
  }

  bool RunPlatformAppTestWithNothing(const std::string& extension_name) {
    return RunPlatformAppTestWithCommandLine(
        extension_name, *base::CommandLine::ForCurrentProcess());
  }

  void RunPlatformAppTestWithFiles(const std::string& extension_name,
                                   const std::string& test_file) {
    extensions::ResultCatcher catcher;

    base::FilePath test_doc(test_data_dir_.AppendASCII(test_file));
    base::FilePath file_path = test_doc.NormalizePathSeparators();

    base::FilePath extension_path = test_data_dir_.AppendASCII(extension_name);
    const extensions::Extension* extension = LoadExtension(extension_path);
    ASSERT_TRUE(extension);

    std::vector<base::FilePath> launch_files;
    launch_files.push_back(file_path);
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->LaunchAppWithFiles(
            extension->id(),
            apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                true /* preferred_container */),
            apps::LaunchSource::kFromTest, std::move(launch_files));
    ASSERT_TRUE(catcher.GetNextResult());
  }

 private:
  bool RunPlatformAppTestWithCommandLine(
      const std::string& extension_name,
      const base::CommandLine& command_line) {
    extensions::ResultCatcher catcher;

    base::FilePath extension_path = test_data_dir_.AppendASCII(extension_name);
    const extensions::Extension* extension = LoadExtension(extension_path);
    if (!extension) {
      message_ = "Failed to load extension.";
      return false;
    }

    apps::AppLaunchParams params(
        extension->id(), apps::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
    params.command_line = command_line;
    params.current_directory = test_data_dir_;
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParamsForTesting(std::move(params));

    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }

    return true;
  }

  base::CommandLine MakeCommandLineWithTestFilePath(
      const base::FilePath& test_file) {
    base::CommandLine command_line = *base::CommandLine::ForCurrentProcess();
    command_line.AppendArgPath(test_file);
    return command_line;
  }

 private:
  base::AutoReset<bool> enable_chrome_apps_;
};

const char kChromiumURL[] = "http://chromium.org";
#if !BUILDFLAG(IS_CHROMEOS_ASH)
const char kTestFilePath[] = "platform_apps/launch_files/test.txt";
#endif

}  // namespace

// Tests that CreateAppWindow doesn't crash if you close it straight away.
// LauncherPlatformAppBrowserTest relies on this behaviour, but is only run for
// ash, so we test that it works here.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, CreateAndCloseAppWindow) {
  const Extension* extension = LoadAndLaunchPlatformApp("minimal", "Launched");
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  CloseAppWindow(window);
}

// Tests that platform apps received the "launch" event when launched.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OnLaunchedEvent) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/launch",
                               {.launch_as_platform_app = true}))
      << message_;
}

// Tests that platform apps cannot use certain disabled window properties, but
// can override them and then use them.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DisabledWindowProperties) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/disabled_window_properties",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, EmptyContextMenu) {
  LoadAndLaunchPlatformApp("minimal", "Launched");

  // The empty app doesn't add any context menu items, so its menu should
  // only include the developer tools.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  auto menu = std::make_unique<PlatformAppContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenu) {
  LoadAndLaunchPlatformApp("context_menu", "Launched");

  // The context_menu app has two context menu items. These, along with a
  // separator and the developer tools, is all that should be in the menu.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  auto menu = std::make_unique<PlatformAppContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();
  int first_extensions_command_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(first_extensions_command_id));
  ASSERT_TRUE(menu->HasCommandWithId(first_extensions_command_id + 1));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, InstalledAppWithContextMenu) {
  ExtensionTestMessageListener launched_listener("Launched");
  InstallAndLaunchPlatformApp("context_menu");

  // Wait for the extension to tell us it's initialized its context menus and
  // launched a window.
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // The context_menu app has two context menu items. For an installed app
  // these are all that should be in the menu.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  auto menu = std::make_unique<PlatformAppContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();
  int extensions_custom_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id));
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id + 1));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_FALSE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
}

// Flaky on Mac10.13 Tests (dbg). See https://crbug.com/1155013
#if BUILDFLAG(IS_MAC)
#define MAYBE_AppWithContextMenuTextField DISABLED_AppWithContextMenuTextField
#else
#define MAYBE_AppWithContextMenuTextField AppWithContextMenuTextField
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_AppWithContextMenuTextField) {
  LoadAndLaunchPlatformApp("context_menu", "Launched");

  // The context_menu app has one context menu item. This, along with a
  // separator and the developer tools, is all that should be in the menu.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  params.is_editable = true;
  auto menu = std::make_unique<PlatformAppContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();
  int extensions_custom_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPY));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenuSelection) {
  LoadAndLaunchPlatformApp("context_menu", "Launched");

  // The context_menu app has one context menu item. This, along with a
  // separator and the developer tools, is all that should be in the menu.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  params.selection_text = u"Hello World";
  auto menu = std::make_unique<PlatformAppContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();
  int extensions_custom_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPY));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenuClicked) {
  LoadAndLaunchPlatformApp("context_menu_click", "Launched");

  // Test that the menu item shows up
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.bar");
  auto menu = std::make_unique<PlatformAppContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();
  int extensions_custom_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id));

  // Execute the menu item
  ExtensionTestMessageListener onclicked_listener("onClicked fired for id1");
  menu->ExecuteCommand(extensions_custom_id, 0);

  ASSERT_TRUE(onclicked_listener.WaitUntilSatisfied());
}

// https://crbug.com/1155013
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DISABLED_DisallowNavigation) {
  TabsAddedNotificationObserver observer(browser(), 1);

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("platform_apps/navigation",
                               {.launch_as_platform_app = true}))
      << message_;

  observer.Wait();
  ASSERT_EQ(1U, observer.tabs().size());
  EXPECT_EQ(GURL(kChromiumURL), observer.tabs()[0]->GetURL());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       DisallowBackgroundPageNavigation) {
  // The test will try to open in app urls and external urls via clicking links
  // and window.open(). Only the external urls should succeed in opening tabs.
  const size_t kExpectedNumberOfTabs = 2u;
  TabsAddedNotificationObserver observer(browser(), kExpectedNumberOfTabs);
  ASSERT_TRUE(RunExtensionTest("platform_apps/background_page_navigation",
                               {.launch_as_platform_app = true}))
      << message_;
  observer.Wait();
  ASSERT_EQ(kExpectedNumberOfTabs, observer.tabs().size());
  EXPECT_FALSE(
      content::WaitForLoadStop(observer.tabs()[kExpectedNumberOfTabs - 1]));
  EXPECT_EQ(GURL(kChromiumURL),
            observer.tabs()[kExpectedNumberOfTabs - 1]->GetURL());
  EXPECT_FALSE(
      content::WaitForLoadStop(observer.tabs()[kExpectedNumberOfTabs - 2]));
  EXPECT_EQ(GURL(kChromiumURL),
            observer.tabs()[kExpectedNumberOfTabs - 2]->GetURL());
}

// Failing on some Win and Linux buildbots.  See crbug.com/354425.
// TODO(crbug.com/1334427): Fix flakiness on macOS and re-enable this test.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_Iframes DISABLED_Iframes
#else
#define MAYBE_Iframes Iframes
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_Iframes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("platform_apps/iframes",
                               {.launch_as_platform_app = true}))
      << message_;
}

// Tests that platform apps can perform filesystem: URL navigations.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AllowFileSystemURLNavigation) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kFileSystemUrlNavigationForChromeAppsOnly)) {
    GTEST_SKIP();
  }
  ASSERT_TRUE(RunExtensionTest("platform_apps/filesystem_url",
                               {.launch_as_platform_app = true}))
      << message_;
}

// Tests that localStorage and WebSQL are disabled for platform apps.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DisallowStorage) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/storage",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, Restrictions) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/restrictions",
                               {.launch_as_platform_app = true}))
      << message_;
}

// Tests that extensions can't use platform-app-only APIs.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, PlatformAppsOnly) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/apps_only", {},
                               {.ignore_manifest_warnings = true}))
      << message_;
}

// TODO(https://crbug.com/1246088): Flaky.
// Tests that platform apps have isolated storage by default.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DISABLED_Isolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load a (non-app) page under the "localhost" origin that sets a cookie.
  GURL set_cookie_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/isolation/set_cookie.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  set_cookie_url = set_cookie_url.ReplaceComponents(replace_host);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), set_cookie_url));

  // Make sure the cookie is set.
  int cookie_size;
  std::string cookie_value;
  ui_test_utils::GetCookies(set_cookie_url,
                            browser()->tab_strip_model()->GetWebContentsAt(0),
                            &cookie_size, &cookie_value);
  ASSERT_EQ("testCookie=1", cookie_value);

  // Let the platform app request the same URL, and make sure that it doesn't
  // see the cookie.
  ASSERT_TRUE(RunExtensionTest("platform_apps/isolation",
                               {.launch_as_platform_app = true}))
      << message_;
}

// See crbug.com/248441
#if BUILDFLAG(IS_WIN)
#define MAYBE_ExtensionWindowingApis DISABLED_ExtensionWindowingApis
#else
#define MAYBE_ExtensionWindowingApis ExtensionWindowingApis
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_ExtensionWindowingApis) {
  // Initially there should be just the one browser window visible to the
  // extensions API.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("common/background_page"));
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(extension));

  // And no app windows.
  ASSERT_EQ(0U, GetAppWindowCount());

  // Launch a platform app that shows a window.
  LoadAndLaunchPlatformApp("minimal", "Launched");
  ASSERT_EQ(1U, GetAppWindowCount());
  int app_window_id = GetFirstAppWindow()->session_id().id();

  // But it's not visible to the extensions API, it still thinks there's just
  // one browser window.
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(extension));
  // It can't look it up by ID either
  ASSERT_FALSE(RunGetWindowFunctionForExtension(app_window_id, extension));

  // The app can also only see one window (its own).
  // TODO(jeremya): add an extension function to get an app window by ID, and
  // to get a list of all the app windows, so we can test this.

  // Launch another platform app that also shows a window.
  LoadAndLaunchPlatformApp("context_menu", "Launched");

  // There are two total app windows, but each app can only see its own.
  ASSERT_EQ(2U, GetAppWindowCount());
  // TODO(jeremya): as above, this requires more extension functions.
}

// ChromeOS does not support passing arguments on the command line, so the tests
// that rely on this functionality are disabled.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Tests that launch data is sent through if the file extension matches.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchFilesWithFileExtension) {
  RunPlatformAppTestWithFiles("platform_apps/launch_file_by_extension",
                              kTestFilePath);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests that command line parameters get passed through to platform apps
// via launchData correctly when launching with a file.
// TODO(benwells/jeremya): tests need a way to specify a handler ID.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, LaunchWithFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_file", kTestFilePath))
      << message_;
}

// Tests that relative paths can be passed through to the platform app.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, LaunchWithRelativeFile) {
  ASSERT_TRUE(
      RunPlatformAppTestWithFile("platform_apps/launch_file",
                                 base::FilePath::FromUTF8Unsafe(kTestFilePath)))
      << message_;
}

// Tests that launch data is sent through if the file extension matches.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithFileExtension) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_file_by_extension", kTestFilePath))
      << message_;
}

// Tests that launch data is sent through to an allowlisted extension if the
// file extension matches.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchAllowListedExtensionWithFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_allowlisted_ext_with_file", kTestFilePath))
      << message_;
}

// Tests that launch data is sent through if the file extension and MIME type
// both match.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithFileExtensionAndMimeType) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_file_by_extension_and_type", kTestFilePath))
      << message_;
}

// Tests that launch data is sent through for a file with no extension if a
// handler accepts "".
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithFileWithoutExtension) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_file_with_no_extension",
      "platform_apps/launch_files/test"))
      << message_;
}

#if !BUILDFLAG(IS_WIN)
// Tests that launch data is sent through for a file with an empty extension if
// a handler accepts "".
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithFileEmptyExtension) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath test_file;
  ASSERT_TRUE(
      CopyTestDataAndGetTestFilePath(test_data_dir_.AppendASCII(kTestFilePath),
                                     temp_dir.GetPath(), "test.", &test_file));
  ASSERT_TRUE(RunPlatformAppTestWithFile(
      "platform_apps/launch_file_with_no_extension", test_file))
      << message_;
}

// Tests that launch data is sent through for a file with an empty extension if
// a handler accepts *.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithFileEmptyExtensionAcceptAny) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath test_file;
  ASSERT_TRUE(
      CopyTestDataAndGetTestFilePath(test_data_dir_.AppendASCII(kTestFilePath),
                                     temp_dir.GetPath(), "test.", &test_file));
  ASSERT_TRUE(RunPlatformAppTestWithFile(
      "platform_apps/launch_file_with_any_extension", test_file))
      << message_;
}
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that launch data is sent through for a file with no extension if a
// handler accepts *.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithFileWithoutExtensionAcceptAny) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_file_with_any_extension",
      "platform_apps/launch_files/test"))
      << message_;
}

// Tests that launch data is sent through for a file with an extension if a
// handler accepts *.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithFileAcceptAnyExtension) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_file_with_any_extension", kTestFilePath))
      << message_;
}

// Tests that no launch data is sent through if the file has the wrong
// extension.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithWrongExtension) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_wrong_extension", kTestFilePath))
      << message_;
}

// Tests that no launch data is sent through if the file has no extension but
// the handler requires a specific extension.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithWrongEmptyExtension) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_wrong_extension",
      "platform_apps/launch_files/test"))
      << message_;
}

// Tests that no launch data is sent through if the file is of the wrong MIME
// type.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, LaunchWithWrongType) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_wrong_type", kTestFilePath))
      << message_;
}

// Tests that no launch data is sent through if the platform app does not
// provide an intent.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, LaunchWithNoIntent) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_no_intent", kTestFilePath))
      << message_;
}

// Tests that launch data is sent through when the file has unknown extension
// but the MIME type can be sniffed and the sniffed type matches.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest,
                       LaunchWithSniffableType) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_file_by_extension_and_type",
      "platform_apps/launch_files/test.unknownextension"))
      << message_;
}

// Tests that launch data is sent through with the MIME type set to
// application/octet-stream if the file MIME type cannot be read.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, LaunchNoType) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_application_octet_stream",
      "platform_apps/launch_files/test_binary.unknownextension"))
      << message_;
}

// Tests that no launch data is sent through if the file does not exist.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, LaunchNoFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_invalid",
      "platform_apps/launch_files/doesnotexist.txt"))
      << message_;
}

// Tests that no launch data is sent through if the argument is a directory.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, LaunchWithDirectory) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/launch_invalid", "platform_apps/launch_files"))
      << message_;
}

// Tests that no launch data is sent through if there are no arguments passed
// on the command line
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, LaunchWithNothing) {
  ASSERT_TRUE(RunPlatformAppTestWithNothing("platform_apps/launch_nothing"))
      << message_;
}

// Test that platform apps can use the chrome.fileSystem.getDisplayPath
// function to get the File System Access path of a file they are launched with.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, GetDisplayPath) {
  ASSERT_TRUE(RunPlatformAppTestWithFileInTestDataDir(
      "platform_apps/get_display_path", kTestFilePath))
      << message_;
}

// Tests that the file is created if the file does not exist and the app has the
// fileSystem.write permission.
IN_PROC_BROWSER_TEST_F(PlatformAppWithFileBrowserTest, LaunchNewFile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(RunPlatformAppTestWithFile(
      "platform_apps/launch_new_file",
      temp_dir.GetPath().AppendASCII("new_file.txt")))
      << message_;
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OpenLink) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  LoadAndLaunchPlatformApp("open_link", "Launched");
  tab_added_waiter.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MutationEventsDisabled) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/mutation_events",
                               {.launch_as_platform_app = true}))
      << message_;
}

// This appears to be unreliable.
// TODO(stevenjb): Investigate and enable
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_AppWindowRestoreState DISABLED_AppWindowRestoreState
#else
#define MAYBE_AppWindowRestoreState AppWindowRestoreState
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_AppWindowRestoreState) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/restore_state",
                               {.launch_as_platform_app = true}));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       AppWindowAdjustBoundsToBeVisibleOnScreen) {
  const Extension* extension = LoadAndLaunchPlatformApp("minimal", "Launched");

  AppWindow* window = CreateAppWindow(browser()->profile(), extension);

  // The screen bounds didn't change, the cached bounds didn't need to adjust.
  gfx::Rect cached_bounds(80, 100, 400, 400);
  gfx::Rect cached_screen_bounds(0, 0, 1600, 900);
  gfx::Rect current_screen_bounds(0, 0, 1600, 900);
  gfx::Size minimum_size(200, 200);
  gfx::Rect bounds;
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
      window, cached_bounds, cached_screen_bounds, current_screen_bounds,
      minimum_size, &bounds);
  EXPECT_EQ(bounds, cached_bounds);

  // We have an empty screen bounds, the cached bounds didn't need to adjust.
  gfx::Rect empty_screen_bounds;
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
      window, cached_bounds, empty_screen_bounds, current_screen_bounds,
      minimum_size, &bounds);
  EXPECT_EQ(bounds, cached_bounds);

  // Cached bounds is completely off the new screen bounds in horizontal
  // locations. Expect to reposition the bounds.
  gfx::Rect horizontal_out_of_screen_bounds(-800, 100, 400, 400);
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
      window, horizontal_out_of_screen_bounds, gfx::Rect(-1366, 0, 1600, 900),
      current_screen_bounds, minimum_size, &bounds);
  EXPECT_EQ(bounds, gfx::Rect(0, 100, 400, 400));

  // Cached bounds is completely off the new screen bounds in vertical
  // locations. Expect to reposition the bounds.
  gfx::Rect vertical_out_of_screen_bounds(10, 1000, 400, 400);
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
      window, vertical_out_of_screen_bounds, gfx::Rect(-1366, 0, 1600, 900),
      current_screen_bounds, minimum_size, &bounds);
  EXPECT_EQ(bounds, gfx::Rect(10, 500, 400, 400));

  // From a large screen resulotion to a small one. Expect it fit on screen.
  gfx::Rect big_cache_bounds(10, 10, 1000, 1000);
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
      window, big_cache_bounds, gfx::Rect(0, 0, 1600, 1000),
      gfx::Rect(0, 0, 800, 600), minimum_size, &bounds);
  EXPECT_EQ(bounds, gfx::Rect(0, 0, 800, 600));

  // Don't resize the bounds smaller than minimum size, when the minimum size is
  // larger than the screen.
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
      window, big_cache_bounds, gfx::Rect(0, 0, 1600, 1000),
      gfx::Rect(0, 0, 800, 600), gfx::Size(900, 900), &bounds);
  EXPECT_EQ(bounds, gfx::Rect(0, 0, 900, 900));
}

namespace {

// TODO(crbug.com/1487630): flaky on Linux dbg.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_PlatformAppDevToolsBrowserTest \
  DISABLED_PlatformAppDevToolsBrowserTest
#else
#define MAYBE_PlatformAppDevToolsBrowserTest PlatformAppDevToolsBrowserTest
#endif
class MAYBE_PlatformAppDevToolsBrowserTest : public PlatformAppBrowserTest {
 protected:
  enum TestFlags {
    RELAUNCH = 0x1,
    HAS_ID = 0x2,
  };
  // Runs a test inside a harness that opens DevTools on an app window.
  void RunTestWithDevTools(const char* name, int test_flags);
};

void MAYBE_PlatformAppDevToolsBrowserTest::RunTestWithDevTools(const char* name,
                                                               int test_flags) {
  using content::DevToolsAgentHost;
  const Extension* extension = LoadAndLaunchPlatformApp(name, "Launched");
  ASSERT_TRUE(extension);
  AppWindow* window = GetFirstAppWindow();
  ASSERT_TRUE(window);
  ASSERT_EQ(window->window_key().empty(), (test_flags & HAS_ID) == 0);
  content::WebContents* web_contents = window->web_contents();
  ASSERT_TRUE(web_contents);

  DevToolsWindowTesting::OpenDevToolsWindowSync(web_contents, false);

  if (test_flags & RELAUNCH) {
    // Close the AppWindow, and ensure it is gone.
    CloseAppWindow(window);
    ASSERT_FALSE(GetFirstAppWindow());

    // Relaunch the app and get a new AppWindow.
    content::CreateAndLoadWebContentsObserver app_loaded_observer(
        /*num_expected_contents=*/2);

    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
            extension->id(), apps::LaunchContainer::kLaunchContainerNone,
            WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));
    app_loaded_observer.Wait();
    window = GetFirstAppWindow();
    ASSERT_TRUE(window);

    // DevTools should have reopened with the relaunch.
    web_contents = window->web_contents();
    ASSERT_TRUE(web_contents);
    ASSERT_TRUE(DevToolsAgentHost::HasFor(web_contents));
  }
}

}  // namespace

IN_PROC_BROWSER_TEST_F(MAYBE_PlatformAppDevToolsBrowserTest, ReOpenedWithID) {
  RunTestWithDevTools("minimal_id", RELAUNCH | HAS_ID);
}

IN_PROC_BROWSER_TEST_F(MAYBE_PlatformAppDevToolsBrowserTest, ReOpenedWithURL) {
  RunTestWithDevTools("minimal", RELAUNCH);
}

// Test that showing a permission request as a constrained window works and is
// correctly parented.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ConstrainedWindowRequest DISABLED_ConstrainedWindowRequest
#else
// TODO(sail): Enable this on other platforms once http://crbug.com/95455 is
// fixed.
#define MAYBE_ConstrainedWindowRequest DISABLED_ConstrainedWindowRequest
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_ConstrainedWindowRequest) {
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  const Extension* extension =
      LoadAndLaunchPlatformApp("optional_permission_request", "Launched");
  ASSERT_TRUE(extension) << "Failed to load extension.";

  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);

  // Verify that the app window has a dialog attached.
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  // Close the constrained window and wait for the reply to the permission
  // request.
  ExtensionTestMessageListener listener("PermissionRequestDone");
  WebContentsModalDialogManager::TestApi test_api(
      web_contents_modal_dialog_manager);
  test_api.CloseAllDialogs();
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

// Tests that an app calling chrome.runtime.reload will reload the app and
// relaunch it if it was running.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, ReloadRelaunches) {
  ExtensionTestMessageListener launched_listener("Launched",
                                                 ReplyBehavior::kWillReply);
  const Extension* extension =
      LoadAndLaunchPlatformApp("reload", &launched_listener);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetFirstAppWindow());

  // Now tell the app to reload itself.
  ExtensionTestMessageListener launched_listener2("Launched");
  launched_listener.Reply("reload");
  ASSERT_TRUE(launched_listener2.WaitUntilSatisfied());
  ASSERT_TRUE(GetFirstAppWindow());
}

// Tests that reloading a component app loads its (lazy) background page.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       ComponentReloadLoadsLazyBackgroundPage) {
  ExtensionTestMessageListener launched_listener("Launched",
                                                 ReplyBehavior::kWillReply);
  const Extension* component_app = LoadExtensionAsComponentWithManifest(
      test_data_dir_.AppendASCII("platform_apps")
          .AppendASCII("component_reload"),
      FILE_PATH_LITERAL("manifest.json"));
  ASSERT_TRUE(component_app);
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // Now tell the app to reload itself.
  ExtensionTestMessageListener launched_listener2("Launched");
  launched_listener.Reply("reload");
  ASSERT_TRUE(launched_listener2.WaitUntilSatisfied());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/1288199): Run these tests on Chrome OS with both Ash and
// Lacros processes active.

class PlatformAppChromeAppsDeprecationTest
    : public PlatformAppBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PlatformAppChromeAppsDeprecationTest()
      : chrome_apps_maybe_enabled_(
            &extensions::testing::g_enable_chrome_apps_for_testing,
            AreChromeAppsEnabledForTesting()) {}

  bool AreChromeAppsEnabledForTesting() { return GetParam(); }

 private:
  base::AutoReset<bool> chrome_apps_maybe_enabled_;
};

IN_PROC_BROWSER_TEST_P(PlatformAppChromeAppsDeprecationTest,
                       PlatformAppInAppService) {
  // The extension loads, but is not populated in the app service.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("minimal"),
      {.context_type = ContextType::kFromManifest}));
  ExtensionId packaged_app_id = last_loaded_extension_id();
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  ASSERT_TRUE(proxy);
  bool called = false;
  proxy->AppRegistryCache().ForOneApp(
      packaged_app_id,
      [&called](const apps::AppUpdate& update) { called = true; });
  if (AreChromeAppsEnabledForTesting()) {
    EXPECT_TRUE(called);
  } else {
    EXPECT_EQ(ExpectChromeAppsDefaultEnabled(), called);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PlatformAppChromeAppsDeprecationTest,
    ::testing::Values(true, false));

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

// Utility class to ensure extension installation does or does not occur in
// certain scenarios.
class CheckExtensionInstalledObserver
    : public extensions::ExtensionRegistryObserver {
 public:
  explicit CheckExtensionInstalledObserver(Profile* profile)
      : seen_(false), registry_(extensions::ExtensionRegistry::Get(profile)) {
    registry_->AddObserver(this);
  }
  ~CheckExtensionInstalledObserver() override {
    registry_->RemoveObserver(this);
  }

  bool seen() const { return seen_; }

  // ExtensionRegistryObserver:
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const extensions::Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override {
    EXPECT_FALSE(seen_);
    seen_ = true;
  }

 private:
  bool seen_;
  raw_ptr<extensions::ExtensionRegistry> registry_;
};

}  // namespace

// Component App Test 1 of 3: ensure that the initial load of a component
// extension utilizing a background page (e.g. a v2 platform app) has its
// background page run and is launchable. Waits for the Launched response from
// the script resource in the opened app window.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       PRE_PRE_ComponentAppBackgroundPage) {
  CheckExtensionInstalledObserver should_install(browser()->profile());

  // Ensure that we wait until the background page is run (to register the
  // OnLaunched listener) before trying to open the application. This is similar
  // to LoadAndLaunchPlatformApp, but we want to load as a component extension.
  content::CreateAndLoadWebContentsObserver app_loaded_observer;

  const Extension* extension = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("component"));
  ASSERT_TRUE(extension);

  app_loaded_observer.Wait();
  ASSERT_TRUE(should_install.seen());

  ExtensionTestMessageListener launched_listener("Launched");
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
          extension->id(), apps::LaunchContainer::kLaunchContainerNone,
          WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
}

// Component App Test 2 of 3: ensure an installed component app can be launched
// on a subsequent browser start, without requiring any install/upgrade logic
// to be run, then perform setup for step 3.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, PRE_ComponentAppBackgroundPage) {
  // Since the component app is now installed, re-adding it in the same profile
  // should not cause it to be re-installed. Instead, we wait for the OnLaunched
  // in a different observer (which would timeout if not the app was not
  // previously installed properly) and then check this observer to make sure it
  // never saw the NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED event.
  CheckExtensionInstalledObserver should_not_install(browser()->profile());
  const Extension* extension = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("component"));
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener launched_listener("Launched");
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
          extension->id(), apps::LaunchContainer::kLaunchContainerNone,
          WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ASSERT_FALSE(should_not_install.seen());

  // Simulate a "downgrade" from version 2 in the test manifest.json to 1.
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser()->profile());

  // Clear the registered events to ensure they are updated.
  extensions::EventRouter::Get(browser()->profile())
      ->ClearRegisteredEventsForTest(extension->id());

  ScopedDictPrefUpdate update(extension_prefs->pref_service(),
                              extensions::pref_names::kExtensions);
  std::string key(extension->id());
  key += ".manifest.version";
  update->SetByDottedPath(key, "1");
}

// Component App Test 3 of 3: simulate a component extension upgrade that
// re-adds the OnLaunched event, and allows the app to be launched.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, ComponentAppBackgroundPage) {
  CheckExtensionInstalledObserver should_install(browser()->profile());
  // Since we are forcing an upgrade, we need to wait for the load again.
  content::CreateAndLoadWebContentsObserver app_loaded_observer;

  const Extension* extension = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("component"));
  ASSERT_TRUE(extension);
  app_loaded_observer.Wait();
  ASSERT_TRUE(should_install.seen());

  ExtensionTestMessageListener launched_listener("Launched");
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
          extension->id(), apps::LaunchContainer::kLaunchContainerNone,
          WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
}

// Disabled due to flakiness. http://crbug.com/468609
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       DISABLED_ComponentExtensionRuntimeReload) {
  // Ensure that we wait until the background page is run (to register the
  // OnLaunched listener) before trying to open the application. This is similar
  // to LoadAndLaunchPlatformApp, but we want to load as a component extension.
  content::CreateAndLoadWebContentsObserver app_loaded_observer;

  const Extension* extension = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("component"));
  ASSERT_TRUE(extension);

  app_loaded_observer.Wait();

  {
    ExtensionTestMessageListener launched_listener("Launched");
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
            extension->id(), apps::LaunchContainer::kLaunchContainerNone,
            WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  }

  {
    ExtensionTestMessageListener launched_listener("Launched");
    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
        extension->id(),
        // NoWait actually waits for a domAutomationController.send() which is
        // implicitly append to the script. Since reload() restarts the
        // extension, the send after reload may not get executed. To get around
        // this, send first, then execute the reload().
        "window.domAutomationController.send(0);"
        "chrome.runtime.reload();"));
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  }
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, Messaging) {
  ResultCatcher result_catcher;
  LoadAndLaunchPlatformApp("messaging/app2", "Ready");
  LoadAndLaunchPlatformApp("messaging/app1", "Launched");
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// This test depends on focus and so needs to be in interactive_ui_tests.
// http://crbug.com/227041
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DISABLED_WebContentsHasFocus) {
  LoadAndLaunchPlatformApp("minimal", "Launched");

  EXPECT_EQ(1LU, GetAppWindowCount());
  EXPECT_TRUE(GetFirstAppWindow()
                  ->web_contents()
                  ->GetRenderWidgetHostView()
                  ->HasFocus());
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       WindowDotPrintShouldBringUpPrintPreview) {
  ScopedPreviewTestDelegate preview_delegate;
  ASSERT_TRUE(RunExtensionTest("platform_apps/print_api",
                               {.launch_as_platform_app = true}))
      << message_;
  preview_delegate.WaitUntilPreviewIsReady();
}

// This test verifies that http://crbug.com/297179 is fixed.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       DISABLED_ClosingWindowWhilePrintingShouldNotCrash) {
  ScopedPreviewTestDelegate preview_delegate;
  ASSERT_TRUE(RunExtensionTest("platform_apps/print_api",
                               {.launch_as_platform_app = true}))
      << message_;
  preview_delegate.WaitUntilPreviewIsReady();
  GetFirstAppWindow()->GetBaseWindow()->Close();
}

#endif  // ENABLE_PRINT_PREVIEW

#if BUILDFLAG(IS_CHROMEOS_ASH)

class PlatformAppIncognitoBrowserTest : public PlatformAppBrowserTest,
                                        public AppWindowRegistry::Observer {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Tell chromeos to launch in Guest mode, aka incognito.
    command_line->AppendSwitch(switches::kIncognito);
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }
  void SetUp() override {
    // Make sure the file manager actually gets loaded.
    ComponentLoader::EnableBackgroundExtensionsForTesting();
    PlatformAppBrowserTest::SetUp();
  }

  // AppWindowRegistry::Observer implementation.
  void OnAppWindowAdded(AppWindow* app_window) override {
    opener_app_ids_.insert(app_window->extension_id());
  }

 protected:
  // A set of ids of apps we've seen open a app window.
  std::set<std::string> opener_app_ids_;
};

// Seen to fail repeatedly on CrOS; crbug.com/774011.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_IncognitoComponentApp IncognitoComponentApp
#else
#define MAYBE_IncognitoComponentApp DISABLED_IncognitoComponentApp
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppIncognitoBrowserTest,
                       MAYBE_IncognitoComponentApp) {
  // Get the file manager app.
  const Extension* file_manager = extension_registry()->GetExtensionById(
      extension_misc::kFilesManagerAppId, ExtensionRegistry::ENABLED);
  ASSERT_TRUE(file_manager != nullptr);
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito_profile != nullptr);

  // Wait until the file manager has had a chance to register its listener
  // for the launch event.
  EventRouter* router = EventRouter::Get(incognito_profile);
  ASSERT_TRUE(router != nullptr);
  while (!router->ExtensionHasEventListener(
      file_manager->id(), app_runtime::OnLaunched::kEventName)) {
    content::RunAllPendingInMessageLoop();
  }

  // Listen for new app windows so we see the file manager app launch itself.
  AppWindowRegistry* registry = AppWindowRegistry::Get(incognito_profile);
  ASSERT_TRUE(registry != nullptr);
  registry->AddObserver(this);
  apps::AppServiceProxyFactory::GetForProfile(incognito_profile)
      ->Launch(file_manager->id(),
               apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                   true /* prefer_container */),
               apps::LaunchSource::kFromTest);

  while (!base::Contains(opener_app_ids_, file_manager->id())) {
    content::RunAllPendingInMessageLoop();
  }
}

class RestartKioskDeviceTest : public PlatformAppBrowserTest,
                               public ash::LocalStateMixin::Delegate {
 public:
  void SetUpLocalState() override {
    // Until EnterKioskSession is called, the setup and the test run in a
    // regular user session. Marking another user as the owner prevents the
    // current user from taking ownership and overriding the kiosk mode.
    user_manager::UserManager::Get()->RecordOwner(
        AccountId::FromUserEmail("not_current_user@example.com"));
  }

  void SetUpOnMainThread() override {
    user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    chromeos::SetUpFakeKioskSession();

    PlatformAppBrowserTest::SetUpOnMainThread();
    // Disable "faked" shutdown of Chrome if the OS was supposed to restart.
    // The fakes this test injects would cause it to crash.
    chromeos::FakePowerManagerClient* fake_power_manager_client =
        chromeos::FakePowerManagerClient::Get();
    ASSERT_NE(nullptr, fake_power_manager_client);
    fake_power_manager_client->set_restart_callback(base::DoNothing());
  }

  void TearDownOnMainThread() override {
    PlatformAppBrowserTest::TearDownOnMainThread();
    user_manager_.Reset();
  }

 protected:
  static int num_request_restart_calls() {
    return chromeos::FakePowerManagerClient::Get()->num_request_restart_calls();
  }

 private:
  ash::LocalStateMixin local_state_mixin_{&mixin_host_, this};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_;
};

// Tests that chrome.runtime.restart would request device restart in
// ChromeOS kiosk mode.
IN_PROC_BROWSER_TEST_F(RestartKioskDeviceTest, Restart) {
  ASSERT_EQ(0, num_request_restart_calls());

  ExtensionTestMessageListener launched_listener("Launched",
                                                 ReplyBehavior::kWillReply);
  const Extension* extension =
      LoadAndLaunchPlatformApp("restart_device", &launched_listener);
  ASSERT_TRUE(extension);

  launched_listener.Reply("restart");
  ExtensionTestMessageListener restart_requested_listener("restartRequested");
  ASSERT_TRUE(restart_requested_listener.WaitUntilSatisfied());

  EXPECT_EQ(1, num_request_restart_calls());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Test that when an application is uninstalled and re-install it does not have
// access to the previously set data.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, ReinstallDataCleanup) {
  // The application is installed and launched. After the 'Launched' message is
  // acknowledged by the browser process, the application will test that some
  // data are not installed and then install them. The application will then be
  // uninstalled and the same process will be repeated.
  std::string extension_id;

  {
    const Extension* extension =
        LoadAndLaunchPlatformApp("reinstall_data_cleanup", "Launched");
    ASSERT_TRUE(extension);
    extension_id = extension->id();

    ResultCatcher result_catcher;
    EXPECT_TRUE(result_catcher.GetNextResult());
  }

  UninstallExtension(extension_id);
  content::RunAllPendingInMessageLoop();

  {
    const Extension* extension =
        LoadAndLaunchPlatformApp("reinstall_data_cleanup", "Launched");
    ASSERT_TRUE(extension);
    ASSERT_EQ(extension_id, extension->id());

    ResultCatcher result_catcher;
    EXPECT_TRUE(result_catcher.GetNextResult());
  }
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppsIgnoreDefaultZoom) {
  const Extension* extension = LoadAndLaunchPlatformApp("minimal", "Launched");

  // Set the browser default zoom to something other than the default (which is
  // 0).
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(1);

  // Launch another window. This is a simple way to guarantee that any messages
  // that would have been delivered to the app renderer and back for zoom have
  // made it through.
  ExtensionTestMessageListener launched_listener("Launched");
  LaunchPlatformApp(extension);
  EXPECT_TRUE(launched_listener.WaitUntilSatisfied());

  // Now check that the app window's default zoom, and actual zoom level,
  // have not been changed from the default.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  content::HostZoomMap* app_host_zoom_map =
      content::HostZoomMap::Get(web_contents->GetSiteInstance());
  EXPECT_EQ(0, app_host_zoom_map->GetDefaultZoomLevel());
  EXPECT_EQ(0, app_host_zoom_map->GetZoomLevel(web_contents));
}

// Sends chrome.test.sendMessage from chrome.app.window.create's callback.
// The app window also adds an <iframe> to the page during window.onload.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWindowIframe) {
  LoadAndLaunchPlatformApp("app_window_send_message",
                           "APP_WINDOW_CREATE_CALLBACK");
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, NewWindowWithNonExistingFile) {
  ASSERT_TRUE(
      RunExtensionTest("platform_apps/new_window_with_non_existing_file",
                       {.launch_as_platform_app = true}));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, SandboxedLocalFile) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/sandboxed_local_file",
                               {.launch_as_platform_app = true}));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, NewWindowAboutBlank) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/new_window_about_blank",
                               {.launch_as_platform_app = true}));
}

// Test that an app window sees the synthetic wheel events of a touchpad pinch.
// While the app window itself does not scale in response to a pinch, we
// still offer the synthetic wheels for pages that want to implement custom
// pinch zoom behaviour.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       TouchpadPinchSyntheticWheelEvents) {
  LoadAndLaunchPlatformApp("touchpad_pinch", "Launched");

  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);

  // Ensure the compositor thread is aware of the wheel listener.
  content::MainThreadFrameObserver synchronize_threads(
      web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost());
  synchronize_threads.Wait();

  ExtensionTestMessageListener synthetic_wheel_listener("Seen wheel event");

  const gfx::Rect contents_rect = web_contents->GetContainerBounds();
  const gfx::Point pinch_position(contents_rect.width() / 2,
                                  contents_rect.height() / 2);
  content::SimulateGesturePinchSequence(web_contents, pinch_position, 1.23,
                                        blink::WebGestureDevice::kTouchpad);

  ASSERT_TRUE(synthetic_wheel_listener.WaitUntilSatisfied());
}

// TODO(crbug.com/961017): Fix memory leaks in tests and re-enable on LSAN.
#if defined(LEAK_SANITIZER)
#define MAYBE_VideoPictureInPicture DISABLED_VideoPictureInPicture
#else
#define MAYBE_VideoPictureInPicture VideoPictureInPicture
#endif

// Tests that platform apps can enter and exit video Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_VideoPictureInPicture) {
  LoadAndLaunchPlatformApp("picture_in_picture", "Launched");

  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::VideoPictureInPictureWindowController* window_controller =
      content::PictureInPictureWindowController::
          GetOrCreateVideoPictureInPictureController(web_contents);
  EXPECT_FALSE(window_controller->GetWindowForTesting());

  EXPECT_EQ(true, content::EvalJs(web_contents, "enterPictureInPicture();"));
  ASSERT_TRUE(window_controller->GetWindowForTesting());
  EXPECT_TRUE(window_controller->GetWindowForTesting()->IsVisible());

  EXPECT_EQ(true, content::EvalJs(web_contents, "exitPictureInPicture();"));
  EXPECT_FALSE(window_controller->GetWindowForTesting()->IsVisible());
}

}  // namespace extensions
