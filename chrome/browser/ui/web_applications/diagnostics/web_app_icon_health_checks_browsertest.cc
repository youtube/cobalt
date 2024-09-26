// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace web_app {

// TODO(https://crbug.com/1353659): Enable tests on Lacros.
// This feature depends on
// https://chromium-review.googlesource.com/c/chromium/src/+/3867152 landing
// to be able to work in Lacros. Currently Lacros doesn't know when the web app
// publisher has been initialised.

#if !BUILDFLAG(IS_CHROMEOS_LACROS)

class WebAppIconHealthChecksBrowserTest : public WebAppControllerBrowserTest {
 public:
  WebAppIconHealthChecksBrowserTest() {
    WebAppMetrics::DisableAutomaticIconHealthChecksForTesting();
  }

  ~WebAppIconHealthChecksBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Profile* profile() { return browser()->profile(); }

  ScopedRegistryUpdate CreateUpdateScope() {
    return ScopedRegistryUpdate(
        &WebAppProvider::GetForTest(profile())->sync_bridge_unsafe());
  }

  void RunIconChecksWithMetricExpectations(
      WebAppIconDiagnostic::Result expected_result) {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    WebAppMetrics::Get(profile())->icon_health_checks_for_testing().Start(
        run_loop.QuitClosure());
    run_loop.Run();

    auto check_histogram = [&](const char* histogram, bool subresult) {
      histogram_tester.ExpectUniqueSample(histogram, subresult, 1);
    };
    check_histogram("WebApp.Icon.AppsWithEmptyDownloadedIconSizes",
                    expected_result.has_empty_downloaded_icon_sizes);
    check_histogram("WebApp.Icon.AppsWithGeneratedIconFlag",
                    expected_result.has_generated_icon_flag);
    check_histogram("WebApp.Icon.AppsWithGeneratedIconBitmap",
                    expected_result.has_generated_icon_bitmap);
    check_histogram("WebApp.Icon.AppsWithGeneratedIconFlagFalseNegative",
                    expected_result.has_generated_icon_flag_false_negative);
    check_histogram("WebApp.Icon.AppsWithEmptyIconBitmap",
                    expected_result.has_empty_icon_bitmap);
    check_histogram("WebApp.Icon.AppsWithEmptyIconFile",
                    expected_result.has_empty_icon_file);
    check_histogram("WebApp.Icon.AppsWithMissingIconFile",
                    expected_result.has_missing_icon_file);
  }

  AppId InstallWebAppAndAwaitAppService(const char* path) {
    AppId app_id =
        InstallWebAppFromPage(browser(), embedded_test_server()->GetURL(path));
    AppReadinessWaiter(profile(), app_id, apps::Readiness::kReady).Await();
    return app_id;
  }
};

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, HealthyIcons) {
  InstallWebAppAndAwaitAppService("/web_apps/basic.html");
  RunIconChecksWithMetricExpectations({});
}

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, EmptyAppName) {
  AppId app_id = InstallWebAppAndAwaitAppService("/web_apps/basic.html");

  // Delete the app name (some users may have corrupt web app databases with
  // missing app names).
  {
    WebAppSyncBridge& sync_bridge =
        WebAppProvider::GetForTest(profile())->sync_bridge_unsafe();
    sync_bridge.set_disable_checks_for_testing(true);
    ScopedRegistryUpdate update(&sync_bridge);
    WebApp* web_app = update->UpdateApp(app_id);
    web_app->SetName("");
  }

  // Check that we don't crash on empty app names.
  RunIconChecksWithMetricExpectations({});
}

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest,
                       MissingDownloadedIconSizes) {
  AppId app_id = InstallWebAppAndAwaitAppService("/web_apps/basic.html");
  CreateUpdateScope()->UpdateApp(app_id)->SetDownloadedIconSizes(
      IconPurpose::ANY, {});
  RunIconChecksWithMetricExpectations(
      {.has_empty_downloaded_icon_sizes = true, .has_empty_icon_bitmap = true});
}

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, GeneratedIcon) {
  InstallWebAppAndAwaitAppService("/web_apps/get_manifest.html?no_icons.json");
  RunIconChecksWithMetricExpectations(
      {.has_generated_icon_flag = true, .has_generated_icon_bitmap = true});
}

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest,
                       GeneratedIconFlagFalseNegative) {
  AppId app_id = InstallWebAppAndAwaitAppService(
      "/web_apps/get_manifest.html?no_icons.json");
  // In https://crbug.com/1317922 manifest update erroneously set
  // is_generated_icon to false.
  CreateUpdateScope()->UpdateApp(app_id)->SetIsGeneratedIcon(false);
  RunIconChecksWithMetricExpectations(
      {.has_generated_icon_flag_false_negative = true,
       .has_generated_icon_bitmap = true});
}

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest,
                       PRE_DeletedIconFiles) {
  AppId app_id = InstallWebAppAndAwaitAppService("/web_apps/basic.html");
  RunIconChecksWithMetricExpectations({});

  // Delete the icons.
  base::RunLoop run_loop;
  WebAppProvider::GetForTest(profile())->icon_manager().DeleteData(
      app_id,
      base::BindLambdaForTesting([&](bool success) { run_loop.Quit(); }));
  run_loop.Run();

  // Restart to reload the app.
}
IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, DeletedIconFiles) {
  RunIconChecksWithMetricExpectations(
      {.has_empty_icon_bitmap = true, .has_missing_icon_file = true});
}

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, PRE_EmptyIconFile) {
  AppId app_id = InstallWebAppAndAwaitAppService("/web_apps/basic.html");
  RunIconChecksWithMetricExpectations({});

  // Empty the contents of one of the icon files.
  base::FilePath icon_path =
      WebAppProvider::GetForTest(profile())
          ->icon_manager()
          .GetIconFilePathForTesting(app_id, IconPurpose::ANY, 32);
  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN})
      ->PostTaskAndReply(FROM_HERE, base::BindLambdaForTesting([icon_path]() {
                           base::WriteFile(icon_path, "");
                         }),
                         run_loop.QuitClosure());
  run_loop.Run();

  // Restart to reload the app.
}
IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, EmptyIconFile) {
  RunIconChecksWithMetricExpectations(
      {.has_empty_icon_bitmap = true, .has_empty_icon_file = true});
}

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, PRE_CorruptIconFile) {
  AppId app_id = InstallWebAppAndAwaitAppService("/web_apps/basic.html");
  RunIconChecksWithMetricExpectations({});

  // Corrupt the contents of one of the icon files.
  base::FilePath icon_path =
      WebAppProvider::GetForTest(profile())
          ->icon_manager()
          .GetIconFilePathForTesting(app_id, IconPurpose::ANY, 32);
  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN})
      ->PostTaskAndReply(
          FROM_HERE, base::BindLambdaForTesting([icon_path]() {
            base::WriteFile(icon_path, "This is invalid data for a PNG file.");
          }),
          run_loop.QuitClosure());
  run_loop.Run();

  // Restart to reload the app.
}
IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, CorruptIconFile) {
  RunIconChecksWithMetricExpectations({.has_empty_icon_bitmap = true});
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace web_app
