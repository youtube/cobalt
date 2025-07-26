// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_TOS_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_TOS_DIALOG_H_

#include "ui/views/window/dialog_delegate.h"

namespace autofill {

class BnplTosController;

// The dialog delegate view implementation for the Buy-Now-Pay-Later Terms of
// Service view. This is owned by the view hierarchy.
class BnplTosDialog : public views::DialogDelegateView {
  METADATA_HEADER(BnplTosDialog, views::DialogDelegateView)

 public:
  explicit BnplTosDialog(base::WeakPtr<BnplTosController> controller);
  BnplTosDialog(const BnplTosDialog&) = delete;
  BnplTosDialog& operator=(const BnplTosDialog&) = delete;
  ~BnplTosDialog() override;

  BnplTosController* controller() const;

 private:
  base::WeakPtr<BnplTosController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_TOS_DIALOG_H_
