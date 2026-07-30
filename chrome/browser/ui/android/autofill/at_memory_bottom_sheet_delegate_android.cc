// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_delegate_android.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"

namespace autofill {

AtMemoryBottomSheetDelegateAndroid::AtMemoryBottomSheetDelegateAndroid(
    AutofillClient* client,
    base::WeakPtr<AutofillSuggestionDelegate> delegate)
    : client_(&CHECK_DEREF(client)), delegate_(delegate) {}

AtMemoryBottomSheetDelegateAndroid::~AtMemoryBottomSheetDelegateAndroid() =
    default;

void AtMemoryBottomSheetDelegateAndroid::OnDismissed() {
  if (client_) {
    client_->HideSuggestions(SuggestionHidingReason::kUserAborted,
                             FillingProduct::kAtMemory);
  }
}

void AtMemoryBottomSheetDelegateAndroid::OnQuerySubmitted(
    const std::u16string& query) {
  if (delegate_) {
    delegate_->OnSearchSubmitted(query);
  }
}

}  // namespace autofill
