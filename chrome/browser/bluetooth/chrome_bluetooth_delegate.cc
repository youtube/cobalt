// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/bluetooth_delegate_impl.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_instance.h"
#include "url/gurl.h"

ChromeBluetoothDelegate::ChromeBluetoothDelegate(std::unique_ptr<Client> client)
    : permissions::BluetoothDelegateImpl(std::move(client)) {}

bool ChromeBluetoothDelegate::MayUseBluetooth(content::RenderFrameHost* rfh) {
  // Because permission is scoped to the profile, guest contexts (like
  // <webview>, <controlledframe>, and SlimWebView), despite having isolated
  // StoragePartitions, would share Bluetooth permissions with the rest of the
  // profile. Therefore, Bluetooth is not allowed in these contexts.
  if (rfh->GetSiteInstance()->GetSecurityPrincipal().IsGuest()) {
    return false;
  }

  // Disable any other non-default StoragePartition contexts, unless it has a
  // non-http/https scheme.
  if (rfh->GetStoragePartition() !=
      rfh->GetBrowserContext()->GetDefaultStoragePartition()) {
    return !rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS();
  }

  return true;
}

ChromeBluetoothDelegate::AllowWebBluetoothResult
ChromeBluetoothDelegate::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  // TODO(crbug.com/40462828): Don't disable if
  // base::CommandLine::ForCurrentProcess()->
  // HasSwitch(switches::kEnableWebBluetooth) is true.
  if (base::GetFieldTrialParamValue(
          permissions::ContentSettingPermissionContextBase::
              kPermissionsKillSwitchFieldStudy,
          "Bluetooth") == permissions::ContentSettingPermissionContextBase::
                              kPermissionsKillSwitchBlockedValue) {
    // The kill switch is enabled for this permission. Block requests.
    return AllowWebBluetoothResult::kBlockGloballyDisabled;
  }

  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (content_settings->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          ContentSettingsType::BLUETOOTH_GUARD) == CONTENT_SETTING_BLOCK) {
    return AllowWebBluetoothResult::kBlockPolicy;
  }
  return AllowWebBluetoothResult::kAllow;
}

std::string ChromeBluetoothDelegate::GetWebBluetoothBlocklist() {
  return base::GetFieldTrialParamValue("WebBluetoothBlocklist",
                                       "blocklist_additions");
}

bool ChromeBluetoothDelegate::IsBluetoothScanningBlocked(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (content_settings->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          ContentSettingsType::BLUETOOTH_SCANNING) == CONTENT_SETTING_BLOCK) {
    return true;
  }
  return false;
}

void ChromeBluetoothDelegate::BlockBluetoothScanning(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  content_settings->SetContentSettingDefaultScope(
      requesting_origin.GetURL(), embedding_origin.GetURL(),
      ContentSettingsType::BLUETOOTH_SCANNING, CONTENT_SETTING_BLOCK);
}
