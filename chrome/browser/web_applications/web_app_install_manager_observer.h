// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class WebAppInstallManagerObserver : public base::CheckedObserver {
 public:
  // Called after a web app is installed.
  virtual void OnWebAppInstalled(const AppId& app_id) {}

  // Called when OS hooks installation is finished during web app installation.
  virtual void OnWebAppInstalledWithOsHooks(const AppId& app_id) {}

  // Called before a web app is uninstalled, before the uninstallation process
  // begins. |app_id| is still registered in the WebAppRegistrar, and OS hooks
  // have not yet been uninstalled.
  virtual void OnWebAppWillBeUninstalled(const AppId& app_id) {}

  // Called after a web app is uninstalled. |app_id| is no longer registered in
  // the WebAppRegistrar, all OS hooks are uninstalled, and icons have been
  // deleted.
  virtual void OnWebAppUninstalled(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) {}

  // Called when any field of a web app's local manifest is updated.
  // Note that |old_name| will always be the same as the current name as we
  // don't support name updating yet. See TODO(crbug.com/1088338).
  virtual void OnWebAppManifestUpdated(const AppId& app_id,
                                       base::StringPiece old_name) {}

  // Called when the WebAppInstallManager is about to be destroyed.
  virtual void OnWebAppInstallManagerDestroyed() {}
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_OBSERVER_H_
