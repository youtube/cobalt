// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/one_shot_event.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/link_capturing/enable_link_capturing_infobar_delegate.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/navigation_capturing_information_forwarder.h"
#include "chrome/browser/web_applications/navigation_capturing_log.h"
#include "chrome/browser/web_applications/navigation_capturing_navigation_handle_user_data.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_launch_params.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_browser_controller_ash.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace web_app {
namespace {
Browser* ReparentWebContentsIntoAppBrowser(content::WebContents* contents,
                                           Browser* target_browser,
                                           const webapps::AppId& app_id,
                                           bool insert_as_pinned_home_tab) {
  DCHECK(target_browser->is_type_app());
  Browser* source_browser = chrome::FindBrowserWithTab(contents);
  CHECK(contents);

  TabStripModel* target_tabstrip = target_browser->tab_strip_model();
  bool target_has_pinned_home_tab = HasPinnedHomeTab(target_tabstrip);
  if (!insert_as_pinned_home_tab) {
    MaybeAddPinnedHomeTab(target_browser, app_id);
  }

  // Avoid causing an existing non-app browser window to close if this is the
  // last tab remaining.
  if (source_browser->tab_strip_model()->count() == 1) {
    chrome::NewTab(source_browser);
  }

  ReparentWebContentsIntoBrowserImpl(
      source_browser, contents, target_browser,
      /*insert_as_pinned_first_tab=*/insert_as_pinned_home_tab);
  if (insert_as_pinned_home_tab) {
    if (target_has_pinned_home_tab) {
      target_tabstrip->DetachAndDeleteWebContentsAt(1);
    }
    SetWebContentsIsPinnedHomeTab(target_tabstrip->GetWebContentsAt(0));
  }
  return target_browser;
}

#if BUILDFLAG(IS_CHROMEOS)
const ash::SystemWebAppDelegate* GetSystemWebAppDelegate(
    Browser* browser,
    const webapps::AppId& app_id) {
  auto system_app_type =
      ash::GetSystemWebAppTypeForAppId(browser->profile(), app_id);
  if (system_app_type) {
    return ash::SystemWebAppManager::Get(browser->profile())
        ->GetSystemApp(*system_app_type);
  }
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<AppBrowserController> CreateWebKioskBrowserController(
    Browser* browser,
    WebAppProvider* provider,
    const webapps::AppId& app_id) {
  const ash::SystemWebAppDelegate* system_app =
      GetSystemWebAppDelegate(browser, app_id);
  return std::make_unique<ash::WebKioskBrowserControllerAsh>(
      *provider, browser, app_id, system_app);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::unique_ptr<AppBrowserController> CreateWebAppBrowserController(
    Browser* browser,
    WebAppProvider* provider,
    const webapps::AppId& app_id) {
  bool should_have_tab_strip_for_swa = false;
#if BUILDFLAG(IS_CHROMEOS)
  const ash::SystemWebAppDelegate* system_app =
      GetSystemWebAppDelegate(browser, app_id);
  should_have_tab_strip_for_swa =
      system_app && system_app->ShouldHaveTabStrip();
#endif  // BUILDFLAG(IS_CHROMEOS)
  const bool has_tab_strip =
      !browser->is_type_app_popup() &&
      (should_have_tab_strip_for_swa ||
       provider->registrar_unsafe().IsTabbedWindowModeEnabled(app_id));
  return std::make_unique<WebAppBrowserController>(*provider, browser, app_id,
#if BUILDFLAG(IS_CHROMEOS)
                                                   system_app,
#endif  // BUILDFLAG(IS_CHROMEOS)
                                                   has_tab_strip);
}

std::unique_ptr<AppBrowserController> MaybeCreateHostedAppBrowserController(
    Browser* browser,
    const webapps::AppId& app_id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser->profile())
          ->GetExtensionById(app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (extension && extension->is_hosted_app()) {
    return std::make_unique<extensions::HostedAppBrowserController>(browser);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return nullptr;
}

base::Value::Dict ToDebugDict(const apps::AppLaunchParams& params) {
  base::Value::Dict value;
  value.Set("app_id", params.app_id);
  value.Set("launch_id", params.launch_id);
  value.Set("container", static_cast<int>(params.container));
  value.Set("disposition", static_cast<int>(params.disposition));
  value.Set("override_url", params.override_url.spec());
  value.Set("override_bounds", params.override_bounds.ToString());
  value.Set("override_app_name", params.override_app_name);
  value.Set("restore_id", params.restore_id);
#if BUILDFLAG(IS_WIN)
  value.Set("command_line",
            base::WideToUTF8(params.command_line.GetCommandLineString()));
#else
  value.Set("command_line", params.command_line.GetCommandLineString());
#endif
  value.Set("current_directory",
            base::FilePathToValue(params.current_directory));
  value.Set("launch_source", static_cast<int>(params.launch_source));
  value.Set("display_id", base::saturated_cast<int>(params.display_id));
  base::Value::List files_list;
  for (const base::FilePath& file : params.launch_files) {
    files_list.Append(base::FilePathToValue(file));
  }
  value.Set("launch_files", std::move(files_list));
  value.Set("intent", params.intent ? "<set>" : "<not set>");
  value.Set("url_handler_launch_url",
            params.url_handler_launch_url.value_or(GURL()).spec());
  value.Set("protocol_handler_launch_url",
            params.protocol_handler_launch_url.value_or(GURL()).spec());
  value.Set("omit_from_session_restore", params.omit_from_session_restore);
  return value;
}

// Returns true if an auxiliary browsing context is getting created, so
// navigation should be done in the same container that it was triggered in.
bool IsAuxiliaryBrowsingContext(const NavigateParams& nav_params) {
  if ((nav_params.contents_to_insert &&
       nav_params.contents_to_insert->HasOpener()) ||
      nav_params.opener) {
    return true;
  }
  return false;
}

bool ShouldEnqueueNavigationHandlingInfoForRedirects(
    const NavigationHandlingInitialResult initial_result) {
  switch (initial_result) {
    case NavigationHandlingInitialResult::kBrowserTab:
    case NavigationHandlingInitialResult::kForcedNewAppContextBrowserTab:
    case NavigationHandlingInitialResult::kForcedNewAppContextAppWindow:
    case NavigationHandlingInitialResult::kNavigateCapturedNewBrowserTab:
    case NavigationHandlingInitialResult::kNavigateCapturingNavigateExisting:
    case NavigationHandlingInitialResult::kNavigateCapturedNewAppWindow:
    // To prevent the 'v1' throttle from enabling, still attach the navigation
    // handling info for aux contexts.
    case NavigationHandlingInitialResult::kAuxContext:
      return true;
    case NavigationHandlingInitialResult::kNotHandledByNavigationHandling:
      return false;
  }
}

// Populate navigation handling information for redirects based on the initial
// result of navigation handling by the web apps system.
void MaybePopulateNavigationHandlingInfoForRedirects(
    base::WeakPtr<content::NavigationHandle> navigation_handle,
    content::WebContents* web_contents,
    const web_app::NavigationCapturingRedirectionInfo& redirection_info) {
  CHECK(web_contents);
  if (!ShouldEnqueueNavigationHandlingInfoForRedirects(
          redirection_info.initial_nav_handling_result())) {
    return;
  }

  if (navigation_handle) {
    web_app::NavigationCapturingNavigationHandleUserData::
        CreateForNavigationHandle(*navigation_handle, redirection_info);
  } else {
    web_app::NavigationCapturingInformationForwarder::CreateForWebContents(
        web_contents, redirection_info);
  }
}

bool IsNavigationCapturingReimplExperimentEnabled(
    const std::optional<webapps::AppId>& current_browser_app_id,
    const GURL& url,
    const std::optional<webapps::AppId>& controlling_app_id,
    const std::optional<blink::mojom::DisplayMode>& display_mode) {
  if (display_mode &&
      !WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
          *display_mode)) {
    return false;
  }
  // Enabling the generic flag turns it on for all navigations.
  if (apps::features::IsNavigationCapturingReimplEnabled()) {
    if (!features::kForcedOffCapturingAppsOnFirstNavigation.Get().empty() &&
        controlling_app_id.has_value()) {
      std::vector<std::string> forced_capturing_off_app_ids = base::SplitString(
          features::kForcedOffCapturingAppsOnFirstNavigation.Get(), ",",
          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      for (const std::string& forced_capturing_off_app_id :
           forced_capturing_off_app_ids) {
        if (controlling_app_id == forced_capturing_off_app_id) {
          return false;
        }
      }
    }
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Check application-specific flags.
  if (controlling_app_id.has_value() &&
      ::web_app::ChromeOsWebAppExperiments::
          IsNavigationCapturingReimplEnabledForTargetApp(*controlling_app_id)) {
    return true;
  }
  if (current_browser_app_id.has_value() &&
      ::web_app::ChromeOsWebAppExperiments::
          IsNavigationCapturingReimplEnabledForSourceApp(
              *current_browser_app_id, url)) {
    return true;
  }
#endif

  return false;
}

// Searches all browsers and tabs to find an applicable browser and (contained)
// tab that matches the given `requested_display_mode`.
std::optional<std::pair<Browser*, int>> GetAppHostForCapturing(
    const Profile& profile,
    const webapps::AppId& app_id,
    blink::mojom::DisplayMode requested_display_mode) {
  std::optional<std::pair<Browser*, int>> first_app_browser_match;
  std::optional<std::pair<Browser*, int>> first_browser_tab_match;
  // These are the type `std::optional<std::pair<Browser*, int>>` for easy usage
  // later when returning.
  std::optional<std::pair<Browser*, int>> first_normal_browser;

  auto GetTabForAppInBrowser = [](Browser* browser,
                                  const webapps::AppId& app_id)
      -> std::optional<std::pair<Browser*, int>> {
    // The active web contents should have preference if it is in scope.
    content::WebContents* active_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (browser->tab_strip_model()->active_index() != TabStripModel::kNoTab) {
      const webapps::AppId* tab_app_id =
          WebAppTabHelper::GetAppId(active_contents);
      if (tab_app_id && *tab_app_id == app_id &&
          !active_contents->HasOpener()) {
        return {{browser, browser->tab_strip_model()->active_index()}};
      }
    }
    // Otherwise, use the first one for the app.
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      const webapps::AppId* tab_app_id = WebAppTabHelper::GetAppId(contents);
      if (tab_app_id && *tab_app_id == app_id &&
          !active_contents->HasOpener()) {
        return {{browser, i}};
      }
    }
    return {};
  };

  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (browser->IsAttemptingToCloseBrowser() || browser->IsBrowserClosing()) {
      continue;
    }
    if (!(browser->is_type_normal() || browser->is_type_app())) {
      continue;
    }
    if (browser->profile() != &profile) {
      continue;
    }
    if (AppBrowserController::IsWebApp(browser)) {
      if (!first_app_browser_match) {
        first_app_browser_match = GetTabForAppInBrowser(browser, app_id);
      }
    } else {
      if (!first_normal_browser.has_value()) {
        first_normal_browser = {browser, -1};
      }
      if (!first_browser_tab_match) {
        first_browser_tab_match = GetTabForAppInBrowser(browser, app_id);
      }
    }

    if (first_browser_tab_match.has_value() &&
        first_app_browser_match.has_value()) {
      break;
    }
  }
  switch (requested_display_mode) {
    case blink::mojom::DisplayMode::kUndefined:
    case blink::mojom::DisplayMode::kBrowser:
      if (first_browser_tab_match) {
        return first_browser_tab_match;
      }
      return first_normal_browser;
    case blink::mojom::DisplayMode::kMinimalUi:
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kWindowControlsOverlay:
    case blink::mojom::DisplayMode::kBorderless:
      if (first_app_browser_match) {
        return first_app_browser_match;
      }
      if (first_browser_tab_match) {
        return first_browser_tab_match;
      }
      return std::nullopt;
      // TODO(crbug.com/375504532): Support tabbed mode on desktop.
    case blink::mojom::DisplayMode::kTabbed:
    case blink::mojom::DisplayMode::kFullscreen:
    case blink::mojom::DisplayMode::kPictureInPicture:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

// static
AppNavigationResult AppNavigationResult::CapturingDisabled() {
  return AppNavigationResult(
      /*capturing_feature_enabled=*/false, std::nullopt,
      /*perform_app_handling_tasks_in_web_contents=*/false,
      NavigationCapturingRedirectionInfo::Disabled(), base::Value::Dict());
}

// static
AppNavigationResult AppNavigationResult::CancelNavigation() {
  return AppNavigationResult(
      /*capturing_feature_enabled=*/false,
      /*browser_tab_override=*/{{nullptr, -1}},
      /*perform_app_handling_tasks_in_web_contents=*/false,
      NavigationCapturingRedirectionInfo::Disabled(), base::Value::Dict());
}

// static
AppNavigationResult AppNavigationResult::NoCapturingOverrideBrowser(
    Browser* browser) {
  return AppNavigationResult(
      /*capturing_feature_enabled=*/false,
      /*browser_tab_override=*/{{browser, -1}},
      /*perform_app_handling_tasks_in_web_contents=*/false,
      NavigationCapturingRedirectionInfo::Disabled(), base::Value::Dict());
}

// static
AppNavigationResult AppNavigationResult::AuxiliaryContext(
    WindowOpenDisposition disposition,
    base::Value::Dict debug_data) {
  return AppNavigationResult(
      /*capturing_feature_enabled=*/true, /*browser_tab_override=*/std::nullopt,
      /*perform_app_handling_tasks_in_web_contents=*/false,
      NavigationCapturingRedirectionInfo::AuxiliaryContext(
          /*source_browser_app_id=*/std::nullopt, disposition),
      std::move(debug_data));
}

// static
AppNavigationResult AppNavigationResult::AuxiliaryContextInAppWindow(
    const webapps::AppId& source_browser_app_id,
    WindowOpenDisposition disposition,
    Browser* app_browser,
    base::Value::Dict debug_data) {
  CHECK(app_browser->app_controller());
  return AppNavigationResult(
      /*capturing_feature_enabled=*/true,
      /*browser_tab_override=*/{{app_browser, -1}},
      /*perform_app_handling_tasks_in_web_contents=*/false,
      NavigationCapturingRedirectionInfo::AuxiliaryContext(
          source_browser_app_id, disposition),
      std::move(debug_data));
}

// static
AppNavigationResult
AppNavigationResult::NoInitialActionRedirectionHandlingEligible(
    std::optional<webapps::AppId> source_browser_app_id,
    WindowOpenDisposition disposition,
    base::Value::Dict debug_data) {
  return AppNavigationResult(
      /*capturing_feature_enabled=*/true,
      /*browser_tab_override=*/std::nullopt,
      /*perform_app_handling_tasks_in_web_contents=*/false,
      NavigationCapturingRedirectionInfo::
          NoInitialActionRedirectionHandlingEligible(source_browser_app_id,
                                                     disposition),
      std::move(debug_data));
}

// static
AppNavigationResult AppNavigationResult::ForcedNewAppContext(
    std::optional<webapps::AppId> source_browser_app_id,
    const webapps::AppId capturing_app_id,
    blink::mojom::DisplayMode new_client_display_mode,
    Browser* host_browser,
    WindowOpenDisposition disposition,
    base::Value::Dict debug_data) {
  CHECK(WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
      new_client_display_mode));
  CHECK((new_client_display_mode != blink::mojom::DisplayMode::kBrowser) ==
        (!!host_browser->app_controller()));
  CHECK(disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
        disposition == WindowOpenDisposition::NEW_WINDOW);
  return AppNavigationResult(
      /*capturing_feature_enabled=*/true,
      /*browser_tab_override=*/{{host_browser, -1}},
      /*perform_app_handling_tasks_in_web_contents=*/true,
      NavigationCapturingRedirectionInfo::ForcedNewContext(
          source_browser_app_id, capturing_app_id, new_client_display_mode,
          disposition),
      std::move(debug_data));
}

// static
AppNavigationResult AppNavigationResult::CapturedNewClient(
    std::optional<webapps::AppId> source_browser_app_id,
    const webapps::AppId capturing_app_id,
    blink::mojom::DisplayMode new_client_display_mode,
    Browser* host_browser,
    WindowOpenDisposition disposition,
    base::Value::Dict debug_data) {
  CHECK(WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
      new_client_display_mode));
  CHECK((new_client_display_mode != blink::mojom::DisplayMode::kBrowser) ==
        (!!host_browser->app_controller()));
  CHECK(disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB);
  return AppNavigationResult(
      /*capturing_feature_enabled=*/true,
      /*browser_tab_override=*/{{host_browser, -1}},
      /*perform_app_handling_tasks_in_web_contents=*/true,
      NavigationCapturingRedirectionInfo::CapturedNewContext(
          source_browser_app_id, capturing_app_id, new_client_display_mode,
          disposition),
      std::move(debug_data));
}

// static
AppNavigationResult AppNavigationResult::CapturedNavigateExisting(
    std::optional<webapps::AppId> source_browser_app_id,
    const webapps::AppId capturing_app_id,
    Browser* app_browser,
    int browser_tab,
    WindowOpenDisposition disposition,
    base::Value::Dict debug_data) {
  CHECK(disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB);
  CHECK(browser_tab != -1);
  return AppNavigationResult(
      /*capturing_feature_enabled=*/true,
      /*browser_tab_override=*/{{app_browser, browser_tab}},
      /*perform_app_handling_tasks_in_web_contents=*/true,
      NavigationCapturingRedirectionInfo::CapturedNavigateExisting(
          source_browser_app_id, capturing_app_id, disposition),
      std::move(debug_data));
}

AppNavigationResult::AppNavigationResult(AppNavigationResult&&) = default;
AppNavigationResult& AppNavigationResult::operator=(AppNavigationResult&&) =
    default;

AppNavigationResult::AppNavigationResult(
    bool capturing_feature_enabled,
    std::optional<std::tuple<Browser*, int>> browser_tab_override,
    bool perform_app_handling_tasks_in_web_contents,
    const NavigationCapturingRedirectionInfo& redirection_info,
    base::Value::Dict debug_value)
    : capturing_feature_enabled_(capturing_feature_enabled),
      browser_tab_override_(std::move(browser_tab_override)),
      perform_app_handling_tasks_in_web_contents_(
          perform_app_handling_tasks_in_web_contents),
      redirection_info_(redirection_info),
      debug_value_(std::move(debug_value)) {
  debug_value_.Set("redirection_info", redirection_info_.ToDebugData());
  debug_value_.Set(
      "result",
      base::Value::Dict()
          .Set("capturing_feature_enabled", capturing_feature_enabled_)
          .Set("browser_tab_override",
               browser_tab_override_.has_value()
                   ? base::Value(
                         base::Value::Dict()
                             .Set("browser", base::ToString(std::get<Browser*>(
                                                 *browser_tab_override_)))
                             .Set("tab", std::get<int>(*browser_tab_override_)))
                   : base::Value("<none>"))
          .Set("perform_app_handling_tasks_in_web_contents",
               perform_app_handling_tasks_in_web_contents_));
}

base::Value::Dict AppNavigationResult::TakeDebugData() {
  return std::move(debug_value_);
}

void ReparentWebContentsIntoBrowserImpl(Browser* source_browser,
                                        content::WebContents* web_contents,
                                        Browser* target_browser,
                                        bool insert_as_first_tab) {
  CHECK(source_browser);
  CHECK(web_contents);
  CHECK(target_browser);

  // In a reparent, the owning session service needs to be told it's tab
  // has been removed, otherwise it will reopen the tab on restoration.
  SessionServiceBase* service =
      GetAppropriateSessionServiceForProfile(source_browser);
  service->TabClosing(web_contents);

  // Check-fail if the web contents given is not part of the source browser.
  std::optional<int> found_tab_index;
  for (int i = 0; i < source_browser->tab_strip_model()->count(); ++i) {
    if (source_browser->tab_strip_model()->GetWebContentsAt(i) ==
        web_contents) {
      found_tab_index = i;
      break;
    }
  }
  CHECK(found_tab_index);

  TabStripModel* const source_tabstrip = source_browser->tab_strip_model();
  const std::optional<webapps::AppId> source_app_id =
      AppBrowserController::IsWebApp(source_browser)
          ? source_browser->app_controller()->app_id()
          : std::optional<webapps::AppId>(std::nullopt);
  const std::optional<webapps::AppId> target_app_id =
      AppBrowserController::IsWebApp(target_browser)
          ? target_browser->app_controller()->app_id()
          : std::optional<webapps::AppId>(std::nullopt);

  // Always reset the window controls overlay titlebar area when going to a
  // browser window or the app ids are different. The code will no-op if the old
  // rect matches the new rect.
  if (!target_app_id || target_app_id != source_app_id) {
    web_contents->UpdateWindowControlsOverlay(gfx::Rect());
  }

  std::unique_ptr<tabs::TabModel> tab_model =
      source_tabstrip->DetachTabAtForInsertion(found_tab_index.value());
  std::unique_ptr<content::WebContents> contents_move =
      tabs::TabModel::DestroyAndTakeWebContents(std::move(tab_model));
  int location = target_browser->tab_strip_model()->count();
  int add_types = (AddTabTypes::ADD_INHERIT_OPENER | AddTabTypes::ADD_ACTIVE);
  if (insert_as_first_tab) {
    location = 0;
    add_types |= AddTabTypes::ADD_PINNED;
  }
  // This method moves a WebContents from a non-normal browser window to a
  // normal browser window. We cannot move the Tab over directly since TabModel
  // enforces the requirement that it cannot move between window types.
  // https://crbug.com/334281979): Non-normal browser windows should not have a
  // tab to begin with.
  target_browser->tab_strip_model()->InsertWebContentsAt(
      location, std::move(contents_move), add_types);
  CHECK_EQ(web_contents,
           target_browser->tab_strip_model()->GetActiveWebContents());

  if (!target_app_id) {
    IntentPickerTabHelper* helper =
        IntentPickerTabHelper::FromWebContents(web_contents);
    CHECK(helper);
    helper->MaybeShowIntentPickerIcon();
  }
#if !BUILDFLAG(IS_CHROMEOS)
  if (source_app_id && source_app_id != target_app_id) {
    apps::EnableLinkCapturingInfoBarDelegate::RemoveInfoBar(web_contents);
  }
#endif
  target_browser->window()->Show();

  // The window will be registered correctly, however the tab will not be
  // correctly tracked. We need to do a reset to get the tab correctly tracked
  // by either the app service or the regular service
  SessionServiceBase* target_service =
      GetAppropriateSessionServiceForProfile(target_browser);
  target_service->ResetFromCurrentBrowsers();
}

std::optional<webapps::AppId> GetWebAppForActiveTab(const Browser* browser) {
  const WebAppProvider* const provider =
      WebAppProvider::GetForWebApps(browser->profile());
  if (!provider) {
    return std::nullopt;
  }

  const content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return std::nullopt;
  }

  return provider->registrar_unsafe().FindInstalledAppWithUrlInScope(
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
}

void PrunePreScopeNavigationHistory(const GURL& scope,
                                    content::WebContents* contents) {
  content::NavigationController& navigation_controller =
      contents->GetController();
  if (!navigation_controller.CanPruneAllButLastCommitted()) {
    return;
  }

  int index = navigation_controller.GetEntryCount() - 1;
  while (index >= 0 &&
         IsInScope(navigation_controller.GetEntryAtIndex(index)->GetURL(),
                   scope)) {
    --index;
  }

  while (index >= 0) {
    navigation_controller.RemoveEntryAtIndex(index);
    --index;
  }
}

bool MaybeHandleIntentPickerFocusExistingOrNavigateExisting(
    Profile* profile,
    const GURL& launch_url,
    content::WebContents* contents,
    const webapps::AppId& app_id,
    WebAppRegistrar& registrar) {
  ClientModeAndBrowser client_mode_and_browser =
      GetEffectiveClientModeAndBrowserForCapturing(*profile, app_id);
  LaunchHandler::ClientMode client_mode =
      client_mode_and_browser.effective_client_mode;
  if (client_mode != LaunchHandler::ClientMode::kFocusExisting &&
      client_mode != LaunchHandler::ClientMode::kNavigateExisting) {
    return false;
  }

  CHECK(client_mode_and_browser.browser);
  CHECK(client_mode_and_browser.tab_index.has_value());

  content::WebContents* preexisting_web_contents =
      client_mode_and_browser.browser->tab_strip_model()->GetWebContentsAt(
          *client_mode_and_browser.tab_index);
  CHECK(preexisting_web_contents != contents);

  // We've found a browser in the background. We need to focus it and enqueue
  // launch params. But first we ensure that the contents (for which the Intent
  // Picker was clicked) goes away without its containing browser closing.
  Browser* foreground_browser = chrome::FindBrowserWithTab(contents);
  if (foreground_browser->tab_strip_model()->count() == 1) {
    chrome::NewTab(foreground_browser);
  }

  contents->Close();

  preexisting_web_contents->Focus();
  if (client_mode == LaunchHandler::ClientMode::kNavigateExisting) {
    NavigateParams nav_params(client_mode_and_browser.browser, launch_url,
                              ui::PageTransition::PAGE_TRANSITION_LINK);
    Navigate(&nav_params);
  }

  EnqueueLaunchParams(preexisting_web_contents, app_id, launch_url,
                      /*wait_for_navigation_to_complete=*/client_mode ==
                          LaunchHandler::ClientMode::kNavigateExisting);
  return true;
}

Browser* ReparentWebAppForActiveTab(Browser* browser) {
  std::optional<webapps::AppId> app_id = GetWebAppForActiveTab(browser);
  if (!app_id) {
    return nullptr;
  }
  return ReparentWebContentsIntoAppBrowser(
      browser->tab_strip_model()->GetActiveWebContents(), *app_id);
}

Browser* ReparentWebContentsIntoAppBrowser(
    content::WebContents* contents,
    const webapps::AppId& app_id,
    base::OnceCallback<void(content::WebContents*)> completion_callback) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  // Incognito tabs reparent correctly, but remain incognito without any
  // indication to the user, so disallow it.
  DCHECK(!profile->IsOffTheRecord());

  // Clear navigation history that occurred before the user most recently
  // entered the app's scope. The minimal-ui Back button will be initially
  // disabled if the previous page was outside scope. Packaged apps are not
  // affected.
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile);
  WebAppRegistrar& registrar = provider->registrar_unsafe();
  const WebApp* web_app = registrar.GetAppById(app_id);
  if (!web_app) {
    std::move(completion_callback).Run(contents);
    return nullptr;
  }

  if (registrar.IsInstalled(app_id)) {
    std::optional<GURL> app_scope = registrar.GetAppScope(app_id);
    if (!app_scope) {
      app_scope = registrar.GetAppStartUrl(app_id).GetWithoutFilename();
    }

    PrunePreScopeNavigationHistory(*app_scope, contents);
  }

  auto launch_url = contents->GetLastCommittedURL();
  UpdateLaunchStats(contents, app_id, launch_url);
  RecordLaunchMetrics(app_id, apps::LaunchContainer::kLaunchContainerWindow,
                      apps::LaunchSource::kFromReparenting, launch_url,
                      contents);
  blink::mojom::DisplayMode display_mode =
      registrar.GetAppEffectiveDisplayMode(app_id);

  // The current browser, in this situation, is a browser tab, so std::nullopt
  // is appropriate for `current_browser_app_id`.
  if (IsNavigationCapturingReimplExperimentEnabled(
          /*current_browser_app_id=*/std::nullopt, launch_url, app_id,
          display_mode)) {
    // The Intent Picker needs to respect kFocusExisting and kNavigateExisting,
    // by focusing such apps in the background instead of re-parenting the
    // current contents.
    if (MaybeHandleIntentPickerFocusExistingOrNavigateExisting(
            profile, launch_url, contents, app_id, registrar)) {
      return nullptr;
    }
  }

  if (web_app->launch_handler()
          .value_or(LaunchHandler{})
          .TargetsExistingClients() ||
      registrar.IsPreventCloseEnabled(web_app->app_id())) {
    if (AppBrowserController::FindForWebApp(*profile, app_id)) {
      // TODO(crbug.com/40246677): Use apps::AppServiceProxy::LaunchAppWithUrl()
      // instead to ensure all the usual wrapping code around web app launches
      // gets executed.
      apps::AppLaunchParams params(
          app_id, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::CURRENT_TAB, apps::LaunchSource::kFromOmnibox);
      params.override_url = launch_url;
      content::WebContents* new_web_contents =
          WebAppLaunchProcess::CreateAndRun(
              *profile, registrar, provider->os_integration_manager(), params);
      contents->Close();
      std::move(completion_callback).Run(new_web_contents);
      return chrome::FindBrowserWithTab(new_web_contents);
    }
  }

  Browser* browser = nullptr;

  if (registrar.IsTabbedWindowModeEnabled(app_id)) {
    browser = AppBrowserController::FindForWebApp(*profile, app_id);
  }

  if (!browser) {
    browser = Browser::Create(Browser::CreateParams::CreateForApp(
        GenerateApplicationNameFromAppId(app_id), true /* trusted_source */,
        gfx::Rect(), profile, true /* user_gesture */));

    // If the current url isn't in scope, then set the initial url on the
    // AppBrowserController so that the 'x' button still shows up.
    CHECK(browser->app_controller());
    browser->app_controller()->MaybeSetInitialUrlOnReparentTab();
  }

  bool as_pinned_home_tab =
      browser->app_controller()->IsUrlInHomeTabScope(launch_url);

  Browser* reparented_browser = ReparentWebContentsIntoAppBrowser(
      contents, browser, app_id, as_pinned_home_tab);
  std::move(completion_callback).Run(contents);
  return reparented_browser;
}

void SetWebContentsIsPinnedHomeTab(content::WebContents* contents) {
  auto* helper = WebAppTabHelper::FromWebContents(contents);
  helper->set_is_pinned_home_tab(true);
}

std::unique_ptr<AppBrowserController> MaybeCreateAppBrowserController(
    Browser* browser) {
  std::unique_ptr<AppBrowserController> controller;
  const webapps::AppId app_id =
      GetAppIdFromApplicationName(browser->app_name());
  auto* const provider =
      WebAppProvider::GetForLocalAppsUnchecked(browser->profile());
  if (provider && provider->registrar_unsafe().IsInstalled(app_id)) {
#if BUILDFLAG(IS_CHROMEOS)
    if (chromeos::IsKioskSession()) {
      controller = CreateWebKioskBrowserController(browser, provider, app_id);
    } else {
      controller = CreateWebAppBrowserController(browser, provider, app_id);
    }
#else
    controller = CreateWebAppBrowserController(browser, provider, app_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
  } else {
    controller = MaybeCreateHostedAppBrowserController(browser, app_id);
  }
  if (controller) {
    controller->Init();
  }
  return controller;
}

void MaybeAddPinnedHomeTab(Browser* browser, const std::string& app_id) {
  WebAppRegistrar& registrar =
      WebAppProvider::GetForLocalAppsUnchecked(browser->profile())
          ->registrar_unsafe();
  std::optional<GURL> pinned_home_tab_url =
      registrar.GetAppPinnedHomeTabUrl(app_id);

  if (registrar.IsTabbedWindowModeEnabled(app_id) &&
      !HasPinnedHomeTab(browser->tab_strip_model()) &&
      pinned_home_tab_url.has_value()) {
    NavigateParams home_tab_nav_params(browser, pinned_home_tab_url.value(),
                                       ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    home_tab_nav_params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    home_tab_nav_params.tabstrip_add_types |= AddTabTypes::ADD_PINNED;
    Navigate(&home_tab_nav_params);

    content::WebContents* const web_contents =
        home_tab_nav_params.navigated_or_inserted_contents;

    if (web_contents) {
      SetWebContentsIsPinnedHomeTab(web_contents);
    }
  }
}

void MaybeShowNavigationCaptureIph(webapps::AppId app_id,
                                   Profile* profile,
                                   Browser* browser) {
  // Prevent ChromeOS from reaching this function in tests.
#if !BUILDFLAG(IS_CHROMEOS)
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  provider->ui_manager().MaybeShowIPHPromoForAppsLaunchedViaLinkCapturing(
      browser, profile, app_id);
#endif
}

Browser::CreateParams CreateParamsForApp(const webapps::AppId& app_id,
                                         bool is_popup,
                                         bool trusted_source,
                                         const gfx::Rect& window_bounds,
                                         Profile* profile,
                                         bool user_gesture) {
  std::string app_name = GenerateApplicationNameFromAppId(app_id);
  Browser::CreateParams params =
      is_popup
          ? Browser::CreateParams::CreateForAppPopup(
                app_name, trusted_source, window_bounds, profile, user_gesture)
          : Browser::CreateParams::CreateForApp(
                app_name, trusted_source, window_bounds, profile, user_gesture);
  params.initial_show_state = IsRunningInForcedAppMode()
                                  ? ui::mojom::WindowShowState::kFullscreen
                                  : ui::mojom::WindowShowState::kDefault;
  return params;
}

Browser* CreateWebAppWindowMaybeWithHomeTab(
    const webapps::AppId& app_id,
    const Browser::CreateParams& params) {
  CHECK(params.type == Browser::Type::TYPE_APP_POPUP ||
        params.type == Browser::Type::TYPE_APP);
  Browser* browser = Browser::Create(params);
  CHECK(GenerateApplicationNameFromAppId(app_id) == browser->app_name());
  if (params.type != Browser::Type::TYPE_APP_POPUP) {
    MaybeAddPinnedHomeTab(browser, app_id);
  }
  return browser;
}

Browser* CreateWebAppWindowFromNavigationParams(
    const webapps::AppId& app_id,
    const NavigateParams& navigate_params,
    bool should_create_app_popup = false) {
  Browser::CreateParams app_browser_params = CreateParamsForApp(
      app_id, should_create_app_popup, /*trusted_source=*/true,
      navigate_params.window_features.bounds,
      navigate_params.initiating_profile, navigate_params.user_gesture);
  Browser* created_browser =
      CreateWebAppWindowMaybeWithHomeTab(app_id, app_browser_params);
  return created_browser;
}

content::WebContents* NavigateWebAppUsingParams(const std::string& app_id,
                                                NavigateParams& nav_params) {
  if (nav_params.browser->app_controller() &&
      nav_params.browser->app_controller()->IsUrlInHomeTabScope(
          nav_params.url)) {
    // Navigations to the home tab URL in tabbed apps should happen in the home
    // tab.
    nav_params.browser->tab_strip_model()->ActivateTabAt(0);
    content::WebContents* home_tab_web_contents =
        nav_params.browser->tab_strip_model()->GetWebContentsAt(0);
    GURL previous_home_tab_url = home_tab_web_contents->GetLastCommittedURL();
    if (previous_home_tab_url == nav_params.url) {
      // URL is identical so no need for the navigation.
      return home_tab_web_contents;
    }
    nav_params.disposition = WindowOpenDisposition::CURRENT_TAB;
  }

#if BUILDFLAG(IS_CHROMEOS)
  Browser* browser = nav_params.browser;
  const std::optional<ash::SystemWebAppType> capturing_system_app_type =
      ash::GetCapturingSystemAppForURL(browser->profile(), nav_params.url);
  if (capturing_system_app_type &&
      (!browser ||
       !IsBrowserForSystemWebApp(browser, capturing_system_app_type.value()))) {
    // Web app launch process should receive the correct `NavigateParams`
    // argument from system web app launches, so that Navigate() call below
    // succeeds (i.e. don't trigger system web app link capture).
    //
    // This block safe guards against misuse of APIs (that can cause
    // GetCapturingSystemAppForURL returning the wrong value).
    //
    // TODO(http://crbug.com/1408946): Remove this block when we find a better
    // way to prevent API misuse (e.g. by ensuring test coverage for new
    // features that could trigger this code) or this code path is no longer
    // possible.
    base::debug::DumpWithoutCrashing();
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  Navigate(&nav_params);

  content::WebContents* const web_contents =
      nav_params.navigated_or_inserted_contents;

  return web_contents;
}

void RecordAppWindowLaunchMetric(Profile* profile,
                                 const std::string& app_id,
                                 apps::LaunchSource launch_source) {
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return;
  }

  const WebApp* web_app = provider->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    return;
  }

  DisplayMode display =
      provider->registrar_unsafe().GetEffectiveDisplayModeFromManifest(app_id);
  if (display != DisplayMode::kUndefined) {
    DCHECK_LT(DisplayMode::kUndefined, display);
    DCHECK_LE(display, DisplayMode::kMaxValue);
    base::UmaHistogramEnumeration("Launch.WebAppDisplayMode", display);
    if (provider->registrar_unsafe().IsShortcutApp(app_id)) {
      base::UmaHistogramEnumeration(
          "Launch.Window.CreateShortcutApp.WebAppDisplayMode", display);
    }
  }

  // Reparenting launches don't respect the launch_handler setting.
  if (launch_source != apps::LaunchSource::kFromReparenting) {
    base::UmaHistogramEnumeration(
        "Launch.WebAppLaunchHandlerClientMode",
        web_app->launch_handler().value_or(LaunchHandler()).client_mode);
  }

  base::UmaHistogramEnumeration("Launch.WebApp.DiyOrCrafted",
                                web_app->is_diy_app()
                                    ? LaunchedAppType::kDiy
                                    : LaunchedAppType::kCrafted);
}

void RecordAppTabLaunchMetric(Profile* profile,
                              const std::string& app_id,
                              apps::LaunchSource launch_source) {
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return;
  }

  const WebApp* web_app = provider->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    return;
  }

  DisplayMode display =
      provider->registrar_unsafe().GetEffectiveDisplayModeFromManifest(app_id);
  if (display != DisplayMode::kUndefined) {
    DCHECK_LT(DisplayMode::kUndefined, display);
    DCHECK_LE(display, DisplayMode::kMaxValue);
    base::UmaHistogramEnumeration("Launch.BrowserTab.WebAppDisplayMode",
                                  display);
    if (provider->registrar_unsafe().IsShortcutApp(app_id)) {
      base::UmaHistogramEnumeration(
          "Launch.BrowserTab.CreateShortcutApp.WebAppDisplayMode", display);
    }
  }

  // Reparenting launches don't respect the launch_handler setting.
  if (launch_source != apps::LaunchSource::kFromReparenting) {
    base::UmaHistogramEnumeration(
        "Launch.BrowserTab.WebAppLaunchHandlerClientMode",
        web_app->launch_handler().value_or(LaunchHandler()).client_mode);
  }
}

void RecordLaunchMetrics(const webapps::AppId& app_id,
                         apps::LaunchContainer container,
                         apps::LaunchSource launch_source,
                         const GURL& launch_url,
                         content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

#if BUILDFLAG(IS_CHROMEOS)
  // System web apps have different launch paths compared with web apps, and
  // those paths aren't configurable. So their launch metrics shouldn't be
  // reported to avoid skewing web app metrics.
  DCHECK(!ash::GetSystemWebAppTypeForAppId(profile, app_id))
      << "System web apps shouldn't be included in web app launch metrics";
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (container == apps::LaunchContainer::kLaunchContainerWindow) {
    RecordAppWindowLaunchMetric(profile, app_id, launch_source);
  }
  if (container == apps::LaunchContainer::kLaunchContainerTab) {
    RecordAppTabLaunchMetric(profile, app_id, launch_source);
  }

  base::UmaHistogramEnumeration("WebApp.LaunchSource", launch_source);
  base::UmaHistogramEnumeration("WebApp.LaunchContainer", container);
}

void UpdateLaunchStats(content::WebContents* web_contents,
                       const webapps::AppId& app_id,
                       const GURL& launch_url) {
  CHECK(web_contents != nullptr);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  WebAppProvider::GetForLocalAppsUnchecked(profile)
      ->sync_bridge_unsafe()
      .SetAppLastLaunchTime(app_id, base::Time::Now());

#if BUILDFLAG(IS_CHROMEOS)
  if (ash::GetSystemWebAppTypeForAppId(profile, app_id)) {
    // System web apps doesn't use the rest of the stats.
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Update the launch time in the site engagement service. A recent web
  // app launch will provide an engagement boost to the origin.
  site_engagement::SiteEngagementService::Get(profile)
      ->SetLastShortcutLaunchTime(web_contents, app_id, launch_url);
}

void LaunchWebApp(apps::AppLaunchParams params,
                  LaunchWebAppWindowSetting launch_setting,
                  Profile& profile,
                  WithAppResources& lock,
                  LaunchWebAppDebugValueCallback callback) {
  base::Value::Dict debug_value;
  debug_value.Set("launch_params", ToDebugDict(params));
  debug_value.Set("launch_window_setting", static_cast<int>(launch_setting));

  if (launch_setting == LaunchWebAppWindowSetting::kOverrideWithWebAppConfig) {
    DisplayMode display_mode =
        lock.registrar().GetAppEffectiveDisplayMode(params.app_id);
    switch (display_mode) {
      case DisplayMode::kUndefined:
      case DisplayMode::kFullscreen:
      case DisplayMode::kBrowser:
        params.container = apps::LaunchContainer::kLaunchContainerTab;
        break;
      case DisplayMode::kMinimalUi:
      case DisplayMode::kWindowControlsOverlay:
      case DisplayMode::kTabbed:
      case DisplayMode::kBorderless:
      case DisplayMode::kPictureInPicture:
      case DisplayMode::kStandalone:
        params.container = apps::LaunchContainer::kLaunchContainerWindow;
        break;
    }
  }

  DCHECK_NE(params.container, apps::LaunchContainer::kLaunchContainerNone);

  apps::LaunchContainer container;
  Browser* browser = nullptr;
  content::WebContents* web_contents = nullptr;
  // Do not launch anything if the profile is being deleted.
  if (Browser::GetCreationStatusForProfile(&profile) ==
      Browser::CreationStatus::kOk) {
    if (lock.registrar().IsInstalled(params.app_id)) {
      container = params.container;
      if (WebAppLaunchProcess::GetOpenApplicationCallbackForTesting()) {
        WebAppLaunchProcess::GetOpenApplicationCallbackForTesting().Run(
            std::move(params));
      } else {
        web_contents = WebAppLaunchProcess::CreateAndRun(
            profile, lock.registrar(), lock.os_integration_manager(), params);
      }
      if (web_contents) {
        browser = chrome::FindBrowserWithTab(web_contents);
      }
    } else {
      debug_value.Set("error", "Unknown app id.");
      // Open an empty browser window as the app_id is invalid.
      DVLOG(1) << "Cannot launch app with unknown id: " << params.app_id;
      container = apps::LaunchContainer::kLaunchContainerNone;
      browser = apps::CreateBrowserWithNewTabPage(&profile);
    }
  } else {
    std::string error_str = base::StringPrintf(
        "Cannot launch app %s without profile creation: %d",
        params.app_id.c_str(),
        static_cast<int>(Browser::GetCreationStatusForProfile(&profile)));
    debug_value.Set("error", error_str);
    DVLOG(1) << error_str;
  }

  debug_value.Set("web_contents", base::ToString(web_contents));
  debug_value.Set("browser", base::ToString(browser));

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     browser ? browser->AsWeakPtr() : nullptr,
                     web_contents ? web_contents->GetWeakPtr() : nullptr,
                     container, base::Value(std::move(debug_value))));
}

ClientModeAndBrowser GetEffectiveClientModeAndBrowserForCapturing(
    Profile& profile,
    const webapps::AppId& app_id) {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(&profile);
  CHECK(provider);
  web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();

  ClientModeAndBrowser result;
  result.effective_client_mode = registrar.GetAppById(app_id)
                                     ->launch_handler()
                                     .value_or(LaunchHandler())
                                     .client_mode;
  if (result.effective_client_mode == LaunchHandler::ClientMode::kAuto) {
    result.effective_client_mode = LaunchHandler::ClientMode::kNavigateNew;
  }

  std::optional<std::pair<Browser*, int>> app_host = GetAppHostForCapturing(
      profile, app_id, registrar.GetAppEffectiveDisplayMode(app_id));
  if (app_host.has_value()) {
    CHECK(app_host->first);
    result.browser = app_host->first;
    if (app_host->second != -1) {
      result.tab_index = app_host->second;
    }
  }

  if (result.effective_client_mode ==
          LaunchHandler::ClientMode::kNavigateExisting ||
      result.effective_client_mode ==
          LaunchHandler::ClientMode::kFocusExisting) {
    if (!result.tab_index.has_value()) {
      result.effective_client_mode = LaunchHandler::ClientMode::kNavigateNew;
    }
  }
  return result;
}

AppNavigationResult MaybeHandleAppNavigation(const NavigateParams& params) {
  Profile* profile = params.initiating_profile;

  if (!AreWebAppsEnabled(profile)) {
    return AppNavigationResult::CapturingDisabled();
  }
  base::Value::Dict debug_data;

  if (params.open_pwa_window_if_possible) {
    std::optional<webapps::AppId> app_id =
        web_app::FindInstalledAppWithUrlInScope(profile, params.url,
                                                /*window_only=*/true);
    if (!app_id && params.force_open_pwa_window) {
      // In theory |force_open_pwa_window| should only be set if we know a
      // matching PWA is installed. However, we can reach here if
      // `WebAppRegistrar` hasn't finished starting yet, which can happen if
      // Chrome is launched with the URL of an isolated app as an argument.
      // This isn't a supported way to launch isolated apps, so we can cancel
      // the navigation, but if we want to support it in the future we'll need
      // to block until `WebAppRegistrar` is loaded.
      return AppNavigationResult::CancelNavigation();
    }
    if (app_id) {
      // Reuse the existing browser for in-app same window navigations.
      bool navigating_same_app =
          params.browser &&
          web_app::AppBrowserController::IsForWebApp(params.browser, *app_id);
      if (navigating_same_app) {
        if (params.disposition == WindowOpenDisposition::CURRENT_TAB) {
          return AppNavigationResult::NoCapturingOverrideBrowser(
              params.browser);
        }

        // If the browser window does not yet have any tabs, and we are
        // attempting to add the first tab to it, allow for it to be reused.
        bool navigating_new_tab =
            params.disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
            params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
        bool browser_has_no_tabs =
            params.browser && params.browser->tab_strip_model()->empty();
        if (navigating_new_tab && browser_has_no_tabs) {
          return AppNavigationResult::NoCapturingOverrideBrowser(
              params.browser);
        }
      }

      auto GetOriginSpecified = [](const NavigateParams& params) {
        return params.window_features.has_x && params.window_features.has_y
                   ? Browser::ValueSpecified::kSpecified
                   : Browser::ValueSpecified::kUnspecified;
      };

      // App popups are handled in the switch statement in
      // `GetBrowserAndTabForDisposition()`.
      if (params.disposition != WindowOpenDisposition::NEW_POPUP &&
          Browser::GetCreationStatusForProfile(profile) ==
              Browser::CreationStatus::kOk) {
        std::string app_name =
            web_app::GenerateApplicationNameFromAppId(*app_id);
        // Installed PWAs are considered trusted.
        Browser::CreateParams browser_params =
            Browser::CreateParams::CreateForApp(
                app_name, /*trusted_source=*/true,
                params.window_features.bounds, profile, params.user_gesture);
        browser_params.initial_origin_specified = GetOriginSpecified(params);
        Browser* browser = Browser::Create(browser_params);
        return AppNavigationResult::NoCapturingOverrideBrowser(browser);
      }
    }
  }

  // Below here handles the states outlined in
  // https://bit.ly/pwa-navigation-capturing
  if (params.started_from_context_menu) {
    return AppNavigationResult::CapturingDisabled();
  }

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    return AppNavigationResult::CapturingDisabled();
  }
  web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();

  std::optional<webapps::AppId> source_browser_app_id =
      params.browser && web_app::AppBrowserController::IsWebApp(params.browser)
          ? std::optional(params.browser->app_controller()->app_id())
          : std::nullopt;
  std::optional<webapps::AppId> controlling_app_id =
      registrar.FindAppThatCapturesLinksInScope(params.url);
  std::optional<DisplayMode> controlling_app_display_mode;
  if (controlling_app_id) {
    controlling_app_display_mode =
        registrar.GetAppEffectiveDisplayMode(*controlling_app_id);
    CHECK(WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
        *controlling_app_display_mode));
  }

  // Only proceed as below if the navigation capturing is enabled. The flag in
  // the redirection info has to store the result of this check, so that the
  // logic in `OnWebAppNavigationAfterWebContentsCreation()` is skipped when not
  // needed.
  if (!IsNavigationCapturingReimplExperimentEnabled(
          source_browser_app_id, params.url, controlling_app_id,
          controlling_app_display_mode)) {
    return AppNavigationResult::CapturingDisabled();
  }
  std::optional<ClientModeAndBrowser> client_mode_and_browser;
  if (controlling_app_id) {
    client_mode_and_browser = GetEffectiveClientModeAndBrowserForCapturing(
        *profile, *controlling_app_id);
    debug_data.Set(
        "effective_client_mode",
        base::ToString(client_mode_and_browser->effective_client_mode));
    CHECK(client_mode_and_browser->effective_client_mode !=
          LaunchHandler::ClientMode::kAuto);
    debug_data.Set("controlling_app_host_browser",
                   client_mode_and_browser->browser
                       ? base::ToString(client_mode_and_browser->browser)
                       : "<none>");
    debug_data.Set("controlling_app_host_tab",
                   client_mode_and_browser->tab_index.has_value()
                       ? base::ToString(*client_mode_and_browser->tab_index)
                       : "<none>");
  }

  content::WebContents* source_contents = params.source_contents;
  // Note: Navigations either happen after this function call, or they are
  // triggered before, but synchronously before (and navigations are async), so
  // this should not have updated to the new url that will be navigated to.
  std::optional<webapps::AppId> source_contents_app_id =
      source_contents
          ? WebAppTabHelper::FromWebContents(params.source_contents)->app_id()
          : std::nullopt;

  std::optional<webapps::AppId> referrer_app_id =
      params.referrer.url.is_valid()
          ? registrar.FindAppThatCapturesLinksInScope(params.referrer.url)
          : std::nullopt;
  debug_data.Set("referrer.url", params.referrer.url.possibly_invalid_spec());
  debug_data.Set("referrer.controlling_app_id",
                 referrer_app_id.value_or("<none>"));
  debug_data.Set(
      "source_contents.url",
      source_contents
          ? source_contents->GetLastCommittedURL().possibly_invalid_spec()
          : "<nullptr>");
  debug_data.Set("source_contents.app_id",
                 source_contents_app_id.value_or("<none>"));

  debug_data.Set("controlling_app_id", controlling_app_id.value_or("<none>"));
  debug_data.Set("controlling_app_display_mode",
                 base::ToString(controlling_app_display_mode.value_or(
                     DisplayMode::kUndefined)));
  debug_data.Set("params.browser", base::ToString(params.browser.get()));

  debug_data.Set("params.url", params.url.possibly_invalid_spec());
  debug_data.Set("params.disposition", base::ToString(params.disposition));
  debug_data.Set("params.opener", params.opener != nullptr);
  debug_data.Set("params.contents_to_insert",
                 base::ToString(params.contents_to_insert.get()));
  debug_data.Set("source_browser_app_id",
                 source_browser_app_id.value_or("<none>"));

  const bool is_user_modified_click =
      params.disposition == WindowOpenDisposition::NEW_WINDOW ||
      params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;

  debug_data.Set("is_user_modified_click", is_user_modified_click);

  // Case: Any click (user modified or non-modified) with auxiliary browsing
  // context. Only needs to be handled if it is triggered in the context of an
  // app browser.
  if (IsAuxiliaryBrowsingContext(params)) {
    debug_data.Set("is_auxiliary_browsing_context", true);
    if (source_browser_app_id.has_value()) {
      Browser* app_window = CreateWebAppWindowFromNavigationParams(
          *source_browser_app_id, params,
          params.disposition == WindowOpenDisposition::NEW_POPUP);

      return AppNavigationResult::AuxiliaryContextInAppWindow(
          *source_browser_app_id, params.disposition, app_window,
          std::move(debug_data));
    }
    return AppNavigationResult::AuxiliaryContext(params.disposition,
                                                 std::move(debug_data));
  }
  debug_data.Set("is_auxiliary_browsing_context", false);

  // If no app controls the the target url, then there cannot be any further
  // capturing logic unless a redirect occurs.
  if (!controlling_app_id) {
    return AppNavigationResult::NoInitialActionRedirectionHandlingEligible(
        source_browser_app_id, params.disposition, std::move(debug_data));
  }
  CHECK(controlling_app_display_mode);
  CHECK(client_mode_and_browser);
  const webapps::AppId& app_id = *controlling_app_id;
  blink::mojom::DisplayMode app_display_mode = *controlling_app_display_mode;
  LaunchHandler::ClientMode client_mode =
      client_mode_and_browser->effective_client_mode;

  // Case: User-modified clicks.
  if (is_user_modified_click) {
    // The default behavior is only modified if the source is an app browser or
    // the controlling app's display mode is 'browser'.
    if (!source_browser_app_id.has_value() &&
        app_display_mode != DisplayMode::kBrowser) {
      return AppNavigationResult::NoInitialActionRedirectionHandlingEligible(
          source_browser_app_id, params.disposition, std::move(debug_data));
    }
    // Case: Shift-clicks with a new top level browsing context.
    if (params.disposition == WindowOpenDisposition::NEW_WINDOW) {
      Browser* app_host_window;
      if (app_display_mode == DisplayMode::kBrowser) {
        app_host_window = Browser::Create(
            Browser::CreateParams(profile, params.user_gesture));
      } else {
        app_host_window =
            CreateWebAppWindowFromNavigationParams(app_id, params);
      }
      return AppNavigationResult::ForcedNewAppContext(
          source_browser_app_id, app_id, app_display_mode, app_host_window,
          params.disposition, std::move(debug_data));
    }

    bool is_in_source_app_with_url_in_scope =
        source_browser_app_id
            ? registrar.IsUrlInAppScope(params.url, *source_browser_app_id)
            : false;

    // Case: Middle clicks with a new top level browsing context.
    if (params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB &&
        (app_display_mode == DisplayMode::kBrowser ||
         (app_id == source_browser_app_id &&
          is_in_source_app_with_url_in_scope))) {
      if (source_browser_app_id.has_value() &&
          !params.browser->app_controller()->ShouldHideNewTabButton()) {
        // Apps that support tabbed mode can open a new tab in the current app
        // browser itself.
        return AppNavigationResult::ForcedNewAppContext(
            source_browser_app_id, app_id, app_display_mode, params.browser,
            params.disposition, std::move(debug_data));
      }
      Browser* app_host_window;
      if (app_display_mode == DisplayMode::kBrowser) {
        // For a 'new tab' with the 'browser' requested display mode, prefer
        // using an existing browser window.
        app_host_window = client_mode_and_browser->browser
                              ? client_mode_and_browser->browser.get()
                              : Browser::Create(Browser::CreateParams(
                                    profile, params.user_gesture));
      } else {
        app_host_window =
            CreateWebAppWindowFromNavigationParams(app_id, params);
      }
      return AppNavigationResult::ForcedNewAppContext(
          source_browser_app_id, app_id, app_display_mode, app_host_window,
          params.disposition, std::move(debug_data));
    }
    return AppNavigationResult::NoInitialActionRedirectionHandlingEligible(
        source_browser_app_id, params.disposition, std::move(debug_data));
  }

  if (params.disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    return AppNavigationResult::CapturingDisabled();
  }
  // Case: Left click, non-user-modified. Capturable.

  // Opening in non-browser-tab requires OS integration. Since os integration
  // cannot be triggered synchronously, treat this as opening in browser.
  if (registrar.GetInstallState(app_id) ==
      proto::INSTALLED_WITHOUT_OS_INTEGRATION) {
    app_display_mode = blink::mojom::DisplayMode::kBrowser;
  }

  // If the developer has an in-scope link that opens a new top level browsing
  // in their own page, then force the client mode to be navigate-new.
  // To detect this, we consider both the app_id that controls the referrer url
  // as well as the app id of the tab that initiated the navigation.
  if (referrer_app_id == app_id || source_contents_app_id == app_id) {
    client_mode = LaunchHandler::ClientMode::kNavigateNew;
  }

  // Prevent-close requires only focusing the existing tab, and never
  // navigating.
  if (registrar.IsPreventCloseEnabled(app_id) &&
      !registrar.IsTabbedWindowModeEnabled(app_id)) {
    client_mode = LaunchHandler::ClientMode::kFocusExisting;
  }
  debug_data.Set("client_mode", base::ToString(client_mode));

  // Focus existing.
  if (client_mode == LaunchHandler::ClientMode::kFocusExisting) {
    CHECK(client_mode_and_browser->browser);
    CHECK(client_mode_and_browser->tab_index.has_value());
    content::WebContents* contents =
        client_mode_and_browser->browser->tab_strip_model()->GetWebContentsAt(
            *client_mode_and_browser->tab_index);
    CHECK(contents);
    contents->Focus();

    // Abort the navigation by returning a `nullptr`. Because this means
    // `OnWebAppNavigationAfterWebContentsCreation` won't be called, enqueue
    // the launch params instantly and record the debug data.
    EnqueueLaunchParams(contents, app_id, params.url,
                        /*wait_for_navigation_to_complete=*/false);
    provider->navigation_capturing_log().StoreNavigationCapturedDebugData(
        base::Value(std::move(debug_data)));

    MaybeShowNavigationCaptureIph(app_id, profile,
                                  client_mode_and_browser->browser);

    // TODO(crbug.com/336371044): Update RecordLaunchMetrics() to also work
    // with apps that open in a new browser tab.
    RecordLaunchMetrics(app_id, apps::LaunchContainer::kLaunchContainerWindow,
                        apps::LaunchSource::kFromNavigationCapturing,
                        params.url, contents);
    return AppNavigationResult::CancelNavigation();
  }

  // Navigate existing.
  if (client_mode == LaunchHandler::ClientMode::kNavigateExisting) {
    CHECK(client_mode_and_browser->browser);
    CHECK(client_mode_and_browser->tab_index.has_value());
    return AppNavigationResult::CapturedNavigateExisting(
        source_browser_app_id, app_id, client_mode_and_browser->browser,
        *client_mode_and_browser->tab_index, params.disposition,
        std::move(debug_data));
  }

  // Navigate new.
  CHECK(client_mode == LaunchHandler::ClientMode::kNavigateNew);

  Browser* host_window = nullptr;
  switch (app_display_mode) {
    case blink::mojom::DisplayMode::kBrowser:
      if (client_mode_and_browser->browser) {
        host_window = client_mode_and_browser->browser;
      } else {
        host_window = Browser::Create(
            Browser::CreateParams(profile, params.user_gesture));
      }
      break;
    case blink::mojom::DisplayMode::kMinimalUi:
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kWindowControlsOverlay:
    case blink::mojom::DisplayMode::kBorderless:
      host_window = CreateWebAppWindowFromNavigationParams(app_id, params);
      break;
    case blink::mojom::DisplayMode::kTabbed:
      if (client_mode_and_browser->browser) {
        host_window = client_mode_and_browser->browser;
      } else {
        host_window = CreateWebAppWindowFromNavigationParams(app_id, params);
      }
      break;
    case blink::mojom::DisplayMode::kUndefined:
    case blink::mojom::DisplayMode::kPictureInPicture:
    case blink::mojom::DisplayMode::kFullscreen:
      NOTREACHED_NORETURN();
  }

  return AppNavigationResult::CapturedNewClient(
      source_browser_app_id, app_id, app_display_mode, host_window,
      params.disposition, std::move(debug_data));
}

void EnqueueLaunchParams(content::WebContents* contents,
                         const webapps::AppId& app_id,
                         const GURL& url,
                         bool wait_for_navigation_to_complete) {
  CHECK(contents);
  WebAppLaunchParams launch_params;
  launch_params.started_new_navigation = wait_for_navigation_to_complete;
  launch_params.app_id = app_id;
  launch_params.target_url = url;
  WebAppTabHelper::FromWebContents(contents)->EnsureLaunchQueue().Enqueue(
      std::move(launch_params));
}

void OnWebAppNavigationAfterWebContentsCreation(
    web_app::AppNavigationResult app_navigation_result,
    const NavigateParams& params,
    base::WeakPtr<content::NavigationHandle> navigation_handle) {
  if (!app_navigation_result.capturing_feature_enabled()) {
    // Don't attach info for redirects nor perform any other logic below if the
    // capturing wasn't enabled for the navigation.
    return;
  }
  CHECK(AreWebAppsEnabled(params.initiating_profile));
  CHECK(!params.open_pwa_window_if_possible);

  std::optional<webapps::AppId> first_navigation_app_id =
      app_navigation_result.redirection_info().first_navigation_app_id();
  MaybePopulateNavigationHandlingInfoForRedirects(
      navigation_handle, params.navigated_or_inserted_contents,
      app_navigation_result.redirection_info());

  base::Value::Dict debug_value = app_navigation_result.TakeDebugData();
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(params.initiating_profile);

  // Exclude all cases where there isn't navigation capturing logic occurring.
  if (!app_navigation_result.browser_tab_override()) {
    // Also record debug information for ALL navigations if expensive DCHECKs
    // are enabled.
#if EXPENSIVE_DCHECKS_ARE_ON()
    debug_value.Set("handled_by_app", false);
    debug_value.Set("result.browser", base::ToString(params.browser.get()));
    debug_value.Set("result.tab_index", params.tabstrip_index);
    provider->navigation_capturing_log().StoreNavigationCapturedDebugData(
        base::Value(std::move(debug_value)));
#endif
    return;
  }
  CHECK(params.browser);

  std::tuple<Browser*, int> browser_tab_override =
      *app_navigation_result.browser_tab_override();
  debug_value.Set("handled_by_app", true);
  debug_value.Set("params.navigated_or_inserted_contents",
                  base::ToString(params.navigated_or_inserted_contents));
  provider->navigation_capturing_log().StoreNavigationCapturedDebugData(
      base::Value(std::move(debug_value)));

  // Enqueue launch params and show the IPH bubble denoting that an app has
  // handled the navigation.
  if (app_navigation_result.perform_app_handling_tasks_in_web_contents()) {
    CHECK(first_navigation_app_id);
    EnqueueLaunchParams(params.navigated_or_inserted_contents,
                        *first_navigation_app_id, params.url,
                        /*wait_for_navigation_to_complete=*/true);

    apps::LaunchContainer container =
        std::get<Browser*>(browser_tab_override)->app_controller()
            ? apps::LaunchContainer::kLaunchContainerWindow
            : apps::LaunchContainer::kLaunchContainerTab;
    RecordLaunchMetrics(*first_navigation_app_id, container,
                        apps::LaunchSource::kFromNavigationCapturing,
                        params.url, params.navigated_or_inserted_contents);

    // TODO(crbug.com/336371044): Support IPH in browser tabs.
    if (params.browser->app_controller()) {
      MaybeShowNavigationCaptureIph(*first_navigation_app_id,
                                    params.initiating_profile, params.browser);
    }
  }
}

}  // namespace web_app
