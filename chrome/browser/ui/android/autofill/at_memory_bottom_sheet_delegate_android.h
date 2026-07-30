// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AT_MEMORY_BOTTOM_SHEET_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AT_MEMORY_BOTTOM_SHEET_DELEGATE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/android/at_memory_bottom_sheet_delegate.h"

namespace autofill {

class AutofillClient;
class AutofillSuggestionDelegate;

// Concrete implementation of AtMemoryBottomSheetDelegate for Android.
// It handles events from the bridge and interacts with the client.
class AtMemoryBottomSheetDelegateAndroid : public AtMemoryBottomSheetDelegate {
 public:
  AtMemoryBottomSheetDelegateAndroid(
      AutofillClient* client,
      base::WeakPtr<AutofillSuggestionDelegate> delegate);
  ~AtMemoryBottomSheetDelegateAndroid() override;

  AtMemoryBottomSheetDelegateAndroid(
      const AtMemoryBottomSheetDelegateAndroid&) = delete;
  AtMemoryBottomSheetDelegateAndroid& operator=(
      const AtMemoryBottomSheetDelegateAndroid&) = delete;

  // AtMemoryBottomSheetDelegate:
  void OnDismissed() override;
  void OnQuerySubmitted(const std::u16string& query) override;

 private:
  raw_ptr<AutofillClient> client_;
  base::WeakPtr<AutofillSuggestionDelegate> delegate_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AT_MEMORY_BOTTOM_SHEET_DELEGATE_ANDROID_H_
