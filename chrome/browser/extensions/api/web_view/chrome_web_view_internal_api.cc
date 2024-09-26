// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_view/chrome_web_view_internal_api.h"

#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/chrome_web_view_internal.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/error_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webview = extensions::api::chrome_web_view_internal;

namespace extensions {

// TODO(lazyboy): Add checks similar to
// WebViewInternalExtensionFunction::RunAsyncSafe(WebViewGuest*).
ExtensionFunction::ResponseAction
ChromeWebViewInternalContextMenusCreateFunction::Run() {
  absl::optional<webview::ContextMenusCreate::Params> params =
      webview::ContextMenusCreate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  MenuItem::Id id(
      Profile::FromBrowserContext(browser_context())->IsOffTheRecord(),
      MenuItem::ExtensionKey(
          extension_id(),
          GetSenderWebContents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
          params->instance_id));

  if (params->create_properties.id) {
    id.string_uid = *params->create_properties.id;
  } else {
    // The Generated Id is added by web_view_internal_custom_bindings.js.
    EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
    EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());
    const base::Value& properties = args()[1];
    EXTENSION_FUNCTION_VALIDATE(properties.is_dict());
    absl::optional<int> result = properties.GetDict().FindInt(
        extensions::context_menus_api_helpers::kGeneratedIdKey);
    EXTENSION_FUNCTION_VALIDATE(result);
    id.uid = *result;
  }

  std::string error;
  bool success = extensions::context_menus_api_helpers::CreateMenuItem(
      params->create_properties, Profile::FromBrowserContext(browser_context()),
      extension(), id, &error);
  return RespondNow(success ? NoArguments() : Error(error));
}

ExtensionFunction::ResponseAction
ChromeWebViewInternalContextMenusUpdateFunction::Run() {
  absl::optional<webview::ContextMenusUpdate::Params> params =
      webview::ContextMenusUpdate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  MenuItem::Id item_id(
      profile->IsOffTheRecord(),
      MenuItem::ExtensionKey(
          extension_id(),
          GetSenderWebContents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
          params->instance_id));

  if (params->id.as_string)
    item_id.string_uid = *params->id.as_string;
  else if (params->id.as_integer)
    item_id.uid = *params->id.as_integer;
  else
    NOTREACHED();

  std::string error;
  bool success = extensions::context_menus_api_helpers::UpdateMenuItem(
      params->update_properties, profile, extension(), item_id, &error);

  return RespondNow(success ? NoArguments() : Error(error));
}

ExtensionFunction::ResponseAction
ChromeWebViewInternalContextMenusRemoveFunction::Run() {
  absl::optional<webview::ContextMenusRemove::Params> params =
      webview::ContextMenusRemove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  MenuManager* menu_manager =
      MenuManager::Get(Profile::FromBrowserContext(browser_context()));

  MenuItem::Id id(
      Profile::FromBrowserContext(browser_context())->IsOffTheRecord(),
      MenuItem::ExtensionKey(
          extension_id(),
          GetSenderWebContents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
          params->instance_id));

  if (params->menu_item_id.as_string) {
    id.string_uid = *params->menu_item_id.as_string;
  } else if (params->menu_item_id.as_integer) {
    id.uid = *params->menu_item_id.as_integer;
  } else {
    NOTREACHED();
  }

  MenuItem* item = menu_manager->GetItemById(id);
  // Ensure one <webview> can't remove another's menu items.
  if (!item || item->id().extension_key != id.extension_key) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        context_menus_api_helpers::kCannotFindItemError,
        context_menus_api_helpers::GetIDString(id))));
  } else if (!menu_manager->RemoveContextMenuItem(id)) {
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ChromeWebViewInternalContextMenusRemoveAllFunction::Run() {
  absl::optional<webview::ContextMenusRemoveAll::Params> params =
      webview::ContextMenusRemoveAll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  MenuManager* menu_manager =
      MenuManager::Get(Profile::FromBrowserContext(browser_context()));
  menu_manager->RemoveAllContextItems(MenuItem::ExtensionKey(
      extension_id(),
      GetSenderWebContents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
      params->instance_id));

  return RespondNow(NoArguments());
}

ChromeWebViewInternalShowContextMenuFunction::
    ChromeWebViewInternalShowContextMenuFunction() {
}

ChromeWebViewInternalShowContextMenuFunction::
    ~ChromeWebViewInternalShowContextMenuFunction() {
}

ExtensionFunction::ResponseAction
ChromeWebViewInternalShowContextMenuFunction::Run() {
  absl::optional<webview::ShowContextMenu::Params> params =
      webview::ShowContextMenu::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(lazyboy): Actually implement filtering menu items.
  guest_->ShowContextMenu(params->request_id);
  return RespondNow(NoArguments());
}

}  // namespace extensions
