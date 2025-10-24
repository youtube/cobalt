// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_UI_CONSTANTS_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_UI_CONSTANTS_H_

#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"

namespace autofill {

inline constexpr int kMigrationDialogMainContainerChildSpacing = 24;
inline constexpr auto kMigrationDialogInsets = gfx::Insets::TLBR(0, 24, 48, 24);

// The time span an Autofill card bubble should be visible even if the document
// navigates away meanwhile. This is to ensure that the user has time to see
// the bubble.
inline constexpr base::TimeDelta kAutofillBubbleSurviveNavigationTime =
    base::Seconds(5);

// The delay before dismissing the progress dialog after the progress throbber
// shows the checkmark. This delay is for users to identify the status change.
inline constexpr base::TimeDelta kDelayBeforeDismissingProgressDialog =
    base::Seconds(1);

// The duration that the "Get New Code" link is disabled after it is clicked in
// Card Unmask OTP Input Dialog.
inline constexpr base::TimeDelta kNewOtpCodeLinkDisabledDuration =
    base::Seconds(5);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_UI_CONSTANTS_H_
