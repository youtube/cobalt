// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebUI;
}

// Handles API requests from chrome://web-app-internals page by implementing
// mojom::WebAppInternalsHandler.
class WebAppInternalsHandler : public mojom::WebAppInternalsHandler {
 public:
  static void BuildDebugInfo(
      Profile* profile,
      base::OnceCallback<void(base::Value root)> callback);

  WebAppInternalsHandler(
      content::WebUI* web_ui,
      mojo::PendingReceiver<mojom::WebAppInternalsHandler> receiver);

  WebAppInternalsHandler(const WebAppInternalsHandler&) = delete;
  WebAppInternalsHandler& operator=(const WebAppInternalsHandler&) = delete;

  ~WebAppInternalsHandler() override;

  // mojom::WebAppInternalsHandler:
  void GetDebugInfoAsJsonString(
      GetDebugInfoAsJsonStringCallback callback) override;
  void InstallIsolatedWebAppFromDevProxy(
      const GURL& url,
      InstallIsolatedWebAppFromDevProxyCallback callback) override;
  void SelectFileAndInstallIsolatedWebAppFromDevBundle(
      SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback)
      override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void ClearExperimentalWebAppIsolationData(
      ClearExperimentalWebAppIsolationDataCallback callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  void SearchForIsolatedWebAppUpdates(
      SearchForIsolatedWebAppUpdatesCallback callback) override;
  void GetIsolatedWebAppDevModeProxyAppInfo(
      GetIsolatedWebAppDevModeProxyAppInfoCallback callback) override;
  void UpdateDevProxyIsolatedWebApp(
      const webapps::AppId& app_id,
      UpdateDevProxyIsolatedWebAppCallback callback) override;

 private:
  class IsolatedWebAppDevBundleSelectListener;

  void OnIsolatedWebAppDevModeBundleSelected(
      SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback,
      absl::optional<base::FilePath> path);
  void OnInstallIsolatedWebAppFromDevModeProxy(
      InstallIsolatedWebAppFromDevProxyCallback callback,
      web_app::IsolatedWebAppInstallationManager::
          MaybeInstallIsolatedWebAppCommandSuccess result);

  const raw_ref<content::WebUI> web_ui_;
  const raw_ref<Profile> profile_;
  mojo::Receiver<mojom::WebAppInternalsHandler> receiver_;
  base::WeakPtrFactory<WebAppInternalsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_
