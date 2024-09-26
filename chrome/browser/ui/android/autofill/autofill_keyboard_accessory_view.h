// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_

#include <jni.h>
#include <stddef.h>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_adapter.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"

namespace autofill {

class AutofillPopupController;

// A suggestion view that acts as an alternative to the field-attached popup
// window. This view appears above the keyboard and spans the width of the
// screen, condensing rather than overlaying the content area.
class AutofillKeyboardAccessoryView
    : public AutofillKeyboardAccessoryAdapter::AccessoryView {
 public:
  explicit AutofillKeyboardAccessoryView(
      base::WeakPtr<AutofillPopupController> controller);

  AutofillKeyboardAccessoryView(const AutofillKeyboardAccessoryView&) = delete;
  AutofillKeyboardAccessoryView& operator=(
      const AutofillKeyboardAccessoryView&) = delete;

  ~AutofillKeyboardAccessoryView() override;

  // Implementation of AutofillKeyboardAccessoryAdapter::AccessoryView.
  bool Initialize() override;
  void Hide() override;
  void Show() override;
  void AxAnnounce(const std::u16string& text) override;
  void ConfirmDeletion(const std::u16string& confirmation_title,
                       const std::u16string& confirmation_body,
                       base::OnceClosure confirm_deletion) override;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  // Called when an autofill item was selected.
  void SuggestionSelected(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint list_index);

  // Called when the deletion of an autofill item was requested.
  void DeletionRequested(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint list_index);

  // Called when the deletion of an autofill item was confirmed.
  void DeletionConfirmed(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  // Called when this view was dismissed.
  void ViewDismissed(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

 private:
  // Weak reference to owner of this class. Always outlives this view.
  base::WeakPtr<AutofillPopupController> controller_;

  // Call to confirm a requested deletion.
  base::OnceClosure confirm_deletion_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_
