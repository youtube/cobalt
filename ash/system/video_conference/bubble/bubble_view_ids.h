// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_IDS_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_IDS_H_

namespace ash::video_conference {

// IDs used for the main views that comprise the video conference bubble view.
// Use these for easy access to the views during the unittests. Note that these
// IDs are only guaranteed to be unique inside the bubble view.
// TODO(b/273786662): `kToggleEffectsButton` belongs to multiple buttons, and
// one id should only belong to one view, so we should remove it.
enum BubbleViewID {
  // Main outermost bubble view, what's actually launched from the tray. Start
  // from 1 because 0 is the default view ID.
  kMainBubbleView = 1,

  // The "return to app" UI, which is a child of `kMainBubbleView`.
  kReturnToApp,

  // Container view for all "set-value" VC effects, a child of
  // `kMainBubbleView`.
  kSetValueEffectsView,

  // Container view for a single "set-value" VC effect, a child of
  // `kSetValueEffectsView`.
  kSingleSetValueEffectView,

  // Container view for all "toggle" VC effects, a child of `kMainBubbleView`.
  kToggleEffectsView,

  // Button for toggling an individual "toggle" VC effect, a child of
  // `kToggleEffectsContainer`.
  kToggleEffectsButton,

  // Icon which is a child of an individual "toggle" VC effect.
  kToggleEffectIcon,

  // Label which is a child of an individual "toggle" VC effect.
  kToggleEffectLabel,

  // The label with a warning icon, indicating that effects are not available
  // for linux apps.
  kLinuxAppWarningView,

  // The VC bubble that will be displayed when all the running media apps are
  // Linux apps.
  kLinuxAppBubbleView,

  kNextAvailableId = kLinuxAppBubbleView + 1,
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_IDS_H_
