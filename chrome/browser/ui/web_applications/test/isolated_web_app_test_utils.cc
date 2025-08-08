// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

IsolatedWebAppBrowserTestHarness::IsolatedWebAppBrowserTestHarness() {
  iwa_scoped_feature_list_.InitWithFeatures(
      {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
}

IsolatedWebAppBrowserTestHarness::~IsolatedWebAppBrowserTestHarness() = default;

std::unique_ptr<net::EmbeddedTestServer>
IsolatedWebAppBrowserTestHarness::CreateAndStartServer(
    const base::FilePath::StringPieceType& chrome_test_data_relative_root) {
  return CreateAndStartDevServer(chrome_test_data_relative_root);
}

IsolatedWebAppUrlInfo
IsolatedWebAppBrowserTestHarness::InstallDevModeProxyIsolatedWebApp(
    const url::Origin& origin) {
  return web_app::InstallDevModeProxyIsolatedWebApp(profile(), origin);
}

Browser* IsolatedWebAppBrowserTestHarness::GetBrowserFromFrame(
    content::RenderFrameHost* frame) {
  Browser* browser = chrome::FindBrowserWithTab(
      content::WebContents::FromRenderFrameHost(frame));
  EXPECT_TRUE(browser);
  return browser;
}

content::RenderFrameHost* IsolatedWebAppBrowserTestHarness::OpenApp(
    const webapps::AppId& app_id,
    base::StringPiece path) {
  return OpenIsolatedWebApp(profile(), app_id, path);
}

content::RenderFrameHost*
IsolatedWebAppBrowserTestHarness::NavigateToURLInNewTab(
    Browser* window,
    const GURL& url,
    WindowOpenDisposition disposition) {
  auto new_contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser()->profile()));
  window->tab_strip_model()->AppendWebContents(std::move(new_contents),
                                               /*foreground=*/true);
  return ui_test_utils::NavigateToURLWithDisposition(
      window, url, disposition, ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

std::unique_ptr<net::EmbeddedTestServer> CreateAndStartDevServer(
    const base::FilePath::StringPieceType& chrome_test_data_relative_root) {
  base::FilePath server_root =
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))
          .Append(chrome_test_data_relative_root);
  auto server = std::make_unique<net::EmbeddedTestServer>();
  server->AddDefaultHandlers(server_root);
  CHECK(server->Start());
  return server;
}

IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
    Profile* profile,
    const url::Origin& proxy_origin) {
  base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                        InstallIsolatedWebAppCommandError>>
      future;

  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      web_package::SignedWebBundleId::CreateRandomForDevelopment());
  WebAppProvider::GetForWebApps(profile)->scheduler().InstallIsolatedWebApp(
      url_info, DevModeProxy{.proxy_url = proxy_origin},
      /*expected_version=*/absl::nullopt,
      /*optional_keep_alive=*/nullptr,
      /*optional_profile_keep_alive=*/nullptr, future.GetCallback());

  CHECK(future.Get().has_value()) << future.Get().error();

  return url_info;
}

content::RenderFrameHost* OpenIsolatedWebApp(Profile* profile,
                                             const webapps::AppId& app_id,
                                             base::StringPiece path) {
  WebAppRegistrar& registrar =
      WebAppProvider::GetForWebApps(profile)->registrar_unsafe();
  const WebApp* app = registrar.GetAppById(app_id);
  EXPECT_TRUE(app);

  NavigateParams params(profile, app->start_url().Resolve(path),
                        ui::PAGE_TRANSITION_GENERATED);
  params.app_id = app->app_id();
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  params.user_gesture = true;
  ui_test_utils::NavigateToURL(&params);
  return params.navigated_or_inserted_contents->GetPrimaryMainFrame();
}

void CreateIframe(content::RenderFrameHost* parent_frame,
                  const std::string& iframe_id,
                  const GURL& url,
                  const std::string& permissions_policy) {
  EXPECT_EQ(true, content::EvalJs(
                      parent_frame,
                      content::JsReplace(R"(
            new Promise(resolve => {
              let f = document.createElement('iframe');
              f.id = $1;
              f.src = $2;
              f.allow = $3;
              f.addEventListener('load', () => resolve(true));
              document.body.appendChild(f);
            });
        )",
                                         iframe_id, url, permissions_policy)));
}

// TODO(crbug.com/1459157): This function should probably be built on top of
// `test::InstallDummyWebApp`, instead of committing the update and triggering
// `NotifyWebAppInstalled` manually. However, the `InstallFromInfoCommand` used
// by that function does not currently allow setting the `WebApp::IsolationData`
// (which is good for non-test-code, as all real IWA installs must go through
// the `InstallIsolatedWebAppCommand`).
webapps::AppId AddDummyIsolatedAppToRegistry(
    Profile* profile,
    const GURL& start_url,
    const std::string& name,
    const WebApp::IsolationData& isolation_data) {
  CHECK(profile);
  WebAppProvider* provider = WebAppProvider::GetForTest(profile);
  CHECK(provider);

  std::unique_ptr<WebApp> isolated_web_app = test::CreateWebApp(start_url);
  const webapps::AppId app_id = isolated_web_app->app_id();
  isolated_web_app->SetName(name);
  isolated_web_app->SetScope(isolated_web_app->start_url());
  isolated_web_app->SetIsolationData(isolation_data);

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        provider->sync_bridge_unsafe().BeginUpdate(future.GetCallback());
    update->CreateApp(std::move(isolated_web_app));
  }
  EXPECT_TRUE(future.Take());
  provider->install_manager().NotifyWebAppInstalled(app_id);
  return app_id;
}

// TODO(cmfcmf): Move more test utils into this `test` namespace
namespace test {

std::string BitmapAsPng(const SkBitmap& bitmap) {
  SkDynamicMemoryWStream stream;
  EXPECT_TRUE(SkPngEncoder::Encode(&stream, bitmap.pixmap(), {}));
  sk_sp<SkData> icon_skdata = stream.detachAsData();
  return std::string(static_cast<const char*>(icon_skdata->data()),
                     icon_skdata->size());
}

}  // namespace test

}  // namespace web_app
