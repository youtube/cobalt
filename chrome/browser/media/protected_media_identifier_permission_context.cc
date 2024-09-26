// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_permission_context.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/constants/dbus_switches.h"  // nogncheck
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#endif

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS))
#error This file currently only supports Chrome OS, Android and Windows.
#endif

ProtectedMediaIdentifierPermissionContext::
    ProtectedMediaIdentifierPermissionContext(
        content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
          blink::mojom::PermissionsPolicyFeature::kEncryptedMedia) {}

ProtectedMediaIdentifierPermissionContext::
    ~ProtectedMediaIdentifierPermissionContext() {
}

ContentSetting
ProtectedMediaIdentifierPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DVLOG(1) << __func__ << ": (" << requesting_origin.spec() << ", "
           << embedding_origin.spec() << ")";

  if (!requesting_origin.is_valid() || !embedding_origin.is_valid() ||
      !IsProtectedMediaIdentifierEnabled()) {
    return CONTENT_SETTING_BLOCK;
  }

  ContentSetting content_setting =
      permissions::PermissionContextBase::GetPermissionStatusInternal(
          render_frame_host, requesting_origin, embedding_origin);
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
#if BUILDFLAG(IS_ANDROID)
         content_setting == CONTENT_SETTING_ASK ||
#endif
         content_setting == CONTENT_SETTING_BLOCK);

  // For automated testing of protected content - having a prompt that
  // requires user intervention is problematic. If the domain has been
  // allowlisted as safe - suppress the request and allow.
  if (content_setting != CONTENT_SETTING_ALLOW &&
      IsOriginAllowed(requesting_origin)) {
    content_setting = CONTENT_SETTING_ALLOW;
  }

  return content_setting;
}

bool ProtectedMediaIdentifierPermissionContext::IsOriginAllowed(
    const GURL& origin) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  const std::string allowlist = command_line.GetSwitchValueASCII(
      switches::kUnsafelyAllowProtectedMediaIdentifierForDomain);

  for (const std::string& domain : base::SplitString(
           allowlist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (origin.DomainIs(domain)) {
      return true;
    }
  }

  return false;
}

void ProtectedMediaIdentifierPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // WebContents may have gone away.
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.global_render_frame_host_id());
  if (content_settings) {
    content_settings->OnProtectedMediaIdentifierPermissionSet(
        requesting_frame.DeprecatedGetOriginAsURL(), allowed);
  }
}

// TODO(xhwang): We should consolidate the "protected content" related pref
// across platforms.
bool ProtectedMediaIdentifierPermissionContext::
    IsProtectedMediaIdentifierEnabled() const {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  // Identifier is not allowed in incognito or guest mode.
  if (profile->IsOffTheRecord() || profile->IsGuestSession()) {
    DVLOG(1) << "Protected media identifier disabled in incognito or guest "
                "mode.";
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kSystemDevMode) &&
      !command_line->HasSwitch(ash::switches::kAllowRAInDevMode)) {
    DVLOG(1) << "Protected media identifier disabled in dev mode.";
    return false;
  }

  // This could be disabled by the device policy or by a switch in content
  // settings.
  bool enabled_for_device = false;
  if (!ash::CrosSettings::Get()->GetBoolean(
          ash::kAttestationForContentProtectionEnabled, &enabled_for_device) ||
      !enabled_for_device) {
    DVLOG(1) << "Protected media identifier disabled by the user or by device "
                "policy.";
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)

  return true;
}
