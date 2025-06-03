// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_uninstall_dialog_view.h"

#include <string>

#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/any_widget_observer.h"

class AppUninstallDialogViewBrowserTest : public DialogBrowserTest {
 public:
  AppDialogView* ActiveView() {
    return AppUninstallDialogView::GetActiveViewForTesting();
  }

  void ShowUi(const std::string& name) override {
    EXPECT_EQ(nullptr, ActiveView());

    auto* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(app_service_proxy);

    base::RunLoop run_loop;
    app_service_proxy->UninstallForTesting(
        app_id_, nullptr,
        base::BindLambdaForTesting([&](bool) { run_loop.Quit(); }));
    run_loop.Run();

    ASSERT_NE(nullptr, ActiveView());
    EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
              ActiveView()->GetDialogButtons());
    std::u16string title =
        u"Uninstall \"" + base::ASCIIToUTF16(app_name_) + u"\"?";
    EXPECT_EQ(title, ActiveView()->GetWindowTitle());

    if (name == "accept") {
      if (app_service_proxy->AppRegistryCache().GetAppType(app_id_) ==
          apps::AppType::kWeb) {
        web_app::WebAppTestUninstallObserver app_listener(browser()->profile());
        app_listener.BeginListening();
        ActiveView()->AcceptDialog();
        app_listener.Wait();
      } else {
        ActiveView()->AcceptDialog();
      }

      bool is_uninstalled = false;
      app_service_proxy->AppRegistryCache().ForOneApp(
          app_id_, [&is_uninstalled, name](const apps::AppUpdate& update) {
            is_uninstalled =
                (update.Readiness() == apps::Readiness::kUninstalledByUser);
          });

      EXPECT_TRUE(is_uninstalled);
    } else {
      ActiveView()->CancelDialog();

      bool is_installed = true;
      app_service_proxy->AppRegistryCache().ForOneApp(
          app_id_, [&is_installed, name](const apps::AppUpdate& update) {
            is_installed = (update.Readiness() == apps::Readiness::kReady);
          });

      EXPECT_TRUE(is_installed);
    }
    // Wait for the dialog window to be closed to destroy the Uninstall
    // dialog.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(nullptr, ActiveView());
  }

 protected:
  std::string app_id_;
  std::string app_name_;
};

class ArcAppsUninstallDialogViewBrowserTest
    : public AppUninstallDialogViewBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    AppUninstallDialogViewBrowserTest::SetUpOnMainThread();

    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);

    // Validating decoded content does not fit well for unit tests.
    ArcAppIcon::DisableSafeDecodingForTesting();

    arc_app_list_pref_ = ArcAppListPrefs::Get(browser()->profile());
    ASSERT_TRUE(arc_app_list_pref_);
    base::RunLoop run_loop;
    arc_app_list_pref_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
    arc_app_list_pref_->app_connection_holder()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_app_list_pref_->app_connection_holder());
  }

  void TearDownOnMainThread() override {
    arc_app_list_pref_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
  }

  void CreateApp() {
    std::vector<arc::mojom::AppInfoPtr> apps;
    apps.emplace_back(arc::mojom::AppInfo::New("Fake App 0", "fake.package.0",
                                               "fake.app.0.activity",
                                               false /* sticky */));
    app_instance_->SendRefreshAppList(apps);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1u, arc_app_list_pref_->GetAppIds().size());
    app_id_ =
        arc_app_list_pref_->GetAppId(apps[0]->package_name, apps[0]->activity);
    app_name_ = apps[0]->name;
  }

 private:
  raw_ptr<ArcAppListPrefs, DanglingUntriaged | ExperimentalAsh>
      arc_app_list_pref_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
};

IN_PROC_BROWSER_TEST_F(ArcAppsUninstallDialogViewBrowserTest, InvokeUi_Accept) {
  CreateApp();
  ShowUi("accept");
}

IN_PROC_BROWSER_TEST_F(ArcAppsUninstallDialogViewBrowserTest, InvokeUi_Cancel) {
  CreateApp();
  ShowUi("cancel");
}

class WebAppsUninstallDialogViewBrowserTest
    : public AppUninstallDialogViewBrowserTest {
 public:
  void SetUpOnMainThread() override {
    AppUninstallDialogViewBrowserTest::SetUpOnMainThread();

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  void CreateApp() {
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = GetAppURL();
    web_app_info->scope = GetAppURL().GetWithoutFilename();

    app_id_ = web_app::test::InstallWebApp(browser()->profile(),
                                           std::move(web_app_info));
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    web_app::LaunchWebAppBrowser(browser()->profile(), app_id_);
    navigation_observer.WaitForNavigationFinished();

    auto* provider = web_app::WebAppProvider::GetForTest(browser()->profile());
    DCHECK(provider);
    app_name_ = provider->registrar_unsafe().GetAppShortName(app_id_);
  }

  GURL GetAppURL() const {
    return https_server_.GetURL("app.com", "/ssl/google.html");
  }

 protected:
  // For mocking a secure site.
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(WebAppsUninstallDialogViewBrowserTest, InvokeUi_Accept) {
  CreateApp();
  ShowUi("accept");
}

IN_PROC_BROWSER_TEST_F(WebAppsUninstallDialogViewBrowserTest, InvokeUi_Cancel) {
  CreateApp();
  ShowUi("cancel");
}

IN_PROC_BROWSER_TEST_F(WebAppsUninstallDialogViewBrowserTest,
                       ExistingDialogFocus) {
  CreateApp();

  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(app_service_proxy);

  // First call to uninstall should return true in callback for successful.
  {
    base::RunLoop run_loop;
    app_service_proxy->UninstallForTesting(
        app_id_, nullptr, base::BindLambdaForTesting([&](bool dialog_opened) {
          EXPECT_TRUE(dialog_opened);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  views::Widget* first_widget = ActiveView()->GetWidget();
  first_widget->Hide();
  EXPECT_FALSE(first_widget->IsVisible());

  // Second call should be unsuccessful.
  {
    base::RunLoop run_loop;

    // The shown widget should be the one opened in the first call to uninstall.
    views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
    observer.set_shown_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          EXPECT_EQ(first_widget, widget);
          EXPECT_TRUE(first_widget->IsVisible());
        }));
    app_service_proxy->UninstallForTesting(
        app_id_, nullptr, base::BindLambdaForTesting([&](bool dialog_opened) {
          EXPECT_FALSE(dialog_opened);
          run_loop.Quit();
        }));

    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(WebAppsUninstallDialogViewBrowserTest,
                       PreventDuplicateUninstallDialogs) {
  CreateApp();

  EXPECT_EQ(nullptr, ActiveView());

  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(app_service_proxy);

  // First call to uninstall should return true in callback for successful.
  {
    base::RunLoop run_loop;
    app_service_proxy->UninstallForTesting(
        app_id_, nullptr, base::BindLambdaForTesting([&](bool dialog_opened) {
          EXPECT_TRUE(dialog_opened);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  // Second call should be unsuccessful.
  {
    base::RunLoop run_loop;
    app_service_proxy->UninstallForTesting(
        app_id_, nullptr, base::BindLambdaForTesting([&](bool dialog_opened) {
          EXPECT_FALSE(dialog_opened);
          run_loop.Quit();
        }));

    run_loop.Run();
  }

  ASSERT_NE(nullptr, ActiveView());
  EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
            ActiveView()->GetDialogButtons());
  std::u16string title =
      u"Uninstall \"" + base::ASCIIToUTF16(app_name_) + u"\"?";
  EXPECT_EQ(title, ActiveView()->GetWindowTitle());

  // Cancelling the active dialog should not uninstall the web app.
  ActiveView()->CancelDialog();
  // Wait for the dialog window to be closed to destroy the Uninstall dialog.
  base::RunLoop().RunUntilIdle();

  bool is_installed = true;
  app_service_proxy->AppRegistryCache().ForOneApp(
      app_id_, [&is_installed](const apps::AppUpdate& update) {
        is_installed = update.Readiness() == apps::Readiness::kReady;
      });
  EXPECT_TRUE(is_installed);

  // Uninstall dialog should be reopenable.
  EXPECT_EQ(nullptr, ActiveView());
  {
    base::RunLoop run_loop;
    app_service_proxy->UninstallForTesting(
        app_id_, nullptr, base::BindLambdaForTesting([&](bool dialog_opened) {
          EXPECT_TRUE(dialog_opened);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  ASSERT_NE(nullptr, ActiveView());
}

IN_PROC_BROWSER_TEST_F(WebAppsUninstallDialogViewBrowserTest,
                       SubAppsShownCorrectly) {
  CreateApp();

  std::unordered_set<std::u16string> sub_apps_expected;

  // Include non-ASCII characters in app names to ensure they get displayed
  // correctly
  for (const std::string& app_name : {"one", "fünf", "🌈"}) {
    std::u16string sub_app_name = u"Sub App " + base::UTF8ToUTF16(app_name);
    sub_apps_expected.emplace(sub_app_name);

    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url =
        https_server_.GetURL("app.com", "/sub-app-" + app_name);
    web_app_info->parent_app_id = app_id_;
    web_app_info->title = sub_app_name;
    web_app_info->parent_app_manifest_id = GetAppURL();

    web_app::test::InstallWebApp(browser()->profile(), std::move(web_app_info),
                                 /*overwrite_existing_manifest_fields=*/true,
                                 webapps::WebappInstallSource::SUB_APP);
  }

  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(app_service_proxy);
  {
    base::RunLoop run_loop;
    app_service_proxy->UninstallForTesting(
        app_id_, nullptr, base::BindLambdaForTesting([&](bool dialog_opened) {
          EXPECT_TRUE(dialog_opened);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  std::unordered_set<std::u16string> sub_apps_actual;
  views::View* view = ActiveView()->GetWidget()->GetContentsView();
  std::vector<views::View*> views_group;
  view->GetViewsInGroup(
      static_cast<int>(AppUninstallDialogView::DialogViewID::SUB_APP_LABEL),
      &views_group);
  for (auto* label : views_group) {
    sub_apps_actual.emplace(static_cast<views::Label*>(label)->GetText());
  }
  EXPECT_EQ(sub_apps_actual, sub_apps_expected);
}
