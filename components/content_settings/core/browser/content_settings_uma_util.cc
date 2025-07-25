// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace {

// WARNING: The value specified here for a type should match exactly the value
// specified in the ContentType enum in enums.xml. Since these values are
// used for histograms, please do not reuse the same value for a different
// content setting. Always append to the end and increment.
constexpr auto kHistogramValue = base::MakeFixedFlatMap<ContentSettingsType,
                                                        int>({
    // Cookies was previously logged to bucket 0, which is not a valid bucket
    // for linear histograms.
    {ContentSettingsType::COOKIES, 100},
    {ContentSettingsType::IMAGES, 1},
    {ContentSettingsType::JAVASCRIPT, 2},
    // Removed PLUGINS in M91.
    {ContentSettingsType::POPUPS, 4},
    {ContentSettingsType::GEOLOCATION, 5},
    {ContentSettingsType::NOTIFICATIONS, 6},
    {ContentSettingsType::AUTO_SELECT_CERTIFICATE, 7},
    {ContentSettingsType::MIXEDSCRIPT, 10},
    {ContentSettingsType::MEDIASTREAM_MIC, 12},
    {ContentSettingsType::MEDIASTREAM_CAMERA, 13},
    {ContentSettingsType::PROTOCOL_HANDLERS, 14},
    // PPAPI_BROKER is deprecated and shouldn't get logged anymore.
    {ContentSettingsType::DEPRECATED_PPAPI_BROKER, -1},
    {ContentSettingsType::AUTOMATIC_DOWNLOADS, 16},
    {ContentSettingsType::MIDI_SYSEX, 17},
    {ContentSettingsType::SSL_CERT_DECISIONS, 19},
    {ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER, 21},
    {ContentSettingsType::APP_BANNER, 22},
    {ContentSettingsType::SITE_ENGAGEMENT, 23},
    {ContentSettingsType::DURABLE_STORAGE, 24},
    // Removed "Key generation setting"
    {ContentSettingsType::BLUETOOTH_GUARD, 26},
    {ContentSettingsType::BACKGROUND_SYNC, 27},
    {ContentSettingsType::AUTOPLAY, 28},
    {ContentSettingsType::IMPORTANT_SITE_INFO, 30},
    {ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA, 31},
    {ContentSettingsType::ADS, 32},
    {ContentSettingsType::ADS_DATA, 33},
    {ContentSettingsType::PASSWORD_PROTECTION, 34},
    {ContentSettingsType::MEDIA_ENGAGEMENT, 35},
    {ContentSettingsType::SOUND, 36},
    {ContentSettingsType::CLIENT_HINTS, 37},
    {ContentSettingsType::SENSORS, 38},
    {ContentSettingsType::ACCESSIBILITY_EVENTS, 39},
    {ContentSettingsType::PAYMENT_HANDLER, 43},
    {ContentSettingsType::USB_GUARD, 44},
    {ContentSettingsType::BACKGROUND_FETCH, 45},
    {ContentSettingsType::INTENT_PICKER_DISPLAY, 46},
    {ContentSettingsType::IDLE_DETECTION, 47},
    {ContentSettingsType::SERIAL_GUARD, 48},
    {ContentSettingsType::SERIAL_CHOOSER_DATA, 49},
    {ContentSettingsType::PERIODIC_BACKGROUND_SYNC, 50},
    {ContentSettingsType::BLUETOOTH_SCANNING, 51},
    {ContentSettingsType::HID_GUARD, 52},
    {ContentSettingsType::HID_CHOOSER_DATA, 53},
    {ContentSettingsType::WAKE_LOCK_SCREEN, 54},
    {ContentSettingsType::WAKE_LOCK_SYSTEM, 55},
    {ContentSettingsType::LEGACY_COOKIE_ACCESS, 56},
    {ContentSettingsType::FILE_SYSTEM_WRITE_GUARD, 57},
    // Removed INSTALLED_WEB_APP_METADATA in M107.
    {ContentSettingsType::NFC, 59},
    {ContentSettingsType::BLUETOOTH_CHOOSER_DATA, 60},
    {ContentSettingsType::CLIPBOARD_READ_WRITE, 61},
    {ContentSettingsType::CLIPBOARD_SANITIZED_WRITE, 62},
    {ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA, 63},
    {ContentSettingsType::VR, 64},
    {ContentSettingsType::AR, 65},
    {ContentSettingsType::FILE_SYSTEM_READ_GUARD, 66},
    {ContentSettingsType::STORAGE_ACCESS, 67},
    {ContentSettingsType::CAMERA_PAN_TILT_ZOOM, 68},
    {ContentSettingsType::WINDOW_MANAGEMENT, 69},
    {ContentSettingsType::INSECURE_PRIVATE_NETWORK, 70},
    {ContentSettingsType::LOCAL_FONTS, 71},
    {ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA, 72},
    {ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY, 73},
    {ContentSettingsType::DISPLAY_CAPTURE, 74},
    // Removed FILE_HANDLING in M98.
    {ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA, 76},
    {ContentSettingsType::FEDERATED_IDENTITY_SHARING, 77},
    // Removed FEDERATED_IDENTITY_REQUEST in M103.
    {ContentSettingsType::JAVASCRIPT_JIT, 79},
    {ContentSettingsType::HTTP_ALLOWED, 80},
    {ContentSettingsType::FORMFILL_METADATA, 81},
    {ContentSettingsType::FEDERATED_IDENTITY_ACTIVE_SESSION, 82},
    {ContentSettingsType::AUTO_DARK_WEB_CONTENT, 83},
    {ContentSettingsType::REQUEST_DESKTOP_SITE, 84},
    {ContentSettingsType::FEDERATED_IDENTITY_API, 85},
    {ContentSettingsType::NOTIFICATION_INTERACTIONS, 86},
    {ContentSettingsType::REDUCED_ACCEPT_LANGUAGE, 87},
    {ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, 88},
    {ContentSettingsType::PRIVATE_NETWORK_GUARD, 89},
    {ContentSettingsType::PRIVATE_NETWORK_CHOOSER_DATA, 90},
    {ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS,
     91},
    {ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, 92},
    {ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, 93},
    {ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION, 94},
    {ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION,
     95},
    {ContentSettingsType::ANTI_ABUSE, 96},
    {ContentSettingsType::THIRD_PARTY_STORAGE_PARTITIONING, 97},
    {ContentSettingsType::HTTPS_ENFORCED, 98},
    {ContentSettingsType::USB_CHOOSER_DATA, 99},
    // The value 100 is assigned to COOKIES!
    // Removed GET_DISPLAY_MEDIA_SET_SELECT_ALL_SCREENS in M116.
    {ContentSettingsType::MIDI, 102},
    {ContentSettingsType::ALL_SCREEN_CAPTURE, 103},
    {ContentSettingsType::COOKIE_CONTROLS_METADATA, 104},
    {ContentSettingsType::TPCD_SUPPORT, 105},
    {ContentSettingsType::AUTO_PICTURE_IN_PICTURE, 106},
    {ContentSettingsType::TPCD_METADATA_GRANTS, 107},
    {ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION, 108},
    {ContentSettingsType::TPCD_HEURISTICS_GRANTS, 109},

    // As mentioned at the top, please don't forget to update ContentType in
    // enums.xml when you add entries here!
});

constexpr int kkHistogramValueMax =
    std::max_element(
        kHistogramValue.begin(),
        kHistogramValue.end(),
        [](const auto a, const auto b) { return a.second < b.second; })
        ->second;

std::string GetProviderNameForHistograms(
    HostContentSettingsMap::ProviderType provider_type) {
  switch (provider_type) {
    // Update the `ContentAllProviderTypes` variants in
    // https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/metadata/content/histograms.xml
    // when new providers are added.
    case HostContentSettingsMap::WEBUI_ALLOWLIST_PROVIDER:
      return "WebuiAllowlistProvider";
    case HostContentSettingsMap::POLICY_PROVIDER:
      return "PolicyProvider";
    case HostContentSettingsMap::SUPERVISED_PROVIDER:
      return "SupervisedProvider";
    case HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER:
      return "CustomExtensionProvider";
    case HostContentSettingsMap::INSTALLED_WEBAPP_PROVIDER:
      return "InstalledWebappProvider";
    case HostContentSettingsMap::NOTIFICATION_ANDROID_PROVIDER:
      return "NotificationAndroidProvider";
    case HostContentSettingsMap::ONE_TIME_PERMISSION_PROVIDER:
      return "OneTimePermissionProvider";
    case HostContentSettingsMap::PREF_PROVIDER:
      return "PrefProvider";
    case HostContentSettingsMap::DEFAULT_PROVIDER:
      return "DefaultProvider";
    case HostContentSettingsMap::PROVIDER_FOR_TESTS:
      return "ProviderForTests";
    case HostContentSettingsMap::OTHER_PROVIDER_FOR_TESTS:
      return "OtherProviderForTests";
    case HostContentSettingsMap::NUM_PROVIDER_TYPES:
      NOTREACHED();
      return "Unknown";
  }
}

}  // namespace

namespace content_settings_uma_util {

void RecordContentSettingsHistogram(const std::string& name,
                                    ContentSettingsType content_setting) {
  base::UmaHistogramExactLinear(
      name, ContentSettingTypeToHistogramValue(content_setting),
      kkHistogramValueMax + 1);
}

int ContentSettingTypeToHistogramValue(ContentSettingsType content_setting) {
  static_assert(kHistogramValue.size() ==
                    static_cast<size_t>(ContentSettingsType::NUM_TYPES),
                "Update content settings histogram lookup");

  auto* found = kHistogramValue.find(content_setting);
  if (found != kHistogramValue.end()) {
    DCHECK_NE(found->second, -1)
        << "Used for deprecated settings: " << static_cast<int>(found->first);
    return found->second;
  }
  NOTREACHED();
  return -1;
}

void RecordActiveExpiryEvent(HostContentSettingsMap::ProviderType provider_type,
                             ContentSettingsType content_setting_type) {
  content_settings_uma_util::RecordContentSettingsHistogram(
      base::StrCat({"ContentSettings.ActiveExpiry.",
                    GetProviderNameForHistograms(provider_type),
                    ".ContentSettingsType"}),
      content_setting_type);
}

}  // namespace content_settings_uma_util
