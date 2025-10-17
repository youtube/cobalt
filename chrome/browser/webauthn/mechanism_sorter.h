// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_MECHANISM_SORTER_H_
#define CHROME_BROWSER_WEBAUTHN_MECHANISM_SORTER_H_

#include <vector>

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

// Class to handle deduplication and sorting of WebAuthn mechanisms,
// with specific logic for the kModalImmediate UI presentation.
class MechanismSorter {
 public:
  // Processes a list of mechanisms by deduplicating and sorting them.
  // - Deduplication: For mechanisms with the same `mechanism.name`,
  //   selects the "best" one based on rules (e.g., recency).
  // - Sorting: Orders the resulting mechanisms based on preference rules
  //   (e.g. recency, passkeys preferred, alphabetical).
  //
  // This logic is applied when ui_presentation is
  // `UIPresentation::kModalImmediate`. For other presentations, it may apply a
  // default behavior or pass through the mechanisms unmodified.
  static std::vector<AuthenticatorRequestDialogModel::Mechanism>
  ProcessMechanisms(
      std::vector<AuthenticatorRequestDialogModel::Mechanism> mechanisms,
      UIPresentation ui_presentation);
};

#endif  // CHROME_BROWSER_WEBAUTHN_MECHANISM_SORTER_H_
