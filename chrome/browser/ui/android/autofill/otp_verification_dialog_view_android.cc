// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/otp_verification_dialog_view_android.h"

#include <jni.h>
#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/autofill/internal/jni_headers/OtpVerificationDialogBridge_jni.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;

namespace autofill {

OtpVerificationDialogViewAndroid::OtpVerificationDialogViewAndroid(
    CardUnmaskOtpInputDialogController* controller)
    : controller_(controller) {
  DCHECK(controller);
}

OtpVerificationDialogViewAndroid::~OtpVerificationDialogViewAndroid() = default;

// static
CardUnmaskOtpInputDialogView* CardUnmaskOtpInputDialogView::CreateAndShow(
    CardUnmaskOtpInputDialogController* controller,
    content::WebContents* web_contents) {
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  if (!view_android) {
    return nullptr;
  }
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return nullptr;
  }
  std::unique_ptr<OtpVerificationDialogViewAndroid> dialog_view =
      std::make_unique<OtpVerificationDialogViewAndroid>(controller);
  // Return the dialog only if we were able to show it.
  return dialog_view->ShowDialog(window_android) ? dialog_view.release()
                                                 : nullptr;
}

void OtpVerificationDialogViewAndroid::ShowPendingState() {
  // For Android, the Java code takes care of showing the pending state after
  // user submits the OTP.
  NOTREACHED();
}

void OtpVerificationDialogViewAndroid::ShowInvalidState(
    const std::u16string& invalid_label_text) {
  DCHECK(java_object_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OtpVerificationDialogBridge_showOtpErrorMessage(
      env, java_object_, ConvertUTF16ToJavaString(env, invalid_label_text));
}

void OtpVerificationDialogViewAndroid::Dismiss(
    bool show_confirmation_before_closing,
    bool user_closed_dialog) {
  if (show_confirmation_before_closing) {
    DCHECK(!user_closed_dialog);
    DCHECK(controller_);
    std::u16string confirmation_message = controller_->GetConfirmationMessage();
    controller_->OnDialogClosed(/*user_closed_dialog=*/false,
                                /*server_request_succeeded=*/true);
    controller_ = nullptr;
    ShowConfirmationAndDismissDialog(confirmation_message);
    return;
  }

  if (controller_) {
    controller_->OnDialogClosed(
        user_closed_dialog,
        /*server_request_succeeded=*/show_confirmation_before_closing);
    controller_ = nullptr;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_OtpVerificationDialogBridge_dismissDialog(env, java_object_);
  } else {
    delete this;
  }
}

void OtpVerificationDialogViewAndroid::OnDialogDismissed(JNIEnv* env) {
  // Inform |controller_| of the dialog's destruction.
  if (controller_) {
    controller_->OnDialogClosed(/*user_closed_dialog=*/true,
                                /*server_request_succeeded=*/false);
    controller_ = nullptr;
  }
  delete this;
}

void OtpVerificationDialogViewAndroid::OnConfirm(
    JNIEnv* env,
    const JavaParamRef<jstring>& otp) {
  controller_->OnOkButtonClicked(ConvertJavaStringToUTF16(env, otp));
}

void OtpVerificationDialogViewAndroid::OnNewOtpRequested(JNIEnv* env) {
  controller_->OnNewCodeLinkClicked();
}

bool OtpVerificationDialogViewAndroid::ShowDialog(
    ui::WindowAndroid* window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_OtpVerificationDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));
  if (java_object_.is_null()) {
    return false;
  }
  Java_OtpVerificationDialogBridge_showDialog(
      env, java_object_, controller_->GetExpectedOtpLength());
  return true;
}

void OtpVerificationDialogViewAndroid::ShowConfirmationAndDismissDialog(
    std::u16string confirmation_message) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (java_object_) {
    Java_OtpVerificationDialogBridge_showConfirmationAndDismissDialog(
        env, java_object_, ConvertUTF16ToJavaString(env, confirmation_message));
  }
}

}  // namespace autofill
