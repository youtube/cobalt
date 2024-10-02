// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/fast_checkout/fast_checkout_view_impl.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/android/fast_checkout/jni_headers/FastCheckoutBridge_jni.h"
#include "chrome/browser/ui/android/fast_checkout/ui_view_android_utils.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;

FastCheckoutViewImpl::FastCheckoutViewImpl(
    base::WeakPtr<FastCheckoutController> controller)
    : controller_(controller) {}

FastCheckoutViewImpl::~FastCheckoutViewImpl() {
  if (java_object_internal_) {
    // Don't create an object just for destruction.
    Java_FastCheckoutBridge_destroy(AttachCurrentThread(),
                                    java_object_internal_);
  }
}

void FastCheckoutViewImpl::OnOptionsSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& autofill_profile_java,
    const base::android::JavaParamRef<jobject>& credit_card_java) {
  std::unique_ptr<autofill::AutofillProfile> autofill_profile =
      CreateFastCheckoutAutofillProfileFromJava(
          env, autofill_profile_java,
          g_browser_process->GetApplicationLocale());

  std::unique_ptr<autofill::CreditCard> credit_card =
      CreateFastCheckoutCreditCardFromJava(env, credit_card_java);

  controller_->OnOptionsSelected(std::move(autofill_profile),
                                 std::move(credit_card));
}

void FastCheckoutViewImpl::OnDismiss(JNIEnv* env) {
  controller_->OnDismiss();
}

void FastCheckoutViewImpl::Show(
    const std::vector<autofill::AutofillProfile*>& autofill_profiles,
    const std::vector<autofill::CreditCard*>& credit_cards) {
  if (!RecreateJavaObjectIfNecessary()) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a waiting
    // state so report that TouchToFill is dismissed in order to show the normal
    // Android keyboard (plus keyboard accessory) instead.
    controller_->OnDismiss();
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> autofill_profiles_array =
      Java_FastCheckoutBridge_createAutofillProfilesArray(
          env, autofill_profiles.size());
  for (size_t i = 0; i < autofill_profiles.size(); ++i) {
    Java_FastCheckoutBridge_setAutofillProfile(
        env, autofill_profiles_array, i,
        CreateFastCheckoutAutofillProfile(
            env, *autofill_profiles[i],
            g_browser_process->GetApplicationLocale()));
  }

  base::android::ScopedJavaLocalRef<jobjectArray> credit_cards_array =
      Java_FastCheckoutBridge_createCreditCardsArray(env, credit_cards.size());
  for (size_t i = 0; i < credit_cards.size(); ++i) {
    Java_FastCheckoutBridge_setCreditCard(
        env, credit_cards_array, i,
        CreateFastCheckoutCreditCard(
            env, *credit_cards[i], g_browser_process->GetApplicationLocale()));
  }

  Java_FastCheckoutBridge_showBottomSheet(
      env, java_object_internal_, autofill_profiles_array, credit_cards_array);
}

void FastCheckoutViewImpl::OpenAutofillProfileSettings(JNIEnv* env) {
  controller_->OpenAutofillProfileSettings();
}

void FastCheckoutViewImpl::OpenCreditCardSettings(JNIEnv* env) {
  controller_->OpenCreditCardSettings();
}

bool FastCheckoutViewImpl::RecreateJavaObjectIfNecessary() {
  if (controller_->GetNativeView() == nullptr ||
      controller_->GetNativeView()->GetWindowAndroid() == nullptr) {
    return false;  // No window attached (yet or anymore).
  }

  if (java_object_internal_) {
    return true;
  }

  java_object_internal_ = Java_FastCheckoutBridge_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      controller_->GetNativeView()->GetWindowAndroid()->GetJavaObject());
  return !!java_object_internal_;
}

// static
std::unique_ptr<FastCheckoutView> FastCheckoutView::Create(
    base::WeakPtr<FastCheckoutController> controller) {
  return std::make_unique<FastCheckoutViewImpl>(controller);
}
