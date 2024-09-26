// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_TYPES_H_

namespace autofill {

// The list of all Autofill popup types that can be presented to the user.
enum class PopupType {
  kUnspecified,
  // Address form, but no address-related field is present. For example, it's
  // a sign-up page in which the user only enters the name and the email.
  kPersonalInformation,
  // Address form with address-related fields.
  kAddresses,
  kCreditCards,
  kIbans,
  kPasswords,
};

// This reason is passed whenever a popup needs to be closed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PopupHidingReason {
  // A suggestion was accepted.
  kAcceptSuggestion = 0,
  // An interstitial page displaces the popup.
  kAttachInterstitialPage = 1,
  // A field isn't edited anymore but remains focused for now.
  kEndEditing = 2,
  // Focus removed from field. Follows kEndEditing.
  kFocusChanged = 3,
  // Scrolling or zooming into the page displaces the popup.
  kContentAreaMoved = 4,
  // A navigation on the page or frame level.
  kNavigation = 5,
  // The popup is or would become empty.
  kNoSuggestions = 6,
  // The renderer explicitly requested closing the popup.
  kRendererEvent = 7,
  // The tab with the popup is destroyed, hidden or has become inactive.
  kTabGone = 8,
  // The popup contains stale data.
  kStaleData = 9,
  // The user explicitly dismissed the popup (e.g. ESC key).
  kUserAborted = 10,
  // The popup view (or its controller) goes out of scope.
  kViewDestroyed = 11,
  // The platform-native UI changed (e.g. window resize).
  kWidgetChanged = 12,
  // Not enough space in content area to display an display at least one row of
  // the popup within the bounds of the content area.
  kInsufficientSpace = 13,
  // The popup would overlap with another open prompt, and may hide sensitive
  // information in the prompt.
  kOverlappingWithAnotherPrompt = 14,
  // The anchor element for which the popup would be shown is not visible in the
  // content area.
  kElementOutsideOfContentArea = 15,
  // The frame holds a pointer lock.
  kMouseLocked = 16,
  // The password generation popup would overlap and hide autofill popup.
  kOverlappingWithPasswordGenerationPopup = 17,
  // The Touch To Fill surface is shown instead of autofill suggestions.
  kOverlappingWithTouchToFillSurface = 18,
  // The context menu is shown instead of the autofill suggestions.
  kOverlappingWithAutofillContextMenu = 19,
  // The Fast Checkout surface is shown instead of autofill suggestions.
  kOverlappingWithFastCheckoutSurface = 20,
  // The picture-in-picture window overlaps with the autofill suggestions.
  kOverlappingWithPictureInPictureWindow = 21,
  kMaxValue = kOverlappingWithPictureInPictureWindow
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_TYPES_H_
