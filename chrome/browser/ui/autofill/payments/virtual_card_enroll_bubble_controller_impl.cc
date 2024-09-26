// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/android/chrome_jni_headers/VirtualCardEnrollmentDelegate_jni.h"

#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_mobile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/messages/android/messages_feature.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

VirtualCardEnrollBubbleControllerImpl::VirtualCardEnrollBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<VirtualCardEnrollBubbleControllerImpl>(
          *web_contents) {}

VirtualCardEnrollBubbleControllerImpl::
    ~VirtualCardEnrollBubbleControllerImpl() {
#if BUILDFLAG(IS_ANDROID)
  HideBubble();
  // Reset the controller reference in the Java delegate.
  if (java_delegate_) {
    Java_VirtualCardEnrollmentDelegate_onNativeDestroyed(
        base::android::AttachCurrentThread(), java_delegate_);
  }
#endif
}

// static
VirtualCardEnrollBubbleController*
VirtualCardEnrollBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents);
  return VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents);
}

void VirtualCardEnrollBubbleControllerImpl::ShowBubble(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  virtual_card_enrollment_fields_ = virtual_card_enrollment_fields;
  accept_virtual_card_callback_ = std::move(accept_virtual_card_callback);
  decline_virtual_card_callback_ = std::move(decline_virtual_card_callback);

  is_user_gesture_ = false;
  Show();

  LogVirtualCardEnrollBubbleCardArtAvailable(
      virtual_card_enrollment_fields_.card_art_image,
      virtual_card_enrollment_fields_.virtual_card_enrollment_source);
}

void VirtualCardEnrollBubbleControllerImpl::ReshowBubble() {
  DCHECK(IsIconVisible());

  if (bubble_view())
    return;

  is_user_gesture_ = true;
  Show();
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_TITLE_LABEL);
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetExplanatoryMessage()
    const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_CONTENT_LABEL,
      GetLearnMoreLinkText());
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetAcceptButtonText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_ACCEPT_BUTTON_LABEL);
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetDeclineButtonText()
    const {
  return l10n_util::GetStringUTF16(
      virtual_card_enrollment_fields_.virtual_card_enrollment_source ==
              VirtualCardEnrollmentSource::kSettingsPage
          ? IDS_CANCEL
          : virtual_card_enrollment_fields_.last_show
                ? IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_NO_THANKS
                : IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_SKIP);
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetLearnMoreLinkText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK_LABEL);
}

const VirtualCardEnrollmentFields
VirtualCardEnrollBubbleControllerImpl::GetVirtualCardEnrollmentFields() const {
  return virtual_card_enrollment_fields_;
}

AutofillBubbleBase*
VirtualCardEnrollBubbleControllerImpl::GetVirtualCardEnrollBubbleView() const {
  return bubble_view();
}

#if !BUILDFLAG(IS_ANDROID)
void VirtualCardEnrollBubbleControllerImpl::HideIconAndBubble() {
  HideBubble();
  bubble_state_ = BubbleState::kHidden;
  UpdatePageActionIcon();
}
#endif

void VirtualCardEnrollBubbleControllerImpl::OnAcceptButton() {
  std::move(accept_virtual_card_callback_).Run();
  decline_virtual_card_callback_.Reset();

#if !BUILDFLAG(IS_ANDROID)
  bubble_state_ = BubbleState::kHidden;
#endif
}

void VirtualCardEnrollBubbleControllerImpl::OnDeclineButton() {
  std::move(decline_virtual_card_callback_).Run();
  accept_virtual_card_callback_.Reset();

#if !BUILDFLAG(IS_ANDROID)
  bubble_state_ = BubbleState::kHidden;
#endif
}

void VirtualCardEnrollBubbleControllerImpl::OnLinkClicked(
    VirtualCardEnrollmentLinkType link_type,
    const GURL& url) {
  reprompt_required_ = true;

  LogVirtualCardEnrollmentLinkClickedMetric(
      link_type, GetVirtualCardEnrollmentBubbleSource());

  web_contents()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));

#if !BUILDFLAG(IS_ANDROID)
  bubble_state_ = BubbleState::kShowingIconAndBubble;
#endif
}

void VirtualCardEnrollBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();

  VirtualCardEnrollmentBubbleResult result;
  switch (closed_reason) {
    case PaymentsBubbleClosedReason::kAccepted:
      result = VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED;
      break;
    case PaymentsBubbleClosedReason::kClosed:
      result = VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED;
      break;
    case PaymentsBubbleClosedReason::kNotInteracted:
      result = VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED;
      break;
    case PaymentsBubbleClosedReason::kLostFocus:
      result = VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_LOST_FOCUS;
      break;
    case PaymentsBubbleClosedReason::kCancelled:
      result = VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED;
      break;
    case PaymentsBubbleClosedReason::kUnknown:
      NOTREACHED();
      result = VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_RESULT_UNKNOWN;
  }

  // If the dialog is to be shown again because user clicked on links, do not
  // log metrics.
  if (!reprompt_required_) {
    LogVirtualCardEnrollmentBubbleResultMetric(
        result, GetVirtualCardEnrollmentBubbleSource(), is_user_gesture_,
        virtual_card_enrollment_fields_.previously_declined);
  }
}

#if BUILDFLAG(IS_ANDROID)
void VirtualCardEnrollBubbleControllerImpl::OnAccepted(JNIEnv* env) {
  OnAcceptButton();
  OnBubbleClosed(PaymentsBubbleClosedReason::kAccepted);
}

void VirtualCardEnrollBubbleControllerImpl::OnDeclined(JNIEnv* env) {
  OnDeclineButton();
  OnBubbleClosed(PaymentsBubbleClosedReason::kCancelled);
}

void VirtualCardEnrollBubbleControllerImpl::OnDismissed(JNIEnv* env) {
  OnBubbleClosed(PaymentsBubbleClosedReason::kNotInteracted);
}

void VirtualCardEnrollBubbleControllerImpl::OnLinkClicked(JNIEnv* env,
                                                          jstring url,
                                                          jint link_type) {
  OnLinkClicked(static_cast<autofill::VirtualCardEnrollmentLinkType>(link_type),
                GURL(base::android::ConvertJavaStringToUTF16(env, url)));
}
#endif

bool VirtualCardEnrollBubbleControllerImpl::IsIconVisible() const {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return bubble_state_ != BubbleState::kHidden;
#endif
}

void VirtualCardEnrollBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
#if BUILDFLAG(IS_ANDROID)
  if (visibility == content::Visibility::VISIBLE && !bubble_view() &&
      reprompt_required_ && messages::IsSaveCardMessagesUiEnabled()) {
    Show();
  } else if (visibility == content::Visibility::HIDDEN) {
    HideBubble();
  }
#else
  if (visibility == content::Visibility::VISIBLE && !bubble_view() &&
      bubble_state_ == BubbleState::kShowingIconAndBubble) {
    Show();
  } else if (visibility == content::Visibility::HIDDEN) {
    HideBubble();
    if (bubble_state_ != BubbleState::kShowingIcon)
      bubble_state_ = BubbleState::kHidden;
  }
#endif
}

PageActionIconType
VirtualCardEnrollBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kVirtualCardEnroll;
}

void VirtualCardEnrollBubbleControllerImpl::DoShowBubble() {
#if BUILDFLAG(IS_ANDROID)
  if (messages::IsSaveCardMessagesUiEnabled() &&
      GetVirtualCardEnrollmentBubbleSource() ==
          VirtualCardEnrollmentBubbleSource::
              VIRTUAL_CARD_ENROLLMENT_UPSTREAM_SOURCE) {
    DCHECK(!bubble_view());

    set_bubble_view(
        VirtualCardEnrollmentViewAndroid::CreateAndShow(web_contents(), this));

    DCHECK(bubble_view());
  } else {
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents());
    DCHECK(infobar_manager);
    infobar_manager->RemoveAllInfoBars(true);
    infobar_manager->AddInfoBar(CreateVirtualCardEnrollmentInfoBarMobile(
        std::make_unique<AutofillVirtualCardEnrollmentInfoBarDelegateMobile>(
            this)));
  }
#else
  // If bubble is already showing for another card, close it.
  if (bubble_view())
    HideBubble();

  bubble_state_ = BubbleState::kShowingIconAndBubble;
  if (!IsWebContentsActive())
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  // For reprompts after link clicks, |is_user_gesture| is set to false.
  bool user_gesture_reprompt = reprompt_required_ ? false : is_user_gesture_;
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowVirtualCardEnrollBubble(web_contents(), this,
                                                    user_gesture_reprompt));
  DCHECK(bubble_view());
  // Update |bubble_state_| after bubble is shown once. In OnVisibilityChanged()
  // we display the bubble if the the state is kShowingIconAndBubble. Once we
  // open the bubble here once, we set |bubble_state_| to kShowingIcon to make
  // sure further OnVisibilityChanged() don't trigger opening the bubble because
  // we don't want to re-show it every time the web contents become visible.
  bubble_state_ = BubbleState::kShowingIcon;
#endif  // BUILDFLAG(IS_ANDROID)

  // If the dialog is to be shown again because user clicked on links, do not
  // log metrics.
  if (!reprompt_required_) {
    LogVirtualCardEnrollmentBubbleShownMetric(
        GetVirtualCardEnrollmentBubbleSource(), is_user_gesture_);
  }

  // Reset value for the next time tab is switched.
  reprompt_required_ = false;

  if (bubble_shown_closure_for_testing_)
    bubble_shown_closure_for_testing_.Run();
}

VirtualCardEnrollmentBubbleSource
VirtualCardEnrollBubbleControllerImpl::GetVirtualCardEnrollmentBubbleSource() {
  switch (virtual_card_enrollment_fields_.virtual_card_enrollment_source) {
    case VirtualCardEnrollmentSource::kUpstream:
      return VirtualCardEnrollmentBubbleSource::
          VIRTUAL_CARD_ENROLLMENT_UPSTREAM_SOURCE;
    case VirtualCardEnrollmentSource::kDownstream:
      return VirtualCardEnrollmentBubbleSource::
          VIRTUAL_CARD_ENROLLMENT_DOWNSTREAM_SOURCE;
    case VirtualCardEnrollmentSource::kSettingsPage:
      return VirtualCardEnrollmentBubbleSource::
          VIRTUAL_CARD_ENROLLMENT_SETTINGS_PAGE_SOURCE;
    default:
      NOTREACHED();
      return VirtualCardEnrollmentBubbleSource::
          VIRTUAL_CARD_ENROLLMENT_UNKNOWN_SOURCE;
  }
}

#if !BUILDFLAG(IS_ANDROID)
bool VirtualCardEnrollBubbleControllerImpl::IsWebContentsActive() {
  Browser* active_browser = chrome::FindBrowserWithActiveWindow();
  if (!active_browser)
    return false;

  return active_browser->tab_strip_model()->GetActiveWebContents() ==
         web_contents();
}
#endif

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaGlobalRef<jobject>
VirtualCardEnrollBubbleControllerImpl::GetOrCreateJavaDelegate() {
  if (java_delegate_)
    return java_delegate_;
  return java_delegate_ = Java_VirtualCardEnrollmentDelegate_create(
             base::android::AttachCurrentThread(),
             reinterpret_cast<intptr_t>(this));
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(VirtualCardEnrollBubbleControllerImpl);

}  // namespace autofill
