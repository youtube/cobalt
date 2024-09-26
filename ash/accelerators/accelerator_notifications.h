// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_NOTIFICATIONS_H_
#define ASH_ACCELERATORS_ACCELERATOR_NOTIFICATIONS_H_

#include "ash/ash_export.h"

namespace ash {

// Identifiers for toggling accessibility notifications.
ASH_EXPORT extern const char kDockedMagnifierToggleAccelNotificationId[];
ASH_EXPORT extern const char kFullscreenMagnifierToggleAccelNotificationId[];
ASH_EXPORT extern const char kHighContrastToggleAccelNotificationId[];

// URL for keyboard shortcut help.
ASH_EXPORT extern const char kKeyboardShortcutHelpPageUrl[];

// Shows a warning the user is using a deprecated accelerator.
ASH_EXPORT void ShowDeprecatedAcceleratorNotification(
    const char* notification_id,
    int message_id,
    int old_shortcut_id,
    int new_shortcut_id);

ASH_EXPORT void ShowDockedMagnifierNotification();

ASH_EXPORT void ShowDockedMagnifierDisabledByAdminNotification(
    bool feature_state);

ASH_EXPORT void RemoveDockedMagnifierNotification();

ASH_EXPORT void ShowFullscreenMagnifierNotification();

ASH_EXPORT void ShowFullscreenMagnifierDisabledByAdminNotification(
    bool feature_state);

ASH_EXPORT void RemoveFullscreenMagnifierNotification();

ASH_EXPORT void ShowHighContrastNotification();

ASH_EXPORT void ShowHighContrastDisabledByAdminNotification(bool feature_state);

ASH_EXPORT void RemoveHighContrastNotification();

ASH_EXPORT void ShowSpokenFeedbackDisabledByAdminNotification(
    bool feature_state);

ASH_EXPORT void RemoveSpokenFeedbackNotification();

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_NOTIFICATIONS_H_
