// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOGS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOGS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_uninstall_dialog_user_options.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
              BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA));

class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace views {
class Widget;
}  // namespace views

namespace webapps {
class MlInstallOperationTracker;
enum class WebappUninstallSource;
struct Screenshot;
}  // namespace webapps

namespace web_app {

struct WebAppInstallInfo;

// Callback used to indicate whether a user has accepted the installation of a
// web app. The boolean parameter is true when the user accepts the dialog. The
// WebAppInstallInfo parameter contains the information about the app,
// possibly modified by the user.
using AppInstallationAcceptanceCallback =
    base::OnceCallback<void(bool, std::unique_ptr<WebAppInstallInfo>)>;

// Shows the Web App install bubble.
//
// |web_app_info| is the WebAppInstallInfo being converted into an app.
// |web_app_info.app_url| should contain a start url from a web app manifest
// (for a Desktop PWA), or the current url (when creating a shortcut app).
void ShowWebAppInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback);

// Creates a dialog that requests the consent from the user to install the
// requested apps as sub apps to the named parent app. This is triggered by
// an app calling the Multi App API add() function. The dialog is modal to
// the browser containing the app calling the API. |sub_apps| contains the
// information to represent each app to the user.
views::Widget* CreateSubAppsInstallDialogWidget(
    const std::u16string parent_app_name,
    const std::u16string parent_app_scope,
    const std::vector<std::unique_ptr<WebAppInstallInfo>>& sub_apps,
    base::RepeatingClosure settings_page_callback,
    gfx::NativeWindow window);

// When an app changes its icon or name, that is considered an app identity
// change which (for some types of apps) needs confirmation from the user.
// This function shows that confirmation dialog. |app_id| is the unique id of
// the app that is updating and |title_change| and |icon_change| specify which
// piece of information is changing. Can be one or the other, or both (but
// both cannot be |false|). |old_title| and |new_title|, as well as |old_icon|
// and |new_icon| show the 'before' and 'after' values. A response is sent
// back via the |callback|.
void ShowWebAppIdentityUpdateDialog(const std::string& app_id,
                                    bool title_change,
                                    bool icon_change,
                                    const std::u16string& old_title,
                                    const std::u16string& new_title,
                                    const SkBitmap& old_icon,
                                    const SkBitmap& new_icon,
                                    content::WebContents* web_contents,
                                    AppIdentityDialogCallback callback);

// Shows the web app uninstallation dialog on a page whenever user has decided
// to uninstall an installed dPWA from a variety of OS surfaces and chrome.
void ShowWebAppUninstallDialog(
    Profile* profile,
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow parent,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps,
    UninstallDialogCallback uninstall_dialog_result_callback);

// Callback used to indicate whether a user has accepted the launch of a
// web app. The |allowed| is true when the user allows the app to launch.
// |remember_user_choice| is true if the user wants to persist the decision.
using WebAppLaunchAcceptanceCallback =
    base::OnceCallback<void(bool allowed, bool remember_user_choice)>;

// Shows the pre-launch dialog for protocol handling PWA launch. The user can
// allow or block the launch.
void ShowWebAppProtocolLaunchDialog(
    const GURL& url,
    Profile* profile,
    const webapps::AppId& app_id,
    WebAppLaunchAcceptanceCallback close_callback);

// Shows the pre-launch dialog for a file handling PWA launch. The user can
// allow or block the launch.
void ShowWebAppFileLaunchDialog(const std::vector<base::FilePath>& file_paths,
                                Profile* profile,
                                const webapps::AppId& app_id,
                                WebAppLaunchAcceptanceCallback close_callback);
// Sets whether |ShowWebAppDialog| should accept immediately without any
// user interaction. |auto_open_in_window| sets whether the open in window
// checkbox is checked.
void SetAutoAcceptWebAppDialogForTesting(bool auto_accept,
                                         bool auto_open_in_window);

// Sets an override title for the installation.
void SetOverrideTitleForTesting(const char* title_to_use);

// Describes the state of in-product-help being shown to the user.
enum class PwaInProductHelpState {
  // The in-product-help bubble was shown.
  kShown,
  // The in-product-help bubble was not shown.
  kNotShown
};

// Shows the PWA installation confirmation bubble anchored off the PWA install
// icon in the omnibox.
//
// |web_app_info| is the WebAppInstallInfo to be installed.
// |callback| is called when install bubble closed.
// |iph_state| records whether PWA install iph is shown before Install bubble is
// shown.
void ShowPWAInstallBubble(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown);

// Shows the Web App detailed install dialog.
// The dialog shows app's detailed information including screenshots. Users then
// confirm or cancel install in this dialog.
void ShowWebAppDetailedInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    const std::vector<webapps::Screenshot>& screenshots,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown);

// Sets whether |ShowPWAInstallBubble| should accept immediately without any
// user interaction.
void SetAutoAcceptPWAInstallConfirmationForTesting(bool auto_accept);

// Shows the Isolated Web App manual install wizard.
void LaunchIsolatedWebAppInstaller(Profile* profile,
                                   const base::FilePath& bundle_path);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOGS_H_
