// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_TYPE_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_TYPE_H_

namespace autofill {

// The type of Autofill snackbar to show.
enum class AutofillSnackbarType {
  // Unspecified snackbar type.
  kUnspecified = 0,

  // Used when virtual card is retrieved.
  kVirtualCard = 1,

  // Used when mandatory reauth opt-in is confirmed.
  kMandatoryReauth = 2
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_TYPE_H_
