// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_registry.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"

namespace content_settings {

namespace {

base::LazyInstance<ContentSettingsRegistry>::DestructorAtExit
    g_content_settings_registry_instance = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ContentSettingsRegistry* ContentSettingsRegistry::GetInstance() {
  return g_content_settings_registry_instance.Pointer();
}

ContentSettingsRegistry::ContentSettingsRegistry()
    : ContentSettingsRegistry(WebsiteSettingsRegistry::GetInstance()) {}

ContentSettingsRegistry::ContentSettingsRegistry(
    WebsiteSettingsRegistry* website_settings_registry)
    // This object depends on WebsiteSettingsRegistry, so get it first so that
    // they will be destroyed in reverse order.
    : website_settings_registry_(website_settings_registry) {
  Init();
}

void ContentSettingsRegistry::ResetForTest() {
  website_settings_registry_->ResetForTest();
  content_settings_info_.clear();
  Init();
}

ContentSettingsRegistry::~ContentSettingsRegistry() = default;

const ContentSettingsInfo* ContentSettingsRegistry::Get(
    ContentSettingsType type) const {
  const auto& it = content_settings_info_.find(type);
  if (it != content_settings_info_.end())
    return it->second.get();
  return nullptr;
}

ContentSettingsRegistry::const_iterator ContentSettingsRegistry::begin() const {
  return const_iterator(content_settings_info_.begin());
}

ContentSettingsRegistry::const_iterator ContentSettingsRegistry::end() const {
  return const_iterator(content_settings_info_.end());
}

void ContentSettingsRegistry::Init() {
  // TODO(raymes): This registration code should not have to be in a single
  // location. It should be possible to register a setting from the code
  // associated with it.

  // WARNING: The string names of the permissions passed in below are used to
  // generate preference names and should never be changed!
  //
  // If a permission is DELETED, please update
  // PrefProvider::DiscardOrMigrateObsoletePreferences() and
  // DefaultProvider::DiscardOrMigrateObsoletePreferences() accordingly.

  Register(
      ContentSettingsType::COOKIES, "cookies", CONTENT_SETTING_ALLOW,
      WebsiteSettingsInfo::SYNCABLE,
      /*allowlisted_schemes=*/{kChromeUIScheme, kChromeDevToolsScheme},
      /*valid_settings=*/
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
       CONTENT_SETTING_SESSION_ONLY},
      WebsiteSettingsInfo::REQUESTING_ORIGIN_WITH_TOP_ORIGIN_EXCEPTIONS_SCOPE,
      WebsiteSettingsRegistry::ALL_PLATFORMS,
      ContentSettingsInfo::INHERIT_IN_INCOGNITO,
      ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::IMAGES, "images", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::SYNCABLE,
           /*allowlisted_schemes=*/
           {kChromeUIScheme, kChromeDevToolsScheme, kExtensionScheme},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_WITH_RESOURCE_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::JAVASCRIPT, "javascript", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::SYNCABLE,
           /*allowlisted_schemes=*/
           {kChromeUIScheme, kChromeDevToolsScheme, kExtensionScheme},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_WITH_RESOURCE_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::GET_DISPLAY_MEDIA_SET_SELECT_ALL_SCREENS,
           "get-display-media-set-select-all-screens", CONTENT_SETTING_BLOCK,
           WebsiteSettingsInfo::SYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::POPUPS, "popups", CONTENT_SETTING_BLOCK,
           WebsiteSettingsInfo::SYNCABLE,
           /*allowlisted_schemes=*/
           {kChromeUIScheme, kChromeDevToolsScheme, kExtensionScheme},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::GEOLOCATION, "geolocation", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::NOTIFICATIONS, "notifications",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           // See also NotificationPermissionContext::DecidePermission which
           // implements additional incognito exceptions.
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::MEDIASTREAM_MIC, "media-stream-mic",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{kChromeUIScheme, kChromeDevToolsScheme},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::MEDIASTREAM_CAMERA, "media-stream-camera",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{kChromeUIScheme, kChromeDevToolsScheme},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::AUTOMATIC_DOWNLOADS, "automatic-downloads",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::SYNCABLE,
           /*allowlisted_schemes=*/
           {kChromeUIScheme, kChromeDevToolsScheme, kExtensionScheme},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  // TODO(raymes): We're temporarily making midi sysex unsyncable while we roll
  // out the kPermissionDelegation feature. We may want to make it syncable
  // again sometime in the future. See https://crbug.com/879954 for details.
  Register(ContentSettingsType::MIDI_SYSEX, "midi-sysex", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
           "protected-media-identifier", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
#if BUILDFLAG(IS_ANDROID)
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
#else   // BUILDFLAG(IS_ANDROID)
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
#endif  // else
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::PLATFORM_ANDROID |
               WebsiteSettingsRegistry::PLATFORM_CHROMEOS |
               WebsiteSettingsRegistry::PLATFORM_WINDOWS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::DURABLE_STORAGE, "durable-storage",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::BACKGROUND_SYNC, "background-sync",
           CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::AUTOPLAY, "autoplay", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::SOUND, "sound", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::ADS, "subresource-filter",
           CONTENT_SETTING_BLOCK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::LEGACY_COOKIE_ACCESS, "legacy-cookie-access",
           CONTENT_SETTING_BLOCK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  // Content settings that aren't used to store any data. TODO(raymes): use a
  // different mechanism rather than content settings to represent these.
  // Since nothing is stored in them, there is no real point in them being a
  // content setting.
  Register(ContentSettingsType::PROTOCOL_HANDLERS, "protocol-handler",
           CONTENT_SETTING_DEFAULT, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{}, /*valid_settings=*/{},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::MIXEDSCRIPT, "mixed-script",
           CONTENT_SETTING_BLOCK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::BLUETOOTH_GUARD, "bluetooth-guard",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::ACCESSIBILITY_EVENTS, "accessibility-events",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  // TODO(crbug.com/904439): Update this to "SECURE_ONLY" once
  // DeviceOrientationEvents and DeviceMotionEvents are only fired in secure
  // contexts.
  Register(ContentSettingsType::SENSORS, "sensors", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::CLIPBOARD_READ_WRITE, "clipboard",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{kChromeUIScheme},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::PAYMENT_HANDLER, "payment-handler",
           CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::USB_GUARD, "usb-guard", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::SERIAL_GUARD, "serial-guard",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::BLUETOOTH_SCANNING, "bluetooth-scanning",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::HID_GUARD, "hid-guard", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
           "file-system-write-guard", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::FILE_SYSTEM_READ_GUARD,
           "file-system-read-guard", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  // The "nfc" name should not be used in the future to avoid name collisions.
  // See crbug.com/1275576
  Register(ContentSettingsType::NFC, "nfc-devices", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::VR, "vr", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::AR, "ar", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::STORAGE_ACCESS, "storage-access",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::REQUESTING_AND_TOP_SCHEMEFUL_SITE_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
           "top-level-storage-access", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::REQUESTING_AND_TOP_ORIGIN_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::CAMERA_PAN_TILT_ZOOM, "camera-pan-tilt-zoom",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{kChromeUIScheme, kChromeDevToolsScheme},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::WINDOW_MANAGEMENT, "window-placement",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::SYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::INSECURE_LOCAL_NETWORK,
           "insecure-private-network", CONTENT_SETTING_BLOCK,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::LOCAL_FONTS, "local-fonts", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::SYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::IDLE_DETECTION, "idle-detection",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/
           {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::JAVASCRIPT_JIT, "javascript-jit",
           CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::PRIVATE_NETWORK_GUARD, "private-network-guard",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  const auto auto_dark_web_content_setting =
#if BUILDFLAG(IS_ANDROID)
      content_settings::kDarkenWebsitesCheckboxOptOut.Get()
          ? CONTENT_SETTING_ALLOW
          : CONTENT_SETTING_BLOCK;
#else
      CONTENT_SETTING_ALLOW;
#endif  // BUILDFLAG(IS_ANDROID)

  Register(ContentSettingsType::AUTO_DARK_WEB_CONTENT, "auto-dark-web-content",
           auto_dark_web_content_setting, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::REQUEST_DESKTOP_SITE, "request-desktop-site",
           CONTENT_SETTING_BLOCK, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::PLATFORM_ANDROID |
               WebsiteSettingsRegistry::PLATFORM_IOS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);

  Register(ContentSettingsType::FEDERATED_IDENTITY_API, "webid-api",
           CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
           "webid-auto-reauthn", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::UNSYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::ANTI_ABUSE, "anti-abuse", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::SYNCABLE,
           /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);

  Register(ContentSettingsType::THIRD_PARTY_STORAGE_PARTITIONING,
           "third-party-storage-partitioning", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::UNSYNCABLE, /*allowlisted_schemes=*/{},
           /*valid_settings=*/{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS);
}

void ContentSettingsRegistry::Register(
    ContentSettingsType type,
    const std::string& name,
    ContentSetting initial_default_value,
    WebsiteSettingsInfo::SyncStatus sync_status,
    const std::vector<std::string>& allowlisted_schemes,
    const std::set<ContentSetting>& valid_settings,
    WebsiteSettingsInfo::ScopingType scoping_type,
    Platforms platforms,
    ContentSettingsInfo::IncognitoBehavior incognito_behavior,
    ContentSettingsInfo::OriginRestriction origin_restriction) {
  // Ensure that nothing has been registered yet for the given type.
  DCHECK(!website_settings_registry_->Get(type));

  base::Value default_value(static_cast<int>(initial_default_value));
  const WebsiteSettingsInfo* website_settings_info =
      website_settings_registry_->Register(
          type, name, std::move(default_value), sync_status,
          WebsiteSettingsInfo::NOT_LOSSY, scoping_type, platforms,
          WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);

  // WebsiteSettingsInfo::Register() will return nullptr if content setting type
  // is not used on the current platform and doesn't need to be registered.
  if (!website_settings_info)
    return;

  DCHECK(!base::Contains(content_settings_info_, type));
  content_settings_info_[type] = std::make_unique<ContentSettingsInfo>(
      website_settings_info, allowlisted_schemes, valid_settings,
      incognito_behavior, origin_restriction);
}

}  // namespace content_settings
