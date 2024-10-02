// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_UTIL_H_

#include <string>

#include "base/strings/string_piece.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/capture_links.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom.h"

namespace blink {

// Checks whether the manifest has no fields set.
BLINK_COMMON_EXPORT bool IsEmptyManifest(const mojom::Manifest& manifest);
BLINK_COMMON_EXPORT bool IsEmptyManifest(const mojom::ManifestPtr& manifest);

// Converts a blink::mojom::DisplayMode to a string. Returns one of
// https://www.w3.org/TR/appmanifest/#dfn-display-modes-values. Return values
// are lowercase. Returns an empty string for DisplayMode::kUndefined.
BLINK_COMMON_EXPORT std::string DisplayModeToString(
    blink::mojom::DisplayMode display);

// Returns the blink::mojom::DisplayMode which matches |display|.
// |display| should be one of
// https://www.w3.org/TR/appmanifest/#dfn-display-modes-values. |display| is
// case insensitive. Returns DisplayMode::kUndefined if there is no
// match.
BLINK_COMMON_EXPORT blink::mojom::DisplayMode DisplayModeFromString(
    const std::string& display);

// Returns true when 'display' is one of
// https://www.w3.org/TR/appmanifest/#dfn-display-modes-values. This
// differentiates the basic display modes from enhanced display modes that can
// be declared in the display_overrides member of the manifest.
BLINK_COMMON_EXPORT bool IsBasicDisplayMode(blink::mojom::DisplayMode display);

// Converts a device::mojom::ScreenOrientationLockType to a string. Returns one
// of https://www.w3.org/TR/screen-orientation/#orientationlocktype-enum. Return
// values are lowercase. Returns an empty string for
// device::mojom::ScreenOrientationLockType::DEFAULT.
BLINK_COMMON_EXPORT std::string WebScreenOrientationLockTypeToString(
    device::mojom::ScreenOrientationLockType);

// Returns the device::mojom::ScreenOrientationLockType which matches
// |orientation|. |orientation| should be one of
// https://www.w3.org/TR/screen-orientation/#orientationlocktype-enum.
// |orientation| is case insensitive. Returns
// device::mojom::ScreenOrientationLockType::DEFAULT if there is no match.
BLINK_COMMON_EXPORT device::mojom::ScreenOrientationLockType
WebScreenOrientationLockTypeFromString(const std::string& orientation);

BLINK_COMMON_EXPORT mojom::CaptureLinks CaptureLinksFromString(
    const std::string& capture_links);

BLINK_COMMON_EXPORT absl::optional<mojom::ManifestLaunchHandler::ClientMode>
ClientModeFromString(const std::string& client_mode);

// Returns the URL format manifest ID from the relative id.
BLINK_COMMON_EXPORT GURL GetIdFromManifest(const mojom::Manifest& manifest);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_UTIL_H_
