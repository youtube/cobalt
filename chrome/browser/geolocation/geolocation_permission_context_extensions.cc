// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_extensions.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_request_id.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/suggest_permission_util.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"

using extensions::APIPermission;
using extensions::ExtensionRegistry;
#endif

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
void CallbackContentSettingWrapper(
    base::OnceCallback<void(ContentSetting)> callback,
    bool allowed) {
  std::move(callback).Run(allowed ? CONTENT_SETTING_ALLOW
                                  : CONTENT_SETTING_BLOCK);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // anonymous namespace

GeolocationPermissionContextExtensions::GeolocationPermissionContextExtensions(
    Profile* profile)
#if BUILDFLAG(ENABLE_EXTENSIONS)
    : profile_(profile)
#endif
{
}

GeolocationPermissionContextExtensions::
~GeolocationPermissionContextExtensions() {
}

bool GeolocationPermissionContextExtensions::DecidePermission(
    const permissions::PermissionRequestID& request_id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::OnceCallback<void(ContentSetting)>* callback,
    bool* permission_set,
    bool* new_permission) {
#if BUILDFLAG(ENABLE_EXTENSIONS)

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_id.global_render_frame_host_id());

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  GURL requesting_frame_origin = requesting_frame.DeprecatedGetOriginAsURL();

  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHost(rfh);
  if (web_view_permission_helper) {
    web_view_permission_helper->RequestGeolocationPermission(
        requesting_frame, user_gesture,
        base::BindOnce(&CallbackContentSettingWrapper, std::move(*callback)));
    *permission_set = false;
    *new_permission = false;
    return true;
  }

  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  if (extension_registry) {
    const extensions::Extension* extension =
        extension_registry->enabled_extensions().GetExtensionOrAppByURL(
            requesting_frame_origin);
    if (IsExtensionWithPermissionOrSuggestInConsole(
            extensions::mojom::APIPermissionID::kGeolocation, extension,
            web_contents->GetPrimaryMainFrame())) {
      // Make sure the extension is in the calling process.
      if (extensions::ProcessMap::Get(profile_)->Contains(
              extension->id(),
              request_id.global_render_frame_host_id().child_id)) {
        *permission_set = true;
        *new_permission = true;
        return true;
      }
    }
  }

  extensions::mojom::ViewType view_type = extensions::GetViewType(web_contents);
  if (view_type != extensions::mojom::ViewType::kTabContents &&
      view_type != extensions::mojom::ViewType::kInvalid) {
    // The tab may have gone away, or the request may not be from a tab at all.
    // TODO(mpcomplete): the request could be from a background page or
    // extension popup (web_contents will have a different ViewType). But why do
    // we care? Shouldn't we still put an infobar up in the current tab?
    LOG(WARNING) << "Attempt to use geolocation tabless renderer: "
                 << request_id.ToString()
                 << " (can't prompt user without a visible tab)";
    *permission_set = true;
    *new_permission = false;
    return true;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
}
