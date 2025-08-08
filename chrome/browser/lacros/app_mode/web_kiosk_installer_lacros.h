// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_APP_MODE_WEB_KIOSK_INSTALLER_LACROS_H_
#define CHROME_BROWSER_LACROS_APP_MODE_WEB_KIOSK_INSTALLER_LACROS_H_

#include "chrome/browser/chromeos/app_mode/web_kiosk_app_installer.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

// Manage the kiosk session and related resources at the lacros side.
class WebKioskInstallerLacros : public crosapi::mojom::WebKioskInstaller {
 public:
  using GetWebKioskInstallStateCallback =
      crosapi::mojom::WebKioskInstaller::GetWebKioskInstallStateCallback;
  using InstallWebKioskCallback =
      crosapi::mojom::WebKioskInstaller::InstallWebKioskCallback;

  explicit WebKioskInstallerLacros(Profile& profile);
  WebKioskInstallerLacros(const WebKioskInstallerLacros&) = delete;
  WebKioskInstallerLacros& operator=(const WebKioskInstallerLacros&) = delete;
  ~WebKioskInstallerLacros() override;

  // crosapi::mojom::WebKioskInstaller
  void GetWebKioskInstallState(
      const GURL& url,
      GetWebKioskInstallStateCallback callback) override;
  void InstallWebKiosk(const GURL& url,
                       InstallWebKioskCallback callback) override;

 private:
  raw_ref<Profile> profile_;

  mojo::Receiver<crosapi::mojom::WebKioskInstaller> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_APP_MODE_WEB_KIOSK_INSTALLER_LACROS_H_
