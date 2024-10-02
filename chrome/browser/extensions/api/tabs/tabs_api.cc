// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include <stddef.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_constants.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_util.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/window_pin_util.h"
#include "chrome/browser/ui/browser_command_controller.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/lacros/window_properties.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "content/public/browser/devtools_agent_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#endif

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using zoom::ZoomController;

namespace extensions {

namespace windows = api::windows;
namespace tabs = api::tabs;

using api::extension_types::InjectDetails;

namespace {

template <typename T>
class ApiParameterExtractor {
 public:
  explicit ApiParameterExtractor(absl::optional<T>& params)
      : params_(*params) {}
  ~ApiParameterExtractor() = default;

  bool populate_tabs() {
    if (params_->query_options && params_->query_options->populate)
      return *params_->query_options->populate;
    return false;
  }

  WindowController::TypeFilter type_filters() {
    if (params_->query_options && params_->query_options->window_types)
      return WindowController::GetFilterFromWindowTypes(
          *params_->query_options->window_types);
    return WindowController::kNoWindowFilter;
  }

 private:
  raw_ref<T> params_;
};

bool GetBrowserFromWindowID(const ChromeExtensionFunctionDetails& details,
                            int window_id,
                            Browser** browser,
                            std::string* error) {
  Browser* result = nullptr;
  result = ExtensionTabUtil::GetBrowserFromWindowID(details, window_id, error);
  if (!result)
    return false;

  *browser = result;
  return true;
}

bool GetBrowserFromWindowID(ExtensionFunction* function,
                            int window_id,
                            Browser** browser,
                            std::string* error) {
  return GetBrowserFromWindowID(ChromeExtensionFunctionDetails(function),
                                window_id, browser, error);
}

// |error_message| can optionally be passed in and will be set with an
// appropriate message if the tab cannot be found by id.
bool GetTabById(int tab_id,
                content::BrowserContext* context,
                bool include_incognito,
                Browser** browser,
                TabStripModel** tab_strip,
                content::WebContents** contents,
                int* tab_index,
                std::string* error_message) {
  if (ExtensionTabUtil::GetTabById(tab_id, context, include_incognito, browser,
                                   tab_strip, contents, tab_index)) {
    return true;
  }

  if (error_message) {
    *error_message = ErrorUtils::FormatErrorMessage(
        tabs_constants::kTabNotFoundError, base::NumberToString(tab_id));
  }

  return false;
}

// Gets the WebContents for |tab_id| if it is specified. Otherwise get the
// WebContents for the active tab in the |function|'s current window.
// Returns nullptr and fills |error| if failed.
content::WebContents* GetTabsAPIDefaultWebContents(ExtensionFunction* function,
                                                   int tab_id,
                                                   std::string* error) {
  content::WebContents* web_contents = nullptr;
  if (tab_id != -1) {
    // We assume this call leaves web_contents unchanged if it is unsuccessful.
    GetTabById(tab_id, function->browser_context(),
               function->include_incognito_information(),
               nullptr /* ignore Browser* output */,
               nullptr /* ignore TabStripModel* output */, &web_contents,
               nullptr /* ignore int tab_index output */, error);
  } else {
    Browser* browser =
        ChromeExtensionFunctionDetails(function).GetCurrentBrowser();
    if (!browser)
      *error = tabs_constants::kNoCurrentWindowError;
    else if (!ExtensionTabUtil::GetDefaultTab(browser, &web_contents, nullptr))
      *error = tabs_constants::kNoSelectedTabError;
  }
  return web_contents;
}

// Returns true if either |boolean| is disengaged, or if |boolean| and
// |value| are equal. This function is used to check if a tab's parameters match
// those of the browser.
bool MatchesBool(const absl::optional<bool>& boolean, bool value) {
  return !boolean || *boolean == value;
}

ui::WindowShowState ConvertToWindowShowState(windows::WindowState state) {
  switch (state) {
    case windows::WINDOW_STATE_NORMAL:
      return ui::SHOW_STATE_NORMAL;
    case windows::WINDOW_STATE_MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case windows::WINDOW_STATE_MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case windows::WINDOW_STATE_FULLSCREEN:
    case windows::WINDOW_STATE_LOCKED_FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case windows::WINDOW_STATE_NONE:
      return ui::SHOW_STATE_DEFAULT;
  }
  NOTREACHED();
  return ui::SHOW_STATE_DEFAULT;
}

bool IsValidStateForWindowsCreateFunction(
    const windows::Create::Params::CreateData* create_data) {
  if (!create_data)
    return true;

  bool has_bound = create_data->left || create_data->top ||
                   create_data->width || create_data->height;

  switch (create_data->state) {
    case windows::WINDOW_STATE_MINIMIZED:
      // If minimised, default focused state should be unfocused.
      return !(create_data->focused && *create_data->focused) && !has_bound;
    case windows::WINDOW_STATE_MAXIMIZED:
    case windows::WINDOW_STATE_FULLSCREEN:
    case windows::WINDOW_STATE_LOCKED_FULLSCREEN:
      // If maximised/fullscreen, default focused state should be focused.
      return !(create_data->focused && !*create_data->focused) && !has_bound;
    case windows::WINDOW_STATE_NORMAL:
    case windows::WINDOW_STATE_NONE:
      return true;
  }
  NOTREACHED();
  return true;
}

bool ExtensionHasLockedFullscreenPermission(const Extension* extension) {
  return extension && extension->permissions_data()->HasAPIPermission(
                          mojom::APIPermissionID::kLockWindowFullscreenPrivate);
}

api::tabs::Tab CreateTabObjectHelper(WebContents* contents,
                                     const Extension* extension,
                                     Feature::Context context,
                                     TabStripModel* tab_strip,
                                     int tab_index) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension, context, contents);
  return ExtensionTabUtil::CreateTabObject(contents, scrub_tab_behavior,
                                           extension, tab_strip, tab_index);
}

// Moves the given tab to the |target_browser|. On success, returns the
// new index of the tab in the target tabstrip. On failure, returns -1.
// Assumes that the caller has already checked whether the target window is
// different from the source.
int MoveTabToWindow(ExtensionFunction* function,
                    int tab_id,
                    Browser* target_browser,
                    int new_index,
                    std::string* error) {
  Browser* source_browser = nullptr;
  TabStripModel* source_tab_strip = nullptr;
  int source_index = -1;
  if (!GetTabById(tab_id, function->browser_context(),
                  function->include_incognito_information(), &source_browser,
                  &source_tab_strip, nullptr, &source_index, error)) {
    return -1;
  }

  if (!ExtensionTabUtil::IsTabStripEditable()) {
    *error = tabs_constants::kTabStripNotEditableError;
    return -1;
  }

  // TODO(crbug.com/990158): Rather than calling is_type_normal(), should
  // this call SupportsWindowFeature(Browser::FEATURE_TABSTRIP)?
  if (!target_browser->is_type_normal()) {
    *error = tabs_constants::kCanOnlyMoveTabsWithinNormalWindowsError;
    return -1;
  }

  if (target_browser->profile() != source_browser->profile()) {
    *error = tabs_constants::kCanOnlyMoveTabsWithinSameProfileError;
    return -1;
  }

  std::unique_ptr<content::WebContents> web_contents =
      source_tab_strip->DetachWebContentsAtForInsertion(source_index);
  if (!web_contents) {
    *error = ErrorUtils::FormatErrorMessage(tabs_constants::kTabNotFoundError,
                                            base::NumberToString(tab_id));
    return -1;
  }

  TabStripModel* target_tab_strip =
      ExtensionTabUtil::GetEditableTabStripModel(target_browser);
  DCHECK(target_tab_strip);

  // Clamp move location to the last position.
  // This is ">" because it can append to a new index position.
  // -1 means set the move location to the last position.
  int target_index = new_index;
  if (target_index > target_tab_strip->count() || target_index < 0)
    target_index = target_tab_strip->count();

  return target_tab_strip->InsertWebContentsAt(
      target_index, std::move(web_contents), AddTabTypes::ADD_NONE);
}

// This function sets the state of the browser window to a "locked"
// fullscreen state (where the user can't exit fullscreen) in response to a
// call to either chrome.windows.create or chrome.windows.update when the
// screen is set locked. This is only necessary for ChromeOS and Lacros and
// is restricted to allowlisted extensions.
void SetLockedFullscreenState(Browser* browser, bool pinned) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  aura::Window* window = browser->window()->GetNativeWindow();
  DCHECK(window);

  // As this gets triggered from extensions, we might encounter this case.
  if (IsWindowPinned(window) == pinned)
    return;

  if (pinned) {
    // Pins from extension are always trusted.
    PinWindow(window, /*trusted=*/true);
  } else {
    UnpinWindow(window);
  }

  // Update the set of available browser commands.
  browser->command_controller()->LockedFullscreenStateChanged();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  aura::Window* window = browser->window()->GetNativeWindow();
  DCHECK(window);

  const chromeos::WindowPinType previous_type =
      window->GetProperty(lacros::kWindowPinTypeKey);
  DCHECK_NE(previous_type, chromeos::WindowPinType::kTrustedPinned)
      << "Extensions only set Trusted Pinned";

  bool previous_pinned =
      previous_type == chromeos::WindowPinType::kTrustedPinned;
  // As this gets triggered from extensions, we might encounter this case.
  if (previous_pinned == pinned)
    return;

  window->SetProperty(lacros::kWindowPinTypeKey,
                      pinned ? chromeos::WindowPinType::kTrustedPinned
                             : chromeos::WindowPinType::kNone);

  auto* pinned_mode_extension =
      views::DesktopWindowTreeHostLacros::From(window->GetHost())
          ->GetPinnedModeExtension();
  if (pinned) {
    pinned_mode_extension->Pin(/*trusted=*/true);
  } else {
    pinned_mode_extension->Unpin();
  }

  // Update the set of available browser commands.
  browser->command_controller()->LockedFullscreenStateChanged();

  // Wipe the clipboard in browser and detach any dev tools.
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  content::DevToolsAgentHost::DetachAllClients();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Returns whether the given `bounds` intersect with at least 50% of all the
// displays.
bool WindowBoundsIntersectDisplays(const gfx::Rect& bounds) {
  int intersect_area = 0;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    gfx::Rect display_bounds = display.bounds();
    display_bounds.Intersect(bounds);
    intersect_area += display_bounds.size().GetArea();
  }
  return intersect_area >= (bounds.size().GetArea() / 2);
}

}  // namespace

void ZoomModeToZoomSettings(ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  switch (zoom_mode) {
    case ZoomController::ZOOM_MODE_DEFAULT:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN;
      break;
    case ZoomController::ZOOM_MODE_ISOLATED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_MANUAL:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_MANUAL;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_DISABLED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_DISABLED;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
  }
}

// Windows ---------------------------------------------------------------------

ExtensionFunction::ResponseAction WindowsGetFunction::Run() {
  absl::optional<windows::Get::Params> params =
      windows::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ApiParameterExtractor<windows::Get::Params> extractor(params);
  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(this, params->window_id,
                                            extractor.type_filters(), &browser,
                                            &error)) {
    return RespondNow(Error(std::move(error)));
  }

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  base::Value::Dict windows = ExtensionTabUtil::CreateWindowValueForExtension(
      *browser, extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetCurrentFunction::Run() {
  absl::optional<windows::GetCurrent::Params> params =
      windows::GetCurrent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ApiParameterExtractor<windows::GetCurrent::Params> extractor(params);
  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(
          this, extension_misc::kCurrentWindowId, extractor.type_filters(),
          &browser, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  base::Value::Dict windows = ExtensionTabUtil::CreateWindowValueForExtension(
      *browser, extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetLastFocusedFunction::Run() {
  absl::optional<windows::GetLastFocused::Params> params =
      windows::GetLastFocused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ApiParameterExtractor<windows::GetLastFocused::Params> extractor(params);
  // The WindowControllerList should contain a list of application,
  // browser and devtools windows.
  Browser* browser = nullptr;
  for (auto* controller : WindowControllerList::GetInstance()->windows()) {
    if (controller->GetBrowser() &&
        windows_util::CanOperateOnWindow(this, controller,
                                         extractor.type_filters())) {
      // TODO(devlin): Doesn't this mean that we'll use the last window in the
      // list if there is no active window? That seems wrong.
      // See https://crbug.com/809822.
      browser = controller->GetBrowser();
      if (controller->window()->IsActive())
        break;  // Use focused window.
    }
  }
  if (!browser)
    return RespondNow(Error(tabs_constants::kNoLastFocusedWindowError));

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  base::Value::Dict windows = ExtensionTabUtil::CreateWindowValueForExtension(
      *browser, extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  absl::optional<windows::GetAll::Params> params =
      windows::GetAll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ApiParameterExtractor<windows::GetAll::Params> extractor(params);
  base::Value::List window_list;
  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  for (auto* controller : WindowControllerList::GetInstance()->windows()) {
    if (!controller->GetBrowser() ||
        !windows_util::CanOperateOnWindow(this, controller,
                                          extractor.type_filters())) {
      continue;
    }
    window_list.Append(ExtensionTabUtil::CreateWindowValueForExtension(
        *controller->GetBrowser(), extension(), populate_tab_behavior,
        source_context_type()));
  }

  return RespondNow(WithArguments(std::move(window_list)));
}

ExtensionFunction::ResponseAction WindowsCreateFunction::Run() {
  absl::optional<windows::Create::Params> params =
      windows::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  std::vector<GURL> urls;
  TabStripModel* source_tab_strip = nullptr;
  int tab_index = -1;

  DCHECK(extension() || source_context_type() == Feature::WEBUI_CONTEXT ||
         source_context_type() == Feature::WEBUI_UNTRUSTED_CONTEXT);
  absl::optional<windows::Create::Params::CreateData>& create_data =
      params->create_data;

  // Look for optional url.
  if (create_data && create_data->url) {
    std::vector<std::string> url_strings;
    // First, get all the URLs the client wants to open.
    if (create_data->url->as_string) {
      url_strings.push_back(std::move(*create_data->url->as_string));
    } else if (create_data->url->as_strings) {
      url_strings = std::move(*create_data->url->as_strings);
    }

    // Second, resolve, validate and convert them to GURLs.
    for (auto& url_string : url_strings) {
      auto url = ExtensionTabUtil::PrepareURLForNavigation(
          url_string, extension(), browser_context());
      if (!url.has_value()) {
        return RespondNow(Error(std::move(url.error())));
      }
      urls.push_back(*url);
    }
  }

  // Decide whether we are opening a normal window or an incognito window.
  std::string error;
  Profile* calling_profile = Profile::FromBrowserContext(browser_context());
  windows_util::IncognitoResult incognito_result =
      windows_util::ShouldOpenIncognitoWindow(
          calling_profile,
          create_data && create_data->incognito
              ? absl::optional<bool>(*create_data->incognito)
              : absl::nullopt,
          &urls, &error);
  if (incognito_result == windows_util::IncognitoResult::kError)
    return RespondNow(Error(std::move(error)));

  Profile* window_profile =
      incognito_result == windows_util::IncognitoResult::kIncognito
          ? calling_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : calling_profile;

  // Look for optional tab id.
  if (create_data && create_data->tab_id) {
    // Find the tab. |source_tab_strip| and |tab_index| will later be used to
    // move the tab into the created window.
    Browser* source_browser = nullptr;
    content::WebContents* web_contents = nullptr;
    if (!GetTabById(*create_data->tab_id, calling_profile,
                    include_incognito_information(), &source_browser,
                    &source_tab_strip, &web_contents, &tab_index, &error)) {
      return RespondNow(Error(std::move(error)));
    }

    if (!ExtensionTabUtil::IsTabStripEditable())
      return RespondNow(Error(tabs_constants::kTabStripNotEditableError));

    if (source_browser->profile() != window_profile)
      return RespondNow(
          Error(tabs_constants::kCanOnlyMoveTabsWithinSameProfileError));

    if (DevToolsWindow::IsDevToolsWindow(web_contents))
      return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
  }

  if (!IsValidStateForWindowsCreateFunction(base::OptionalToPtr(create_data)))
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));

  Browser::Type window_type = Browser::TYPE_NORMAL;

  gfx::Rect window_bounds;
  bool focused = true;
  std::string extension_id;

  if (create_data) {
    // Figure out window type before figuring out bounds so that default
    // bounds can be set according to the window type.
    switch (create_data->type) {
      // TODO(stevenjb): Remove 'panel' from windows.json.
      case windows::CREATE_TYPE_PANEL:
      case windows::CREATE_TYPE_POPUP:
        window_type = Browser::TYPE_POPUP;
        if (extension())
          extension_id = extension()->id();
        break;
      case windows::CREATE_TYPE_NONE:
      case windows::CREATE_TYPE_NORMAL:
        break;
      default:
        return RespondNow(Error(tabs_constants::kInvalidWindowTypeError));
    }

    // Initialize default window bounds according to window type.
    ui::WindowShowState ignored_show_state = ui::SHOW_STATE_DEFAULT;
    WindowSizer::GetBrowserWindowBoundsAndShowState(
        gfx::Rect(), nullptr, &window_bounds, &ignored_show_state);

    // Update the window bounds based on the create parameters.
    bool set_window_position = false;
    bool set_window_size = false;
    if (create_data->left) {
      window_bounds.set_x(*create_data->left);
      set_window_position = true;
    }
    if (create_data->top) {
      window_bounds.set_y(*create_data->top);
      set_window_position = true;
    }
    if (create_data->width) {
      window_bounds.set_width(*create_data->width);
      set_window_size = true;
    }
    if (create_data->height) {
      window_bounds.set_height(*create_data->height);
      set_window_size = true;
    }

    // If the extension specified the window size but no position, adjust the
    // window to fit in the display.
    if (!set_window_position && set_window_size) {
      const display::Display& display =
          display::Screen::GetScreen()->GetDisplayMatching(window_bounds);
      window_bounds.AdjustToFit(display.bounds());
    }

    // Immediately fail if the window bounds don't intersect the displays.
    if ((set_window_position || set_window_size) &&
        !WindowBoundsIntersectDisplays(window_bounds)) {
      return RespondNow(Error(tabs_constants::kInvalidWindowBoundsError));
    }

    if (create_data->focused)
      focused = *create_data->focused;

    // Record the window height and width to determine if we
    // can set a mininimum value for them (crbug.com/1369103).
    UMA_HISTOGRAM_COUNTS_1000("Extensions.CreateWindowWidth",
                              window_bounds.width());
    UMA_HISTOGRAM_COUNTS_1000("Extensions.CreateWindowHeight",
                              window_bounds.height());
  }

  // Create a new BrowserWindow if possible.
  if (Browser::GetCreationStatusForProfile(window_profile) !=
      Browser::CreationStatus::kOk) {
    return RespondNow(Error(tabs_constants::kBrowserWindowNotAllowed));
  }
  Browser::CreateParams create_params(window_type, window_profile,
                                      user_gesture());
  if (extension_id.empty()) {
    create_params.initial_bounds = window_bounds;
  } else {
    // extension_id is only set for CREATE_TYPE_POPUP.
    create_params = Browser::CreateParams::CreateForAppPopup(
        web_app::GenerateApplicationNameFromAppId(extension_id),
        false /* trusted_source */, window_bounds, window_profile,
        user_gesture());
  }
  create_params.initial_show_state = ui::SHOW_STATE_NORMAL;
  if (create_data && create_data->state) {
    if (create_data->state == windows::WINDOW_STATE_LOCKED_FULLSCREEN &&
        !ExtensionHasLockedFullscreenPermission(extension())) {
      return RespondNow(
          Error(tabs_constants::kMissingLockWindowFullscreenPrivatePermission));
    }
    create_params.initial_show_state =
        ConvertToWindowShowState(create_data->state);
  }

  Browser* new_window = Browser::Create(create_params);
  if (!new_window)
    return RespondNow(Error(tabs_constants::kBrowserWindowNotAllowed));

  for (const GURL& url : urls) {
    NavigateParams navigate_params(new_window, url, ui::PAGE_TRANSITION_LINK);
    navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

    // Depending on the |setSelfAsOpener| option, we need to put the new
    // contents in the same BrowsingInstance as their opener.  See also
    // https://crbug.com/713888.
    bool set_self_as_opener = create_data->set_self_as_opener &&  // present?
                              *create_data->set_self_as_opener;  // set to true?
    if (set_self_as_opener) {
      if (is_from_service_worker()) {
        // TODO(crbug.com/984350): Add test for this.
        return RespondNow(
            Error("Cannot specify setSelfAsOpener Service Worker extension."));
      }
      // TODO(crbug.com/984350): Add tests for checking opener SiteInstance
      // behavior from a SW based extension's extension frame (e.g. from popup).
      // See ExtensionApiTest.WindowsCreate* tests for details.
      navigate_params.initiator_origin =
          extension() ? extension()->origin()
                      : render_frame_host()->GetLastCommittedOrigin();
      navigate_params.opener = render_frame_host();
      navigate_params.source_site_instance =
          render_frame_host()->GetSiteInstance();
    }

    Navigate(&navigate_params);
  }

  WebContents* contents = nullptr;
  // Move the tab into the created window only if it's an empty popup or it's
  // a tabbed window.
  if (window_type == Browser::TYPE_NORMAL || urls.empty()) {
    if (source_tab_strip) {
      std::unique_ptr<content::WebContents> detached_tab =
          source_tab_strip->DetachWebContentsAtForInsertion(tab_index);
      contents = detached_tab.get();
      TabStripModel* target_tab_strip =
          ExtensionTabUtil::GetEditableTabStripModel(new_window);
      if (!target_tab_strip)
        return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
      target_tab_strip->InsertWebContentsAt(
          urls.size(), std::move(detached_tab), AddTabTypes::ADD_NONE);
    }
  }
  // Create a new tab if the created window is still empty. Don't create a new
  // tab when it is intended to create an empty popup.
  if (!contents && urls.empty() && window_type == Browser::TYPE_NORMAL) {
    chrome::NewTab(new_window);
  }
  chrome::SelectNumberedTab(
      new_window, 0,
      TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kNone));

  if (focused) {
    new_window->window()->Show();
  } else {
    // The new window isn't supposed to be focused. Here, instead of showing an
    // unfocused window on top (possible on some operating systems), we show
    // the window and then bring the old focused window back on top.
    // We still use ShowInactive() (instead of doing a Show() followed
    // immediately by Deactivate()) because the process of showing the window is
    // somewhat asynchronous. This causes the immediate Deactivate() call to not
    // work.
    BrowserList* const browser_list = BrowserList::GetInstance();
    Browser* active_browser = browser_list->GetLastActive();
    bool reset_active = false;
    // Check if there's a currently-active window that should re-take focus.
    // NOTE: This browser *may* be from another profile. We don't access any
    // data from it.
    if (active_browser && active_browser->window()->IsActive())
      reset_active = true;
    new_window->window()->ShowInactive();
    // NOTE: It's possible that showing the new browser synchronously caused
    // the old one to close. Ensure it's still valid before activating it.
    if (reset_active && base::Contains(*browser_list, active_browser))
      active_browser->window()->Activate();
  }

// Despite creating the window with initial_show_state() ==
// ui::SHOW_STATE_MINIMIZED above, on Linux the window is not created as
// minimized.
// TODO(crbug.com/1410400): Remove this workaround when linux is fixed.
#if BUILDFLAG(IS_LINUX)
// TODO(crbug.com/1410400): Find a fix for wayland as well.

// Must be defined inside IS_LINUX to compile on windows/mac.
#if BUILDFLAG(OZONE_PLATFORM_X11)
  if (new_window->initial_show_state() == ui::SHOW_STATE_MINIMIZED) {
    new_window->window()->Minimize();
  }
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)
#endif  // BUILDFLAG(IS_LINUX)

  // Lock the window fullscreen only after the new tab has been created
  // (otherwise the tabstrip is empty), and window()->show() has been called
  // (otherwise that resets the locked mode for devices in tablet mode).
  if (create_data &&
      create_data->state == windows::WINDOW_STATE_LOCKED_FULLSCREEN) {
    SetLockedFullscreenState(new_window, /*pinned=*/true);
  }

  if (new_window->profile()->IsOffTheRecord() &&
      !browser_context()->IsOffTheRecord() &&
      !include_incognito_information()) {
    // Don't expose incognito windows if extension itself works in non-incognito
    // profile and CanCrossIncognito isn't allowed.
    return RespondNow(WithArguments(base::Value()));
  }

  return RespondNow(
      WithArguments(ExtensionTabUtil::CreateWindowValueForExtension(
          *new_window, extension(), ExtensionTabUtil::kPopulateTabs,
          source_context_type())));
}

ExtensionFunction::ResponseAction WindowsUpdateFunction::Run() {
  absl::optional<windows::Update::Params> params =
      windows::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(
          this, params->window_id, WindowController::GetAllWindowFilter(),
          &browser, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  // Don't allow locked fullscreen operations on a window without the proper
  // permission (also don't allow any operations on a locked window if the
  // extension doesn't have the permission).
  const bool is_locked_fullscreen =
      platform_util::IsBrowserLockedFullscreen(browser);
  if ((params->update_info.state == windows::WINDOW_STATE_LOCKED_FULLSCREEN ||
       is_locked_fullscreen) &&
      !ExtensionHasLockedFullscreenPermission(extension())) {
    return RespondNow(
        Error(tabs_constants::kMissingLockWindowFullscreenPrivatePermission));
  }

  // Before changing any of a window's state, validate the update parameters.
  // This prevents Chrome from performing "half" an update.

  // Update the window bounds if the bounds from the update parameters intersect
  // the displays.
  gfx::Rect window_bounds = browser->window()->IsMinimized()
                                ? browser->window()->GetRestoredBounds()
                                : browser->window()->GetBounds();
  bool set_window_bounds = false;
  if (params->update_info.left) {
    window_bounds.set_x(*params->update_info.left);
    set_window_bounds = true;
  }
  if (params->update_info.top) {
    window_bounds.set_y(*params->update_info.top);
    set_window_bounds = true;
  }
  if (params->update_info.width) {
    window_bounds.set_width(*params->update_info.width);
    set_window_bounds = true;
  }
  if (params->update_info.height) {
    window_bounds.set_height(*params->update_info.height);
    set_window_bounds = true;
  }

  if (set_window_bounds && !WindowBoundsIntersectDisplays(window_bounds))
    return RespondNow(Error(tabs_constants::kInvalidWindowBoundsError));

  ui::WindowShowState show_state =
      ConvertToWindowShowState(params->update_info.state);
  if (set_window_bounds && (show_state == ui::SHOW_STATE_MINIMIZED ||
                            show_state == ui::SHOW_STATE_MAXIMIZED ||
                            show_state == ui::SHOW_STATE_FULLSCREEN)) {
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
  }

  if (params->update_info.focused) {
    bool focused = *params->update_info.focused;
    // A window cannot be focused and minimized, or not focused and maximized
    // or fullscreened.
    if (focused && show_state == ui::SHOW_STATE_MINIMIZED)
      return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
    if (!focused && (show_state == ui::SHOW_STATE_MAXIMIZED ||
                     show_state == ui::SHOW_STATE_FULLSCREEN)) {
      return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
    }
  }

  // Parameters are valid. Now to perform the actual updates.

  // state will be WINDOW_STATE_NONE if the state parameter wasn't passed from
  // the JS side, and in that case we don't want to change the locked state.
  if (is_locked_fullscreen &&
      params->update_info.state != windows::WINDOW_STATE_LOCKED_FULLSCREEN &&
      params->update_info.state != windows::WINDOW_STATE_NONE) {
    SetLockedFullscreenState(browser, /*pinned=*/false);
  } else if (!is_locked_fullscreen &&
             params->update_info.state ==
                 windows::WINDOW_STATE_LOCKED_FULLSCREEN) {
    SetLockedFullscreenState(browser, /*pinned=*/true);
  }

  if (show_state != ui::SHOW_STATE_FULLSCREEN &&
      show_state != ui::SHOW_STATE_DEFAULT) {
    browser->extension_window_controller()->SetFullscreenMode(
        false, extension()->url());
  }

  switch (show_state) {
    case ui::SHOW_STATE_MINIMIZED:
      browser->window()->Minimize();
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      browser->window()->Maximize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      if (browser->window()->IsMinimized() ||
          browser->window()->IsMaximized()) {
        browser->window()->Restore();
      }
      browser->extension_window_controller()->SetFullscreenMode(
          true, extension()->url());
      break;
    case ui::SHOW_STATE_NORMAL:
      browser->window()->Restore();
      break;
    default:
      break;
  }

  if (set_window_bounds) {
    // TODO(varkha): Updating bounds during a drag can cause problems and a more
    // general solution is needed. See http://crbug.com/251813 .
    browser->window()->SetBounds(window_bounds);
  }

  if (params->update_info.focused) {
    if (*params->update_info.focused) {
      browser->window()->Activate();
    } else {
      browser->window()->Deactivate();
    }
  }

  if (params->update_info.draw_attention)
    browser->window()->FlashFrame(*params->update_info.draw_attention);

  return RespondNow(
      WithArguments(ExtensionTabUtil::CreateWindowValueForExtension(
          *browser, extension(), ExtensionTabUtil::kDontPopulateTabs,
          source_context_type())));
}

ExtensionFunction::ResponseAction WindowsRemoveFunction::Run() {
  absl::optional<windows::Remove::Params> params =
      windows::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(this, params->window_id,
                                            WindowController::kNoWindowFilter,
                                            &browser, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (platform_util::IsBrowserLockedFullscreen(browser) &&
      !ExtensionHasLockedFullscreenPermission(extension())) {
    return RespondNow(
        Error(tabs_constants::kMissingLockWindowFullscreenPrivatePermission));
  }

  WindowController* controller = browser->extension_window_controller();
  WindowController::Reason reason;
  if (!controller->CanClose(&reason)) {
    return RespondNow(Error(reason == WindowController::REASON_NOT_EDITABLE
                                ? tabs_constants::kTabStripNotEditableError
                                : kUnknownErrorDoNotUse));
  }
  controller->window()->Close();
  return RespondNow(NoArguments());
}

// Tabs ------------------------------------------------------------------------

ExtensionFunction::ResponseAction TabsGetSelectedFunction::Run() {
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;

  absl::optional<tabs::GetSelected::Params> params =
      tabs::GetSelected::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->window_id)
    window_id = *params->window_id;

  Browser* browser = nullptr;
  std::string error;
  if (!GetBrowserFromWindowID(this, window_id, &browser, &error))
    return RespondNow(Error(std::move(error)));

  TabStripModel* tab_strip =
      ExtensionTabUtil::GetEditableTabStripModel(browser);
  if (!tab_strip)
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
  WebContents* contents = tab_strip->GetActiveWebContents();
  if (!contents)
    return RespondNow(Error(tabs_constants::kNoSelectedTabError));
  return RespondNow(ArgumentList(tabs::Get::Results::Create(
      CreateTabObjectHelper(contents, extension(), source_context_type(),
                            tab_strip, tab_strip->active_index()))));
}

ExtensionFunction::ResponseAction TabsGetAllInWindowFunction::Run() {
  absl::optional<tabs::GetAllInWindow::Params> params =
      tabs::GetAllInWindow::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (params->window_id)
    window_id = *params->window_id;

  Browser* browser = nullptr;
  std::string error;
  if (!GetBrowserFromWindowID(this, window_id, &browser, &error))
    return RespondNow(Error(std::move(error)));

  return RespondNow(WithArguments(ExtensionTabUtil::CreateTabList(
      browser, extension(), source_context_type())));
}

ExtensionFunction::ResponseAction TabsQueryFunction::Run() {
  absl::optional<tabs::Query::Params> params =
      tabs::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool loading_status_set = params->query_info.status != tabs::TAB_STATUS_NONE;

  URLPatternSet url_patterns;
  if (params->query_info.url) {
    std::vector<std::string> url_pattern_strings;
    if (params->query_info.url->as_string)
      url_pattern_strings.push_back(*params->query_info.url->as_string);
    else if (params->query_info.url->as_strings)
      url_pattern_strings.swap(*params->query_info.url->as_strings);
    // It is o.k. to use URLPattern::SCHEME_ALL here because this function does
    // not grant access to the content of the tabs, only to seeing their URLs
    // and meta data.
    std::string error;
    if (!url_patterns.Populate(url_pattern_strings, URLPattern::SCHEME_ALL,
                               true, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  std::string title = params->query_info.title.value_or(std::string());

  int window_id = extension_misc::kUnknownWindowId;
  if (params->query_info.window_id)
    window_id = *params->query_info.window_id;

  absl::optional<int> group_id = absl::nullopt;
  if (params->query_info.group_id)
    group_id = *params->query_info.group_id;

  int index = -1;
  if (params->query_info.index)
    index = *params->query_info.index;

  std::string window_type;
  if (params->query_info.window_type != tabs::WINDOW_TYPE_NONE)
    window_type = tabs::ToString(params->query_info.window_type);

  base::Value::List result;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* last_active_browser =
      chrome::FindAnyBrowser(profile, include_incognito_information());
  Browser* current_browser =
      ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
  for (auto* browser : *BrowserList::GetInstance()) {
    if (!profile->IsSameOrParent(browser->profile()))
      continue;

    if (!browser->window())
      continue;

    if (!include_incognito_information() && profile != browser->profile())
      continue;

    if (!browser->extension_window_controller()->IsVisibleToTabsAPIForExtension(
            extension(), false /*allow_dev_tools_windows*/)) {
      continue;
    }

    if (window_id >= 0 && window_id != ExtensionTabUtil::GetWindowId(browser))
      continue;

    if (window_id == extension_misc::kCurrentWindowId &&
        browser != current_browser) {
      continue;
    }

    if (!MatchesBool(params->query_info.current_window,
                     browser == current_browser)) {
      continue;
    }

    if (!MatchesBool(params->query_info.last_focused_window,
                     browser == last_active_browser)) {
      continue;
    }

    if (!window_type.empty() &&
        window_type !=
            browser->extension_window_controller()->GetWindowTypeText()) {
      continue;
    }

    TabStripModel* tab_strip = browser->tab_strip_model();
    DCHECK(tab_strip);
    for (int i = 0; i < tab_strip->count(); ++i) {
      WebContents* web_contents = tab_strip->GetWebContentsAt(i);

      if (index > -1 && i != index)
        continue;

      if (!web_contents)
        continue;

      if (!MatchesBool(params->query_info.highlighted,
                       tab_strip->IsTabSelected(i))) {
        continue;
      }

      if (!MatchesBool(params->query_info.active,
                       i == tab_strip->active_index())) {
        continue;
      }

      if (!MatchesBool(params->query_info.pinned, tab_strip->IsTabPinned(i))) {
        continue;
      }

      if (group_id.has_value()) {
        absl::optional<tab_groups::TabGroupId> group =
            tab_strip->GetTabGroupForTab(i);
        if (group_id.value() == -1) {
          if (group.has_value())
            continue;
        } else if (!group.has_value()) {
          continue;
        } else if (tab_groups_util::GetGroupId(group.value()) !=
                   group_id.value()) {
          continue;
        }
      }

      auto* audible_helper =
          RecentlyAudibleHelper::FromWebContents(web_contents);
      if (!MatchesBool(params->query_info.audible,
                       audible_helper->WasRecentlyAudible())) {
        continue;
      }

      auto* tab_lifecycle_unit_external =
          resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
              web_contents);

      if (!MatchesBool(params->query_info.discarded,
                       tab_lifecycle_unit_external->IsDiscarded())) {
        continue;
      }

      if (!MatchesBool(params->query_info.auto_discardable,
                       tab_lifecycle_unit_external->IsAutoDiscardable())) {
        continue;
      }

      if (!MatchesBool(params->query_info.muted,
                       web_contents->IsAudioMuted())) {
        continue;
      }

      if (!title.empty() || !url_patterns.is_empty()) {
        // "title" and "url" properties are considered privileged data and can
        // only be checked if the extension has the "tabs" permission or it has
        // access to the WebContents's origin. Otherwise, this tab is considered
        // not matched.
        if (!extension_->permissions_data()->HasAPIPermissionForTab(
                ExtensionTabUtil::GetTabId(web_contents),
                mojom::APIPermissionID::kTab) &&
            !extension_->permissions_data()->HasHostPermission(
                web_contents->GetURL())) {
          continue;
        }

        if (!title.empty() &&
            !base::MatchPattern(web_contents->GetTitle(),
                                base::UTF8ToUTF16(title))) {
          continue;
        }

        if (!url_patterns.is_empty() &&
            !url_patterns.MatchesURL(web_contents->GetURL())) {
          continue;
        }
      }

      if (loading_status_set &&
          params->query_info.status !=
              ExtensionTabUtil::GetLoadingStatus(web_contents)) {
        continue;
      }

      result.Append(CreateTabObjectHelper(web_contents, extension(),
                                          source_context_type(), tab_strip, i)
                        .ToValue());
    }
  }

  return RespondNow(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  absl::optional<tabs::Create::Params> params =
      tabs::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!ExtensionTabUtil::IsTabStripEditable())
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));

  ExtensionTabUtil::OpenTabParams options;
  options.window_id = params->create_properties.window_id;
  options.opener_tab_id = params->create_properties.opener_tab_id;
  options.active = params->create_properties.selected;
  // The 'active' property has replaced the 'selected' property.
  options.active = params->create_properties.active;
  options.pinned = params->create_properties.pinned;
  options.index = params->create_properties.index;
  options.url = params->create_properties.url;

  std::string error;
  auto result = ExtensionTabUtil::OpenTab(this, options, user_gesture());
  if (!result.has_value())
    return RespondNow(Error(result.error()));

  // Return data about the newly created tab.
  return RespondNow(has_callback() ? WithArguments(std::move(*result))
                                   : NoArguments());
}

ExtensionFunction::ResponseAction TabsDuplicateFunction::Run() {
  absl::optional<tabs::Duplicate::Params> params =
      tabs::Duplicate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!ExtensionTabUtil::IsTabStripEditable())
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
  int tab_id = params->tab_id;

  Browser* browser = nullptr;
  TabStripModel* tab_strip = nullptr;
  int tab_index = -1;
  std::string error;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  &browser, &tab_strip, nullptr, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (!ExtensionTabUtil::IsTabStripEditable())
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));

  if (!chrome::CanDuplicateTabAt(browser, tab_index)) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        tabs_constants::kCannotDuplicateTab, base::NumberToString(tab_id))));
  }

  WebContents* new_contents = chrome::DuplicateTabAt(browser, tab_index);
  if (!has_callback())
    return RespondNow(NoArguments());

  // Duplicated tab may not be in the same window as the original, so find
  // the window and the tab.
  TabStripModel* new_tab_strip = nullptr;
  int new_tab_index = -1;
  ExtensionTabUtil::GetTabStripModel(new_contents,
                                     &new_tab_strip,
                                     &new_tab_index);
  if (!new_tab_strip || new_tab_index == -1) {
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  return RespondNow(ArgumentList(tabs::Get::Results::Create(
      CreateTabObjectHelper(new_contents, extension(), source_context_type(),
                            new_tab_strip, new_tab_index))));
}

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  absl::optional<tabs::Get::Params> params = tabs::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  int tab_id = params->tab_id;

  TabStripModel* tab_strip = nullptr;
  WebContents* contents = nullptr;
  int tab_index = -1;
  std::string error;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  nullptr, &tab_strip, &contents, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  return RespondNow(ArgumentList(tabs::Get::Results::Create(
      CreateTabObjectHelper(contents, extension(), source_context_type(),
                            tab_strip, tab_index))));
}

ExtensionFunction::ResponseAction TabsGetCurrentFunction::Run() {
  DCHECK(dispatcher());

  // Return the caller, if it's a tab. If not the result isn't an error but an
  // empty tab (hence returning true).
  WebContents* caller_contents = GetSenderWebContents();
  if (caller_contents && ExtensionTabUtil::GetTabId(caller_contents) >= 0) {
    return RespondNow(ArgumentList(tabs::Get::Results::Create(
        CreateTabObjectHelper(caller_contents, extension(),
                              source_context_type(), nullptr, -1))));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsHighlightFunction::Run() {
  absl::optional<tabs::Highlight::Params> params =
      tabs::Highlight::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Get the window id from the params; default to current window if omitted.
  int window_id = params->highlight_info.window_id.value_or(
      extension_misc::kCurrentWindowId);

  Browser* browser = nullptr;
  std::string error;
  if (!GetBrowserFromWindowID(this, window_id, &browser, &error))
    return RespondNow(Error(std::move(error)));

  // Don't let the extension update the tab if the user is dragging tabs.
  TabStripModel* tabstrip = ExtensionTabUtil::GetEditableTabStripModel(browser);
  if (!tabstrip)
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
  ui::ListSelectionModel selection;
  absl::optional<size_t> active_index;

  if (params->highlight_info.tabs.as_integers) {
    std::vector<int>& tab_indices = *params->highlight_info.tabs.as_integers;
    // Create a new selection model as we read the list of tab indices.
    for (int tab_index : tab_indices) {
      if (!HighlightTab(tabstrip, &selection, &active_index, tab_index,
                        &error)) {
        return RespondNow(Error(std::move(error)));
      }
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->highlight_info.tabs.as_integer);
    if (!HighlightTab(tabstrip, &selection, &active_index,
                      *params->highlight_info.tabs.as_integer, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  // Make sure they actually specified tabs to select.
  if (selection.empty())
    return RespondNow(Error(tabs_constants::kNoHighlightedTabError));

  selection.set_active(active_index);
  TabStripModel* tab_strip_model =
      ExtensionTabUtil::GetEditableTabStripModel(browser);
  if (!tab_strip_model)
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
  tab_strip_model->SetSelectionFromModel(std::move(selection));
  return RespondNow(
      WithArguments(ExtensionTabUtil::CreateWindowValueForExtension(
          *browser, extension(), ExtensionTabUtil::kPopulateTabs,
          source_context_type())));
}

bool TabsHighlightFunction::HighlightTab(TabStripModel* tabstrip,
                                         ui::ListSelectionModel* selection,
                                         absl::optional<size_t>* active_index,
                                         int index,
                                         std::string* error) {
  // Make sure the index is in range.
  if (!tabstrip->ContainsIndex(index)) {
    *error = ErrorUtils::FormatErrorMessage(
        tabs_constants::kTabIndexNotFoundError, base::NumberToString(index));
    return false;
  }

  // By default, we make the first tab in the list active.
  if (!active_index->has_value())
    *active_index = static_cast<size_t>(index);

  selection->AddIndexToSelection(index);
  return true;
}

TabsUpdateFunction::TabsUpdateFunction() : web_contents_(nullptr) {}

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  absl::optional<tabs::Update::Params> params =
      tabs::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = -1;
  WebContents* contents = nullptr;
  if (!params->tab_id) {
    Browser* browser = ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
    if (!browser)
      return RespondNow(Error(tabs_constants::kNoCurrentWindowError));
    TabStripModel* tab_strip_model =
        ExtensionTabUtil::GetEditableTabStripModel(browser);
    if (!tab_strip_model)
      return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
    contents = tab_strip_model->GetActiveWebContents();
    if (!contents)
      return RespondNow(Error(tabs_constants::kNoSelectedTabError));
    tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
  } else {
    tab_id = *params->tab_id;
  }

  int tab_index = -1;
  TabStripModel* tab_strip = nullptr;
  Browser* browser = nullptr;
  std::string error;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  &browser, &tab_strip, &contents, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (DevToolsWindow::IsDevToolsWindow(contents))
    return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));

  if (!ExtensionTabUtil::BrowserSupportsTabs(browser))
    return RespondNow(Error(tabs_constants::kNoCurrentWindowError));

  web_contents_ = contents;

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url is different.
  if (params->update_properties.url) {
    std::string updated_url = *params->update_properties.url;
    if (browser->profile()->IsIncognitoProfile() &&
        !IsURLAllowedInIncognito(GURL(updated_url), browser->profile())) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          tabs_constants::kURLsNotAllowedInIncognitoError, updated_url)));
    }
    if (!UpdateURL(updated_url, tab_id, &error))
      return RespondNow(Error(std::move(error)));
  }

  bool active = false;
  // TODO(rafaelw): Setting |active| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (params->update_properties.selected)
    active = *params->update_properties.selected;

  // The 'active' property has replaced 'selected'.
  if (params->update_properties.active)
    active = *params->update_properties.active;

  if (active) {
    // Bug fix for crbug.com/1197888. Don't let the extension update the tab if
    // the user is dragging tabs.
    if (!ExtensionTabUtil::IsTabStripEditable())
      return RespondNow(Error(tabs_constants::kTabStripNotEditableError));

    if (tab_strip->active_index() != tab_index) {
      tab_strip->ActivateTabAt(tab_index);
      DCHECK_EQ(contents, tab_strip->GetActiveWebContents());
    }
  }

  if (params->update_properties.highlighted) {
    // Bug fix for crbug.com/1197888. Don't let the extension update the tab if
    // the user is dragging tabs.
    if (!ExtensionTabUtil::IsTabStripEditable())
      return RespondNow(Error(tabs_constants::kTabStripNotEditableError));

    bool highlighted = *params->update_properties.highlighted;
    if (highlighted != tab_strip->IsTabSelected(tab_index)) {
      tab_strip->ToggleSelectionAt(tab_index);
    }
  }

  if (params->update_properties.pinned) {
    // Bug fix for crbug.com/1197888. Don't let the extension update the tab if
    // the user is dragging tabs.
    if (!ExtensionTabUtil::IsTabStripEditable())
      return RespondNow(Error(tabs_constants::kTabStripNotEditableError));

    bool pinned = *params->update_properties.pinned;
    tab_strip->SetTabPinned(tab_index, pinned);

    // Update the tab index because it may move when being pinned.
    tab_index = tab_strip->GetIndexOfWebContents(contents);
  }

  if (params->update_properties.muted &&
      !chrome::SetTabAudioMuted(contents, *params->update_properties.muted,
                                TabMutedReason::EXTENSION, extension()->id())) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        tabs_constants::kCannotUpdateMuteCaptured,
        base::NumberToString(tab_id))));
  }

  if (params->update_properties.opener_tab_id) {
    int opener_id = *params->update_properties.opener_tab_id;
    WebContents* opener_contents = nullptr;
    if (opener_id == tab_id)
      return RespondNow(Error("Cannot set a tab's opener to itself."));
    if (!ExtensionTabUtil::GetTabById(opener_id, browser_context(),
                                      include_incognito_information(), nullptr,
                                      nullptr, &opener_contents, nullptr)) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          tabs_constants::kTabNotFoundError, base::NumberToString(opener_id))));
    }

    // Bug fix for crbug.com/1197888. Don't let the extension update the tab if
    // the user is dragging tabs.
    if (!ExtensionTabUtil::IsTabStripEditable())
      return RespondNow(Error(tabs_constants::kTabStripNotEditableError));

    if (tab_strip->GetIndexOfWebContents(opener_contents) ==
        TabStripModel::kNoTab) {
      return RespondNow(
          Error("Tab opener must be in the same window as the updated tab."));
    }
    tab_strip->SetOpenerOfWebContentsAt(tab_index, opener_contents);
  }

  if (params->update_properties.auto_discardable) {
    bool state = *params->update_properties.auto_discardable;
    resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
        web_contents_)
        ->SetAutoDiscardable(state);
  }

  return RespondNow(GetResult());
}

bool TabsUpdateFunction::UpdateURL(const std::string& url_string,
                                   int tab_id,
                                   std::string* error) {
  auto url = ExtensionTabUtil::PrepareURLForNavigation(url_string, extension(),
                                                       browser_context());
  if (!url.has_value()) {
    *error = std::move(url.error());
    return false;
  }

  // JavaScript URLs are forbidden in chrome.tabs.update().
  if (url->SchemeIs(url::kJavaScriptScheme)) {
    *error = tabs_constants::kJavaScriptUrlsNotAllowedInTabsUpdate;
    return false;
  }

  NavigationController::LoadURLParams load_params(*url);

  // Treat extension-initiated navigations as renderer-initiated so that the URL
  // does not show in the omnibox until it commits.  This avoids URL spoofs
  // since URLs can be opened on behalf of untrusted content.
  load_params.is_renderer_initiated = true;
  // All renderer-initiated navigations need to have an initiator origin.
  load_params.initiator_origin = extension()->origin();
  // |source_site_instance| needs to be set so that a renderer process
  // compatible with |initiator_origin| is picked by Site Isolation.
  load_params.source_site_instance = content::SiteInstance::CreateForURL(
      web_contents_->GetBrowserContext(),
      load_params.initiator_origin->GetURL());

  // Marking the navigation as initiated via an API means that the focus
  // will stay in the omnibox - see https://crbug.com/1085779.
  load_params.transition_type = ui::PAGE_TRANSITION_FROM_API;

  web_contents_->GetController().LoadURLWithParams(load_params);

  DCHECK_EQ(*url,
            web_contents_->GetController().GetPendingEntry()->GetVirtualURL());

  return true;
}

ExtensionFunction::ResponseValue TabsUpdateFunction::GetResult() {
  if (!has_callback())
    return NoArguments();

  return ArgumentList(tabs::Get::Results::Create(CreateTabObjectHelper(
      web_contents_, extension(), source_context_type(), nullptr, -1)));
}

ExtensionFunction::ResponseAction TabsMoveFunction::Run() {
  absl::optional<tabs::Move::Params> params =
      tabs::Move::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int new_index = params->move_properties.index;
  const auto& window_id = params->move_properties.window_id;
  base::Value::List tab_values;

  size_t num_tabs = 0;
  std::string error;
  if (params->tab_ids.as_integers) {
    std::vector<int>& tab_ids = *params->tab_ids.as_integers;
    num_tabs = tab_ids.size();
    for (int tab_id : tab_ids) {
      if (!MoveTab(tab_id, &new_index, tab_values, window_id, &error))
        return RespondNow(Error(std::move(error)));
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    num_tabs = 1;
    if (!MoveTab(*params->tab_ids.as_integer, &new_index, tab_values, window_id,
                 &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  // TODO(devlin): It's weird that whether or not the method provides a callback
  // can determine its success (as we return errors below).
  if (!has_callback())
    return RespondNow(NoArguments());

  if (num_tabs == 0)
    return RespondNow(Error("No tabs given."));
  if (num_tabs == 1) {
    CHECK_EQ(1u, tab_values.size());
    return RespondNow(WithArguments(std::move(tab_values[0])));
  }

  // Return the results as an array if there are multiple tabs.
  return RespondNow(WithArguments(std::move(tab_values)));
}

bool TabsMoveFunction::MoveTab(int tab_id,
                               int* new_index,
                               base::Value::List& tab_values,
                               const absl::optional<int>& window_id,
                               std::string* error) {
  Browser* source_browser = nullptr;
  TabStripModel* source_tab_strip = nullptr;
  WebContents* contents = nullptr;
  int tab_index = -1;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  &source_browser, &source_tab_strip, &contents, &tab_index,
                  error)) {
    return false;
  }

  if (DevToolsWindow::IsDevToolsWindow(contents)) {
    *error = tabs_constants::kNotAllowedForDevToolsError;
    return false;
  }

  // Don't let the extension move the tab if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    *error = tabs_constants::kTabStripNotEditableError;
    return false;
  }

  if (window_id && *window_id != ExtensionTabUtil::GetWindowIdOfTab(contents)) {
    Browser* target_browser = nullptr;
    if (!GetBrowserFromWindowID(this, *window_id, &target_browser, error))
      return false;

    int inserted_index =
        MoveTabToWindow(this, tab_id, target_browser, *new_index, error);
    if (inserted_index < 0)
      return false;

    *new_index = inserted_index;

    if (has_callback()) {
      TabStripModel* tab_strip_model =
          ExtensionTabUtil::GetEditableTabStripModel(target_browser);
      DCHECK(tab_strip_model);
      content::WebContents* web_contents =
          tab_strip_model->GetWebContentsAt(inserted_index);

      tab_values.Append(CreateTabObjectHelper(web_contents, extension(),
                                              source_context_type(),
                                              tab_strip_model, inserted_index)
                            .ToValue());
    }

    // Insert the tabs one after another.
    *new_index += 1;

    return true;
  }

  // Perform a simple within-window move.
  // Clamp move location to the last position.
  // This is ">=" because the move must be to an existing location.
  // -1 means set the move location to the last position.
  if (*new_index >= source_tab_strip->count() || *new_index < 0)
    *new_index = source_tab_strip->count() - 1;

  if (*new_index != tab_index)
    *new_index =
        source_tab_strip->MoveWebContentsAt(tab_index, *new_index, false);

  if (has_callback()) {
    tab_values.Append(CreateTabObjectHelper(contents, extension(),
                                            source_context_type(),
                                            source_tab_strip, *new_index)
                          .ToValue());
  }

  // Insert the tabs one after another.
  *new_index += 1;

  return true;
}

ExtensionFunction::ResponseAction TabsReloadFunction::Run() {
  absl::optional<tabs::Reload::Params> params =
      tabs::Reload::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool bypass_cache = false;
  if (params->reload_properties && params->reload_properties->bypass_cache) {
    bypass_cache = *params->reload_properties->bypass_cache;
  }

  content::WebContents* web_contents = nullptr;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  Browser* current_browser =
      ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
  if (!params->tab_id) {
    if (!current_browser)
      return RespondNow(Error(tabs_constants::kNoCurrentWindowError));

    if (!ExtensionTabUtil::GetDefaultTab(current_browser, &web_contents,
                                         nullptr))
      return RespondNow(Error(kUnknownErrorDoNotUse));
  } else {
    int tab_id = *params->tab_id;

    Browser* browser = nullptr;
    std::string error;
    if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                    &browser, nullptr, &web_contents, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  web_contents->GetController().Reload(
      bypass_cache ? content::ReloadType::BYPASSING_CACHE
                   : content::ReloadType::NORMAL,
      true);

  return RespondNow(NoArguments());
}

TabsRemoveFunction::TabsRemoveFunction() = default;
TabsRemoveFunction::~TabsRemoveFunction() = default;

ExtensionFunction::ResponseAction TabsRemoveFunction::Run() {
  absl::optional<tabs::Remove::Params> params =
      tabs::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  if (params->tab_ids.as_integers) {
    std::vector<int>& tab_ids = *params->tab_ids.as_integers;
    for (int tab_id : tab_ids) {
      if (!RemoveTab(tab_id, &error))
        return RespondNow(Error(std::move(error)));
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    if (!RemoveTab(*params->tab_ids.as_integer, &error))
      return RespondNow(Error(std::move(error)));
  }
  triggered_all_tab_removals_ = true;
  DCHECK(!did_respond());
  // WebContentsDestroyed will return the response in most cases, except when
  // the last tab closed immediately (it won't return a response because
  // |triggered_all_tab_removals_| will still be false). In this case we should
  // return the response from here.
  if (remaining_tabs_count_ == 0) {
    return RespondNow(NoArguments());
  }
  return RespondLater();
}

bool TabsRemoveFunction::RemoveTab(int tab_id, std::string* error) {
  Browser* browser = nullptr;
  WebContents* contents = nullptr;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  &browser, nullptr, &contents, nullptr, error)) {
    return false;
  }

  // Don't let the extension remove a tab if the user is dragging tabs around.
  if (!browser->window()->IsTabStripEditable()) {
    *error = tabs_constants::kTabStripNotEditableError;
    return false;
  }
  // The tab might not immediately close after calling Close() below, so we
  // should wait until WebContentsDestroyed is called before responding.
  web_contents_destroyed_observers_.push_back(
      std::make_unique<WebContentsDestroyedObserver>(this, contents));
  // Ensure that we're going to keep this class alive until
  // |remaining_tabs_count| reaches zero. This relies on WebContents::Close()
  // always (eventually) resulting in a WebContentsDestroyed() call; otherwise,
  // this function will never respond and may leak.
  AddRef();
  remaining_tabs_count_++;

  // There's a chance that the tab is being dragged, or we're in some other
  // nested event loop. This code path ensures that the tab is safely closed
  // under such circumstances, whereas |TabStripModel::CloseWebContentsAt()|
  // does not.
  contents->Close();
  return true;
}

void TabsRemoveFunction::TabDestroyed() {
  DCHECK_GT(remaining_tabs_count_, 0);
  // One of the tabs we wanted to remove had been destroyed.
  remaining_tabs_count_--;
  // If we've triggered all the tab removals we need, and this is the last tab
  // we're waiting for and we haven't sent a response (it's possible that we've
  // responded earlier in case of errors, etc.), send a response.
  if (triggered_all_tab_removals_ && remaining_tabs_count_ == 0 &&
      !did_respond()) {
    Respond(NoArguments());
  }
  Release();
}

class TabsRemoveFunction::WebContentsDestroyedObserver
    : public content::WebContentsObserver {
 public:
  WebContentsDestroyedObserver(extensions::TabsRemoveFunction* owner,
                               content::WebContents* watched_contents)
      : content::WebContentsObserver(watched_contents), owner_(owner) {}

  ~WebContentsDestroyedObserver() override = default;
  WebContentsDestroyedObserver(const WebContentsDestroyedObserver&) = delete;
  WebContentsDestroyedObserver& operator=(const WebContentsDestroyedObserver&) =
      delete;

  // WebContentsObserver
  void WebContentsDestroyed() override { owner_->TabDestroyed(); }

 private:
  // Guaranteed to outlive this object.
  raw_ptr<extensions::TabsRemoveFunction> owner_;
};

ExtensionFunction::ResponseAction TabsGroupFunction::Run() {
  absl::optional<tabs::Group::Params> params =
      tabs::Group::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;

  // Get the target browser from the parameters.
  int group_id = -1;
  Browser* target_browser = nullptr;
  tab_groups::TabGroupId group = tab_groups::TabGroupId::CreateEmpty();
  if (params->options.group_id) {
    if (params->options.create_properties)
      return RespondNow(Error(tabs_constants::kGroupParamsError));

    group_id = *params->options.group_id;
    if (!tab_groups_util::GetGroupById(
            group_id, browser_context(), include_incognito_information(),
            &target_browser, &group, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  } else {
    int window_id = extension_misc::kCurrentWindowId;
    if (params->options.create_properties &&
        params->options.create_properties->window_id) {
      window_id = *params->options.create_properties->window_id;
    }
    if (!GetBrowserFromWindowID(this, window_id, &target_browser, &error))
      return RespondNow(Error(std::move(error)));
  }

  DCHECK(target_browser);
  if (!target_browser->window()->IsTabStripEditable())
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));

  // Get all tab IDs from parameters.
  std::vector<int> tab_ids;
  if (params->options.tab_ids.as_integers) {
    tab_ids = *params->options.tab_ids.as_integers;
    EXTENSION_FUNCTION_VALIDATE(!tab_ids.empty());
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->options.tab_ids.as_integer);
    tab_ids.push_back(*params->options.tab_ids.as_integer);
  }

  // Get each tab's current browser. All tabs will need to be moved into the
  // target browser before grouping.
  std::vector<Browser*> tab_browsers;
  tab_browsers.reserve(tab_ids.size());
  for (int tab_id : tab_ids) {
    Browser* tab_browser = nullptr;
    content::WebContents* web_contents = nullptr;
    if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                    &tab_browser, nullptr, &web_contents, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }
    tab_browsers.push_back(tab_browser);

    if (DevToolsWindow::IsDevToolsWindow(web_contents))
      return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
  }

  // Move all tabs to the target browser, appending to the end each time. Only
  // tabs that are not already in the target browser are moved.
  for (size_t i = 0; i < tab_ids.size(); ++i) {
    if (tab_browsers[i] != target_browser) {
      if (MoveTabToWindow(this, tab_ids[i], target_browser, -1, &error) < 0)
        return RespondNow(Error(std::move(error)));
    }
  }

  // Get the resulting tab indices in the target browser. We recalculate these
  // after all tabs are moved so that any callbacks are resolved and the indices
  // are final.
  std::vector<int> tab_indices;
  tab_indices.reserve(tab_ids.size());
  for (int tab_id : tab_ids) {
    int tab_index = -1;
    if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                    nullptr, nullptr, nullptr, &tab_index, &error)) {
      return RespondNow(Error(std::move(error)));
    }
    tab_indices.push_back(tab_index);
  }
  // Sort and dedupe these indices for processing in the tabstrip.
  std::sort(tab_indices.begin(), tab_indices.end());
  tab_indices.erase(std::unique(tab_indices.begin(), tab_indices.end()),
                    tab_indices.end());

  // Get the remaining group metadata and add the tabs to the group.
  // At this point, we assume this is a valid action due to the checks above.
  TabStripModel* tab_strip =
      ExtensionTabUtil::GetEditableTabStripModel(target_browser);
  if (!tab_strip)
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
  if (!tab_strip->SupportsTabGroups()) {
    return RespondNow(
        Error(tabs_constants::kTabStripDoesNotSupportTabGroupsError));
  }
  if (group.is_empty()) {
    group = tab_strip->AddToNewGroup(tab_indices);
    group_id = tab_groups_util::GetGroupId(group);
  } else {
    tab_strip->AddToExistingGroup(tab_indices, group);
  }

  DCHECK_GT(group_id, 0);

  return RespondNow(WithArguments(group_id));
}

ExtensionFunction::ResponseAction TabsUngroupFunction::Run() {
  absl::optional<tabs::Ungroup::Params> params =
      tabs::Ungroup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<int> tab_ids;
  if (params->tab_ids.as_integers) {
    tab_ids = *params->tab_ids.as_integers;
    EXTENSION_FUNCTION_VALIDATE(!tab_ids.empty());
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    tab_ids.push_back(*params->tab_ids.as_integer);
  }

  std::string error;
  for (int tab_id : tab_ids) {
    if (!UngroupTab(tab_id, &error))
      return RespondNow(Error(std::move(error)));
  }

  return RespondNow(NoArguments());
}

bool TabsUngroupFunction::UngroupTab(int tab_id, std::string* error) {
  Browser* browser = nullptr;
  TabStripModel* tab_strip_model = nullptr;
  int tab_index = -1;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  &browser, &tab_strip_model, nullptr, &tab_index, error)) {
    return false;
  }

  if (!browser->window()->IsTabStripEditable()) {
    *error = tabs_constants::kTabStripNotEditableError;
    return false;
  }

  if (!tab_strip_model->SupportsTabGroups()) {
    *error = tabs_constants::kTabStripDoesNotSupportTabGroupsError;
    return false;
  }

  tab_strip_model->RemoveFromGroup({tab_index});

  return true;
}

// static
bool TabsCaptureVisibleTabFunction::disable_throttling_for_test_ = false;

TabsCaptureVisibleTabFunction::TabsCaptureVisibleTabFunction()
    : chrome_details_(this) {
}

WebContentsCaptureClient::ScreenshotAccess
TabsCaptureVisibleTabFunction::GetScreenshotAccess(
    content::WebContents* web_contents) const {
  PrefService* service =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  if (service->GetBoolean(prefs::kDisableScreenshots))
    return ScreenshotAccess::kDisabledByPreferences;

  if (ExtensionsBrowserClient::Get()->IsScreenshotRestricted(web_contents))
    return ScreenshotAccess::kDisabledByDlp;

  return ScreenshotAccess::kEnabled;
}

bool TabsCaptureVisibleTabFunction::ClientAllowsTransparency() {
  return false;
}

WebContents* TabsCaptureVisibleTabFunction::GetWebContentsForID(
    int window_id,
    std::string* error) {
  Browser* browser = nullptr;
  if (!GetBrowserFromWindowID(chrome_details_, window_id, &browser, error))
    return nullptr;

  TabStripModel* tab_strip_model =
      ExtensionTabUtil::GetEditableTabStripModel(browser);
  if (!tab_strip_model) {
    *error = tabs_constants::kTabStripNotEditableError;
    return nullptr;
  }
  WebContents* contents = tab_strip_model->GetActiveWebContents();
  if (!contents) {
    *error = "No active web contents to capture";
    return nullptr;
  }

  if (!extension()->permissions_data()->CanCaptureVisiblePage(
          contents->GetLastCommittedURL(),
          sessions::SessionTabHelper::IdForTab(contents).id(), error,
          extensions::CaptureRequirement::kActiveTabOrAllUrls)) {
    return nullptr;
  }
  return contents;
}

ExtensionFunction::ResponseAction TabsCaptureVisibleTabFunction::Run() {
  using api::extension_types::ImageDetails;

  EXTENSION_FUNCTION_VALIDATE(has_args());
  int context_id = extension_misc::kCurrentWindowId;

  if (args().size() > 0 && args()[0].is_int())
    context_id = args()[0].GetInt();

  std::unique_ptr<ImageDetails> image_details;
  if (args().size() > 1) {
    image_details = ImageDetails::FromValueDeprecated(args()[1]);
  }

  std::string error;
  WebContents* contents = GetWebContentsForID(context_id, &error);
  if (!contents)
    return RespondNow(Error(std::move(error)));

  // NOTE: CaptureAsync() may invoke its callback from a background thread,
  // hence the BindPostTask().
  const CaptureResult capture_result = CaptureAsync(
      contents, image_details.get(),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &TabsCaptureVisibleTabFunction::CopyFromSurfaceComplete, this)));
  if (capture_result == OK) {
    // CopyFromSurfaceComplete might have already responded.
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  return RespondNow(Error(CaptureResultToErrorMessage(capture_result)));
}

void TabsCaptureVisibleTabFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  constexpr base::TimeDelta kSecond = base::Seconds(1);
  QuotaLimitHeuristic::Config limit = {
      tabs::MAX_CAPTURE_VISIBLE_TAB_CALLS_PER_SECOND, kSecond};

  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      limit, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_CAPTURE_VISIBLE_TAB_CALLS_PER_SECOND"));
}

bool TabsCaptureVisibleTabFunction::ShouldSkipQuotaLimiting() const {
  return disable_throttling_for_test_;
}

void TabsCaptureVisibleTabFunction::OnCaptureSuccess(const SkBitmap& bitmap) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&TabsCaptureVisibleTabFunction::EncodeBitmapOnWorkerThread,
                     this, base::SingleThreadTaskRunner::GetCurrentDefault(),
                     bitmap));
}

void TabsCaptureVisibleTabFunction::EncodeBitmapOnWorkerThread(
    scoped_refptr<base::TaskRunner> reply_task_runner,
    const SkBitmap& bitmap) {
  std::string base64_result;
  bool success = EncodeBitmap(bitmap, &base64_result);
  reply_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&TabsCaptureVisibleTabFunction::OnBitmapEncodedOnUIThread,
                     this, success, std::move(base64_result)));
}

void TabsCaptureVisibleTabFunction::OnBitmapEncodedOnUIThread(
    bool success,
    std::string base64_result) {
  if (!success) {
    OnCaptureFailure(FAILURE_REASON_ENCODING_FAILED);
    return;
  }

  Respond(WithArguments(std::move(base64_result)));
}

void TabsCaptureVisibleTabFunction::OnCaptureFailure(CaptureResult result) {
  Respond(Error(CaptureResultToErrorMessage(result)));
}

// static.
std::string TabsCaptureVisibleTabFunction::CaptureResultToErrorMessage(
    CaptureResult result) {
  const char* reason_description = "internal error";
  switch (result) {
    case FAILURE_REASON_READBACK_FAILED:
      reason_description = "image readback failed";
      break;
    case FAILURE_REASON_ENCODING_FAILED:
      reason_description = "encoding failed";
      break;
    case FAILURE_REASON_VIEW_INVISIBLE:
      reason_description = "view is invisible";
      break;
    case FAILURE_REASON_SCREEN_SHOTS_DISABLED:
      return tabs_constants::kScreenshotsDisabled;
    case FAILURE_REASON_SCREEN_SHOTS_DISABLED_BY_DLP:
      return tabs_constants::kScreenshotsDisabledByDlp;
    case OK:
      NOTREACHED() << "CaptureResultToErrorMessage should not be called"
                      " with a successful result";
      return kUnknownErrorDoNotUse;
  }
  return ErrorUtils::FormatErrorMessage("Failed to capture tab: *",
                                        reason_description);
}

void TabsCaptureVisibleTabFunction::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kDisableScreenshots, false);
}

ExtensionFunction::ResponseAction TabsDetectLanguageFunction::Run() {
  absl::optional<tabs::DetectLanguage::Params> params =
      tabs::DetectLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = 0;
  Browser* browser = nullptr;
  WebContents* contents = nullptr;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  std::string error;
  if (params->tab_id) {
    tab_id = *params->tab_id;
    if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                    &browser, nullptr, &contents, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }
    // TODO(devlin): Can this happen? GetTabById() should return false if
    // |browser| or |contents| is null.
    if (!browser || !contents)
      return RespondNow(Error(kUnknownErrorDoNotUse));
  } else {
    browser = ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
    if (!browser)
      return RespondNow(Error(tabs_constants::kNoCurrentWindowError));
    TabStripModel* tab_strip_model =
        ExtensionTabUtil::GetEditableTabStripModel(browser);
    if (!tab_strip_model)
      return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
    contents = tab_strip_model->GetActiveWebContents();
    if (!contents)
      return RespondNow(Error(tabs_constants::kNoSelectedTabError));
  }

  if (contents->GetController().NeedsReload()) {
    // If the tab hasn't been loaded, don't wait for the tab to load.
    return RespondNow(
        Error(tabs_constants::kCannotDetermineLanguageOfUnloadedTab));
  }

  AddRef();  // Balanced in RespondWithLanguage().

  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  if (!chrome_translate_client->GetLanguageState().source_language().empty()) {
    // Delay the callback invocation until after the current JS call has
    // returned.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TabsDetectLanguageFunction::RespondWithLanguage, this,
            chrome_translate_client->GetLanguageState().source_language()));
    return RespondLater();
  }

  // The tab contents does not know its language yet. Let's wait until it
  // receives it, or until the tab is closed/navigates to some other page.

  // Observe the WebContents' lifetime and navigations.
  Observe(contents);
  // Wait until the language is determined.
  chrome_translate_client->GetTranslateDriver()->AddLanguageDetectionObserver(
      this);
  is_observing_ = true;

  return RespondLater();
}

void TabsDetectLanguageFunction::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // Call RespondWithLanguage() with an empty string as we want to guarantee the
  // callback is called for every API call the extension made.
  RespondWithLanguage(std::string());
}

void TabsDetectLanguageFunction::WebContentsDestroyed() {
  // Call RespondWithLanguage() with an empty string as we want to guarantee the
  // callback is called for every API call the extension made.
  RespondWithLanguage(std::string());
}

void TabsDetectLanguageFunction::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  RespondWithLanguage(details.adopted_language);
}

void TabsDetectLanguageFunction::RespondWithLanguage(
    const std::string& language) {
  // Stop observing.
  if (is_observing_) {
    ChromeTranslateClient::FromWebContents(web_contents())
        ->GetTranslateDriver()
        ->RemoveLanguageDetectionObserver(this);
    Observe(nullptr);
  }

  Respond(WithArguments(language));
  Release();  // Balanced in Run()
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : chrome_details_(this), execute_tab_id_(-1) {
}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

ExecuteCodeFunction::InitResult ExecuteCodeInTabFunction::Init() {
  if (init_result_)
    return init_result_.value();

  if (args().size() < 2)
    return set_init_result(VALIDATION_FAILURE);

  const auto& tab_id_value = args()[0];
  // |tab_id| is optional so it's ok if it's not there.
  int tab_id = -1;
  if (tab_id_value.is_int()) {
    // But if it is present, it needs to be non-negative.
    tab_id = tab_id_value.GetInt();
    if (tab_id < 0) {
      return set_init_result(VALIDATION_FAILURE);
    }
  }

  // |details| are not optional.
  const base::Value& details_value = args()[1];
  if (!details_value.is_dict())
    return set_init_result(VALIDATION_FAILURE);
  std::unique_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(details_value.GetDict(), *details)) {
    return set_init_result(VALIDATION_FAILURE);
  }

  // If the tab ID wasn't given then it needs to be converted to the
  // currently active tab's ID.
  if (tab_id == -1) {
    Browser* browser = chrome_details_.GetCurrentBrowser();
    // Can happen during shutdown.
    if (!browser)
      return set_init_result_error(tabs_constants::kNoCurrentWindowError);
    content::WebContents* web_contents = nullptr;
    // Can happen during shutdown.
    if (!ExtensionTabUtil::GetDefaultTab(browser, &web_contents, &tab_id))
      return set_init_result_error(tabs_constants::kNoTabInBrowserWindowError);
  }

  execute_tab_id_ = tab_id;
  details_ = std::move(details);
  set_host_id(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension()->id()));
  return set_init_result(SUCCESS);
}

bool ExecuteCodeInTabFunction::ShouldInsertCSS() const {
  return false;
}

bool ExecuteCodeInTabFunction::ShouldRemoveCSS() const {
  return false;
}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage(std::string* error) {
  content::WebContents* contents = nullptr;

  // If |tab_id| is specified, look for the tab. Otherwise default to selected
  // tab in the current window.
  CHECK_GE(execute_tab_id_, 0);
  if (!GetTabById(execute_tab_id_, browser_context(),
                  include_incognito_information(), nullptr, nullptr, &contents,
                  nullptr, error)) {
    return false;
  }

  CHECK(contents);

  int frame_id = details_->frame_id ? *details_->frame_id
                                    : ExtensionApiFrameIdMap::kTopFrameId;
  content::RenderFrameHost* rfh =
      ExtensionApiFrameIdMap::GetRenderFrameHostById(contents, frame_id);
  if (!rfh) {
    *error = ErrorUtils::FormatErrorMessage(
        tabs_constants::kFrameNotFoundError, base::NumberToString(frame_id),
        base::NumberToString(execute_tab_id_));
    return false;
  }

  // Content scripts declared in manifest.json can access frames at about:-URLs
  // if the extension has permission to access the frame's origin, so also allow
  // programmatic content scripts at about:-URLs for allowed origins.
  GURL effective_document_url(rfh->GetLastCommittedURL());
  bool is_about_url = effective_document_url.SchemeIs(url::kAboutScheme);
  if (is_about_url && details_->match_about_blank &&
      *details_->match_about_blank) {
    effective_document_url = GURL(rfh->GetLastCommittedOrigin().Serialize());
  }

  if (!effective_document_url.is_valid()) {
    // Unknown URL, e.g. because no load was committed yet. Allow for now, the
    // renderer will check again and fail the injection if needed.
    return true;
  }

  // NOTE: This can give the wrong answer due to race conditions, but it is OK,
  // we check again in the renderer.
  if (!extension()->permissions_data()->CanAccessPage(effective_document_url,
                                                      execute_tab_id_, error)) {
    if (is_about_url &&
        extension()->permissions_data()->active_permissions().HasAPIPermission(
            mojom::APIPermissionID::kTab)) {
      *error = ErrorUtils::FormatErrorMessage(
          manifest_errors::kCannotAccessAboutUrl,
          rfh->GetLastCommittedURL().spec(),
          rfh->GetLastCommittedOrigin().Serialize());
    }
    return false;
  }

  return true;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor(
    std::string* error) {
  Browser* browser = nullptr;
  content::WebContents* contents = nullptr;

  bool success = GetTabById(execute_tab_id_, browser_context(),
                            include_incognito_information(), &browser, nullptr,
                            &contents, nullptr, error) &&
                 contents && browser;

  if (!success)
    return nullptr;

  return TabHelper::FromWebContents(contents)->script_executor();
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  return false;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  return GURL::EmptyGURL();
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

bool TabsRemoveCSSFunction::ShouldRemoveCSS() const {
  return true;
}

ExtensionFunction::ResponseAction TabsSetZoomFunction::Run() {
  absl::optional<tabs::SetZoom::Params> params =
      tabs::SetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(std::move(error)));

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error))
    return RespondNow(Error(std::move(error)));

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  double zoom_level =
      params->zoom_factor > 0
          ? blink::PageZoomFactorToZoomLevel(params->zoom_factor)
          : zoom_controller->GetDefaultZoomLevel();

  auto client = base::MakeRefCounted<ExtensionZoomRequestClient>(extension());
  if (!zoom_controller->SetZoomLevelByClient(zoom_level, client)) {
    // Tried to zoom a tab in disabled mode.
    return RespondNow(Error(tabs_constants::kCannotZoomDisabledTabError));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomFunction::Run() {
  absl::optional<tabs::GetZoom::Params> params =
      tabs::GetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(std::move(error)));

  double zoom_level =
      ZoomController::FromWebContents(web_contents)->GetZoomLevel();
  double zoom_factor = blink::PageZoomLevelToZoomFactor(zoom_level);

  return RespondNow(ArgumentList(tabs::GetZoom::Results::Create(zoom_factor)));
}

ExtensionFunction::ResponseAction TabsSetZoomSettingsFunction::Run() {
  using api::tabs::ZoomSettings;

  absl::optional<tabs::SetZoomSettings::Params> params =
      tabs::SetZoomSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(std::move(error)));

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error))
    return RespondNow(Error(std::move(error)));

  // "per-origin" scope is only available in "automatic" mode.
  if (params->zoom_settings.scope == tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_AUTOMATIC &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_NONE) {
    return RespondNow(Error(tabs_constants::kPerOriginOnlyInAutomaticError));
  }

  // Determine the correct internal zoom mode to set |web_contents| to from the
  // user-specified |zoom_settings|.
  ZoomController::ZoomMode zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_settings.mode) {
    case tabs::ZOOM_SETTINGS_MODE_NONE:
    case tabs::ZOOM_SETTINGS_MODE_AUTOMATIC:
      switch (params->zoom_settings.scope) {
        case tabs::ZOOM_SETTINGS_SCOPE_NONE:
        case tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN:
          zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
          break;
        case tabs::ZOOM_SETTINGS_SCOPE_PER_TAB:
          zoom_mode = ZoomController::ZOOM_MODE_ISOLATED;
      }
      break;
    case tabs::ZOOM_SETTINGS_MODE_MANUAL:
      zoom_mode = ZoomController::ZOOM_MODE_MANUAL;
      break;
    case tabs::ZOOM_SETTINGS_MODE_DISABLED:
      zoom_mode = ZoomController::ZOOM_MODE_DISABLED;
  }

  ZoomController::FromWebContents(web_contents)->SetZoomMode(zoom_mode);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomSettingsFunction::Run() {
  absl::optional<tabs::GetZoomSettings::Params> params =
      tabs::GetZoomSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(std::move(error)));
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  ZoomController::ZoomMode zoom_mode = zoom_controller->zoom_mode();
  api::tabs::ZoomSettings zoom_settings;
  ZoomModeToZoomSettings(zoom_mode, &zoom_settings);
  zoom_settings.default_zoom_factor =
      blink::PageZoomLevelToZoomFactor(zoom_controller->GetDefaultZoomLevel());

  return RespondNow(
      ArgumentList(api::tabs::GetZoomSettings::Results::Create(zoom_settings)));
}

ExtensionFunction::ResponseAction TabsDiscardFunction::Run() {
  absl::optional<tabs::Discard::Params> params =
      tabs::Discard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebContents* contents = nullptr;
  // If |tab_id| is given, find the web_contents respective to it.
  // Otherwise invoke discard function in TabManager with null web_contents
  // that will discard the least important tab.
  if (params->tab_id) {
    int tab_id = *params->tab_id;
    std::string error;
    if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                    nullptr, nullptr, &contents, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }

    if (DevToolsWindow::IsDevToolsWindow(contents))
      return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
  }
  // Discard the tab.
  contents =
      g_browser_process->GetTabManager()->DiscardTabByExtension(contents);

  // Create the Tab object and return it in case of success.
  if (contents) {
    return RespondNow(
        ArgumentList(tabs::Discard::Results::Create(CreateTabObjectHelper(
            contents, extension(), source_context_type(), nullptr, -1))));
  }

  // Return appropriate error message otherwise.
  return RespondNow(Error(params->tab_id
                              ? ErrorUtils::FormatErrorMessage(
                                    tabs_constants::kCannotDiscardTab,
                                    base::NumberToString(*params->tab_id))
                              : tabs_constants::kCannotFindTabToDiscard));
}

TabsDiscardFunction::TabsDiscardFunction() {}
TabsDiscardFunction::~TabsDiscardFunction() {}

ExtensionFunction::ResponseAction TabsGoForwardFunction::Run() {
  absl::optional<tabs::GoForward::Params> params =
      tabs::GoForward::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(std::move(error)));

  NavigationController& controller = web_contents->GetController();
  if (!controller.CanGoForward())
    return RespondNow(Error(tabs_constants::kNotFoundNextPageError));

  controller.GoForward();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGoBackFunction::Run() {
  absl::optional<tabs::GoBack::Params> params =
      tabs::GoBack::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(std::move(error)));

  NavigationController& controller = web_contents->GetController();
  if (!controller.CanGoBack())
    return RespondNow(Error(tabs_constants::kNotFoundNextPageError));

  controller.GoBack();
  return RespondNow(NoArguments());
}

}  // namespace extensions
