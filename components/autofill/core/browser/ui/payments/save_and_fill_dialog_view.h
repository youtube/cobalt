// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_VIEW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

// Interface that exposes the view to SaveAndFillControllerImpl.
class SaveAndFillDialogView {
 public:
  virtual ~SaveAndFillDialogView() = default;

  virtual base::WeakPtr<SaveAndFillDialogView> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_VIEW_H_
