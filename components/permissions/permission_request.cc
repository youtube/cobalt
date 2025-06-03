// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request.h"

#include <string>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

PermissionRequest::PermissionRequest(
    const GURL& requesting_origin,
    RequestType request_type,
    bool has_gesture,
    PermissionDecidedCallback permission_decided_callback,
    base::OnceClosure delete_callback)
    : data_(
          PermissionRequestData(request_type, has_gesture, requesting_origin)),
      permission_decided_callback_(std::move(permission_decided_callback)),
      delete_callback_(std::move(delete_callback)) {}

PermissionRequest::PermissionRequest(
    PermissionRequestData request_data,
    PermissionDecidedCallback permission_decided_callback,
    base::OnceClosure delete_callback)
    : data_(std::move(request_data)),
      permission_decided_callback_(std::move(permission_decided_callback)),
      delete_callback_(std::move(delete_callback)) {}

PermissionRequest::~PermissionRequest() {
  DCHECK(delete_callback_.is_null());
}

RequestType PermissionRequest::request_type() const {
  CHECK(data_.request_type);
  return data_.request_type.value();
}

bool PermissionRequest::IsDuplicateOf(PermissionRequest* other_request) const {
  return request_type() == other_request->request_type() &&
         requesting_origin() == other_request->requesting_origin();
}

base::WeakPtr<PermissionRequest> PermissionRequest::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

#if BUILDFLAG(IS_ANDROID)
std::u16string PermissionRequest::GetDialogMessageText() const {
  int message_id = 0;
  switch (request_type()) {
    case RequestType::kAccessibilityEvents:
      message_id = IDS_ACCESSIBILITY_EVENTS_INFOBAR_TEXT;
      break;
    case RequestType::kArSession:
      message_id = IDS_AR_INFOBAR_TEXT;
      break;
    case RequestType::kCameraStream:
      message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_INFOBAR_TEXT;
      break;
    case RequestType::kClipboard:
      message_id = IDS_CLIPBOARD_INFOBAR_TEXT;
      break;
    case RequestType::kDiskQuota:
      // Handled by an override in `QuotaPermissionRequest`.
      NOTREACHED();
      break;
    case RequestType::kGeolocation:
      message_id = IDS_GEOLOCATION_INFOBAR_TEXT;
      break;
    case RequestType::kIdleDetection:
      message_id = IDS_IDLE_DETECTION_INFOBAR_TEXT;
      break;
    case RequestType::kMicStream:
      message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_INFOBAR_TEXT;
      break;
    case RequestType::kMidi:
      message_id = IDS_MIDI_INFOBAR_TEXT;
      break;
    case RequestType::kMidiSysex:
      message_id = IDS_MIDI_SYSEX_INFOBAR_TEXT;
      break;
    case RequestType::kMultipleDownloads:
      message_id = IDS_MULTI_DOWNLOAD_WARNING;
      break;
    case RequestType::kNfcDevice:
      message_id = IDS_NFC_INFOBAR_TEXT;
      break;
    case RequestType::kNotifications:
      message_id = IDS_NOTIFICATIONS_INFOBAR_TEXT;
      break;
    case RequestType::kProtectedMediaIdentifier:
      message_id =
          IDS_PROTECTED_MEDIA_IDENTIFIER_PER_ORIGIN_PROVISIONING_INFOBAR_TEXT;
      break;
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      // Handled by `PermissionPromptAndroid::GetMessageText` directly.
      NOTREACHED();
      break;
    case RequestType::kVrSession:
      message_id = IDS_VR_INFOBAR_TEXT;
      break;
  }
  DCHECK_NE(0, message_id);
  return l10n_util::GetStringFUTF16(
      message_id, url_formatter::FormatUrlForSecurityDisplay(
                      requesting_origin(),
                      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}
#endif

bool PermissionRequest::IsEmbeddedPermissionElementInitiated() const {
  return data_.embedded_permission_element_initiated;
}

#if !BUILDFLAG(IS_ANDROID)

bool PermissionRequest::IsConfirmationChipSupported() {
  return permissions::IsConfirmationChipSupported(request_type());
}

IconId PermissionRequest::GetIconForChip() {
  return permissions::GetIconId(request_type());
}

IconId PermissionRequest::GetBlockedIconForChip() {
  return permissions::GetBlockedIconId(request_type());
}

absl::optional<std::u16string> PermissionRequest::GetRequestChipText(
    ChipTextType type) const {
  static base::NoDestructor<std::map<RequestType, std::vector<int>>> kMessageIds(
      {{RequestType::kArSession,
        {IDS_AR_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kCameraStream,
        {IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_CAMERA_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_CAMERA_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_CAMERA_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kClipboard,
        {IDS_CLIPBOARD_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kGeolocation,
        {IDS_GEOLOCATION_PERMISSION_CHIP,
         IDS_GEOLOCATION_PERMISSION_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_GEOLOCATION_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_GEOLOCATION_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kIdleDetection,
        {IDS_IDLE_DETECTION_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kMicStream,
        {IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_MICROPHONE_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_MICROPHONE_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_MICROPHONE_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kMidi,
        {IDS_MIDI_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kMidiSysex,
        {IDS_MIDI_SYSEX_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kNotifications,
        {IDS_NOTIFICATION_PERMISSIONS_CHIP,
         IDS_NOTIFICATION_PERMISSIONS_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION, -1,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_NOTIFICATION_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         -1,
         IDS_PERMISSIONS_NOTIFICATION_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kStorageAccess,
        {IDS_SAA_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION, -1,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_SAA_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT, -1,
         IDS_PERMISSIONS_SAA_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kVrSession,
        {IDS_VR_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}}});

  auto messages = kMessageIds->find(request_type());
  if (messages != kMessageIds->end() && messages->second[type] != -1)
    return l10n_util::GetStringUTF16(messages->second[type]);

  return absl::nullopt;
}

std::u16string PermissionRequest::GetMessageTextFragment() const {
  int message_id = 0;
  switch (request_type()) {
    case RequestType::kAccessibilityEvents:
      message_id = IDS_ACCESSIBILITY_EVENTS_PERMISSION_FRAGMENT;
      break;
    case RequestType::kArSession:
      message_id = IDS_AR_PERMISSION_FRAGMENT;
      break;
    case RequestType::kCameraPanTiltZoom:
      message_id = IDS_MEDIA_CAPTURE_CAMERA_PAN_TILT_ZOOM_PERMISSION_FRAGMENT;
      break;
    case RequestType::kCameraStream:
      message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT;
      break;
    case RequestType::kClipboard:
      message_id = IDS_CLIPBOARD_PERMISSION_FRAGMENT;
      break;
    case RequestType::kDiskQuota:
      message_id = IDS_REQUEST_QUOTA_PERMISSION_FRAGMENT;
      break;
    case RequestType::kFileSystemAccess:
      message_id = IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE;
      break;
    case RequestType::kGeolocation:
      message_id = IDS_GEOLOCATION_INFOBAR_PERMISSION_FRAGMENT;
      break;
    case RequestType::kIdleDetection:
      message_id = IDS_IDLE_DETECTION_PERMISSION_FRAGMENT;
      break;
    case RequestType::kLocalFonts:
      message_id = IDS_FONT_ACCESS_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMicStream:
      message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMidi:
      message_id = IDS_MIDI_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMidiSysex:
      message_id = IDS_MIDI_SYSEX_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMultipleDownloads:
      message_id = IDS_MULTI_DOWNLOAD_PERMISSION_FRAGMENT;
      break;
    case RequestType::kNotifications:
      message_id = IDS_NOTIFICATION_PERMISSIONS_FRAGMENT;
      break;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case RequestType::kProtectedMediaIdentifier:
      message_id = IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_FRAGMENT;
      break;
#endif
    case RequestType::kRegisterProtocolHandler:
      // Handled by an override in `RegisterProtocolHandlerPermissionRequest`.
      NOTREACHED();
      return std::u16string();
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      message_id = IDS_STORAGE_ACCESS_PERMISSION_FRAGMENT;
      break;
    case RequestType::kVrSession:
      message_id = IDS_VR_PERMISSION_FRAGMENT;
      break;
    case RequestType::kWindowManagement:
      message_id = IDS_WINDOW_MANAGEMENT_PERMISSION_FRAGMENT;
      break;
  }
  DCHECK_NE(0, message_id);
  return l10n_util::GetStringUTF16(message_id);
}
#endif

bool PermissionRequest::ShouldUseTwoOriginPrompt() const {
  return request_type() == RequestType::kStorageAccess &&
         base::FeatureList::IsEnabled(
             permissions::features::kPermissionStorageAccessAPI);
}

void PermissionRequest::PermissionGranted(bool is_one_time) {
  std::move(permission_decided_callback_)
      .Run(CONTENT_SETTING_ALLOW, is_one_time,
           /*is_final_decision=*/true);
}

void PermissionRequest::PermissionDenied() {
  std::move(permission_decided_callback_)
      .Run(CONTENT_SETTING_BLOCK, /*is_one_time=*/false,
           /*is_final_decision=*/true);
}

void PermissionRequest::Cancelled(bool is_final_decision) {
  permission_decided_callback_.Run(CONTENT_SETTING_DEFAULT,
                                   /*is_one_time=*/false, is_final_decision);
}

void PermissionRequest::RequestFinished() {
  std::move(delete_callback_).Run();
}

PermissionRequestGestureType PermissionRequest::GetGestureType() const {
  return PermissionUtil::GetGestureType(data_.user_gesture);
}

ContentSettingsType PermissionRequest::GetContentSettingsType() const {
  auto type = RequestTypeToContentSettingsType(request_type());
  if (type.has_value())
    return type.value();
  return ContentSettingsType::DEFAULT;
}

std::u16string PermissionRequest::GetPermissionNameTextFragment() const {
  int message_id = 0;
  switch (request_type()) {
    case RequestType::kCameraStream:
      message_id = IDS_CAMERA_PERMISSION_NAME_FRAGMENT;
      break;
    case RequestType::kMicStream:
      message_id = IDS_MICROPHONE_PERMISSION_NAME_FRAGMENT;
      break;
    default:
      NOTREACHED();
      return std::u16string();
  }
  DCHECK_NE(0, message_id);
  return l10n_util::GetStringUTF16(message_id);
}

void PermissionRequest::SetEmbeddedPermissionElementInitiatedForTesting(
    bool embedded_permission_element_initiated) {
  data_.embedded_permission_element_initiated =
      embedded_permission_element_initiated;
}

}  // namespace permissions
