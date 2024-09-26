// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_bubble_controller.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>
#include <memory>
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/autofill/virtual_card_enrollment_view_android.h"
#endif

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_H_

namespace autofill {

class VirtualCardEnrollBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public VirtualCardEnrollBubbleController,
      public content::WebContentsUserData<
          VirtualCardEnrollBubbleControllerImpl> {
 public:
  VirtualCardEnrollBubbleControllerImpl(
      const VirtualCardEnrollBubbleControllerImpl&) = delete;
  VirtualCardEnrollBubbleControllerImpl& operator=(
      const VirtualCardEnrollBubbleControllerImpl&) = delete;
  ~VirtualCardEnrollBubbleControllerImpl() override;

  // Displays both the virtual card enroll bubble and its associated omnibox
  // icon. Sets virtual card enrollment fields as well as the closure for the
  // accept and decline bubble events.
  void ShowBubble(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback);

  // Shows the bubble again if the users clicks the omnibox icon.
  void ReshowBubble();

  // VirtualCardEnrollBubbleController:
  std::u16string GetWindowTitle() const override;
  std::u16string GetExplanatoryMessage() const override;
  std::u16string GetAcceptButtonText() const override;
  std::u16string GetDeclineButtonText() const override;
  std::u16string GetLearnMoreLinkText() const override;
  const VirtualCardEnrollmentFields GetVirtualCardEnrollmentFields()
      const override;
  AutofillBubbleBase* GetVirtualCardEnrollBubbleView() const override;

#if !BUILDFLAG(IS_ANDROID)
  void HideIconAndBubble() override;
#endif

  void OnAcceptButton() override;
  void OnDeclineButton() override;
  void OnLinkClicked(VirtualCardEnrollmentLinkType link_type,
                     const GURL& url) override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;

#if BUILDFLAG(IS_ANDROID)
  void OnAccepted(JNIEnv* env);
  void OnDeclined(JNIEnv* env);
  void OnDismissed(JNIEnv* env);
  void OnLinkClicked(JNIEnv* env, jstring url, jint link_type);
#endif

  bool IsIconVisible() const override;

#if defined(UNIT_TEST)
  void SetBubbleShownClosureForTesting(
      base::RepeatingClosure bubble_shown_closure_for_testing) {
    bubble_shown_closure_for_testing_ = bubble_shown_closure_for_testing;
  }
#endif

 protected:
  explicit VirtualCardEnrollBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase::
  void OnVisibilityChanged(content::Visibility visibility) override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  // Gets the correct virtual card enrollment source metric to log.
  VirtualCardEnrollmentBubbleSource GetVirtualCardEnrollmentBubbleSource();

  friend class content::WebContentsUserData<
      VirtualCardEnrollBubbleControllerImpl>;

  // Contains more details regarding the sort of bubble to show the users.
  VirtualCardEnrollmentFields virtual_card_enrollment_fields_;

  // Whether we should re-show the dialog when users return to the tab.
  bool reprompt_required_ = false;

#if !BUILDFLAG(IS_ANDROID)
  // Returns whether the web content associated with this controller is active.
  virtual bool IsWebContentsActive();

  // Represents the current state of icon and bubble.
  BubbleState bubble_state_ = BubbleState::kHidden;
#endif

  // Denotes whether the bubble is shown due to user gesture. If this is true,
  // it means the bubble is a reshown bubble.
  bool is_user_gesture_ = false;

  // Closure invoked when the user agrees to enroll in a virtual card.
  base::OnceClosure accept_virtual_card_callback_;

  // Closure invoked when the user rejects enrolling in a virtual card.
  base::OnceClosure decline_virtual_card_callback_;

  // Closure used for testing purposes that notifies that the enrollment bubble
  // has been shown.
  base::RepeatingClosure bubble_shown_closure_for_testing_;

#if BUILDFLAG(IS_ANDROID)
  // VirtualCardEnrollBubbleController:
  base::android::ScopedJavaGlobalRef<jobject> GetOrCreateJavaDelegate()
      override;

  // The Android delegate that facilitates making native calls from Android
  // view.
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
#endif

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_H_
