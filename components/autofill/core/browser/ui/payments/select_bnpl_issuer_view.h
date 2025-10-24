// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_VIEW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_VIEW_H_

namespace autofill::payments {

// The cross-platform UI interface which prompts the user to select a BNPL
// issuer from a list. This object is responsible for its own lifetime.
class SelectBnplIssuerView {
 public:
  virtual ~SelectBnplIssuerView() = default;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_VIEW_H_
