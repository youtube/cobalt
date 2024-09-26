// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/buildflags.h"
#include "components/webapps/browser/installable/installable_metrics.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/services/app_service/public/cpp/url_handler_info.h"
#endif

class GURL;
class Profile;

namespace web_app {

class WebAppProvider;

namespace test {

// Start the WebAppProvider and subsystems, and wait for startup to complete.
// Disables auto-install of system web apps and default web apps. Intended for
// unit tests, not browser tests.
void AwaitStartWebAppProviderAndSubsystems(Profile* profile);

// Wait until the provided WebAppProvider is ready.
void WaitUntilReady(WebAppProvider* provider);

// Wait until the provided WebAppProvider is ready and its subsystems startup
// is complete.
void WaitUntilWebAppProviderAndSubsystemsReady(WebAppProvider* provider);

AppId InstallDummyWebApp(
    Profile* profile,
    const std::string& app_name,
    const GURL& app_url,
    const webapps::WebappInstallSource install_source =
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

// Synchronous version of WebAppInstallManager::InstallWebAppFromInfo. May be
// used in unit tests and browser tests.
AppId InstallWebApp(Profile* profile,
                    std::unique_ptr<WebAppInstallInfo> web_app_info,
                    bool overwrite_existing_manifest_fields = false,
                    webapps::WebappInstallSource install_source =
                        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

// Synchronously uninstall a web app. May be used in unit tests and browser
// tests.
void UninstallWebApp(Profile* profile, const AppId& app_id);

// Synchronously uninstall all web apps for the given profile. May be used in
// unit tests and browser tests. Returns `false` if there was a failure.
bool UninstallAllWebApps(Profile* profile);

}  // namespace test
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_TEST_UTILS_H_
