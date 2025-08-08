// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_PERMISSION_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_PERMISSION_UTILS_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace blink {

// This enum is also used for UMA purposes, so it needs to adhere to
// the UMA guidelines.
// Make sure you update enums.xml and GetAllPermissionTypes if you add new
// or deprecate permission types.
// Never delete or reorder an entry; only add new entries
// immediately before PermissionType::NUM
enum class PermissionType {
  MIDI_SYSEX = 1,
  // PUSH_MESSAGING = 2,
  NOTIFICATIONS = 3,
  GEOLOCATION = 4,
  PROTECTED_MEDIA_IDENTIFIER = 5,
  MIDI = 6,
  DURABLE_STORAGE = 7,
  AUDIO_CAPTURE = 8,
  VIDEO_CAPTURE = 9,
  BACKGROUND_SYNC = 10,
  // FLASH = 11,
  SENSORS = 12,
  ACCESSIBILITY_EVENTS = 13,
  // CLIPBOARD_READ = 14, // Replaced by CLIPBOARD_READ_WRITE in M81.
  // CLIPBOARD_WRITE = 15, // Replaced by CLIPBOARD_SANITIZED_WRITE in M81.
  PAYMENT_HANDLER = 16,
  BACKGROUND_FETCH = 17,
  IDLE_DETECTION = 18,
  PERIODIC_BACKGROUND_SYNC = 19,
  WAKE_LOCK_SCREEN = 20,
  WAKE_LOCK_SYSTEM = 21,
  NFC = 22,
  CLIPBOARD_READ_WRITE = 23,
  CLIPBOARD_SANITIZED_WRITE = 24,
  VR = 25,
  AR = 26,
  STORAGE_ACCESS_GRANT = 27,
  CAMERA_PAN_TILT_ZOOM = 28,
  WINDOW_MANAGEMENT = 29,
  LOCAL_FONTS = 30,
  DISPLAY_CAPTURE = 31,
  // FILE_HANDLING = 32,  // Removed in M98.
  TOP_LEVEL_STORAGE_ACCESS = 33,

  // Always keep this at the end.
  NUM,
};

// Converts a permission string ("granted", "denied", "prompt") into a
// PermissionStatus.
BLINK_COMMON_EXPORT mojom::PermissionStatus ToPermissionStatus(
    const std::string& status);

// Converts `PermissionType` into a string.
BLINK_COMMON_EXPORT std::string GetPermissionString(PermissionType permission);

// Get a list of all permission types.
BLINK_COMMON_EXPORT const std::vector<PermissionType>& GetAllPermissionTypes();

// Given |descriptor|, set |permission_type| to a corresponding PermissionType.
BLINK_COMMON_EXPORT absl::optional<PermissionType>
PermissionDescriptorToPermissionType(
    const mojom::PermissionDescriptorPtr& descriptor);

// Ideally this would be an equivalent function to
// |PermissionDescriptorToPermissionType| but for a
// `mojom::blink::PermissionDescriptorPtr` descriptor. But unfortunately mojo
// blink headers depend on blink/common so we can't introduce the reverse
// dependency. Instead we provide this function that requires the relevant
// information for making the decision and the caller needs to extract it from
// the descriptor and provide it.
BLINK_COMMON_EXPORT absl::optional<PermissionType>
PermissionDescriptorInfoToPermissionType(mojom::PermissionName name,
                                         bool midi_sysex,
                                         bool camera_ptz,
                                         bool clipboard_will_be_sanitized,
                                         bool clipboard_has_user_gesture);

// Converts `permission` type into the corresponding permission policy feature.
// If there is no, returns nullopt.
BLINK_COMMON_EXPORT absl::optional<mojom::PermissionsPolicyFeature>
PermissionTypeToPermissionsPolicyFeature(PermissionType permission);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_PERMISSION_UTILS_H_
