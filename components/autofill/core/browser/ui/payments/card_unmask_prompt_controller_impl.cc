// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

std::u16string GetSideOfCardTranslationString(bool is_cvc_in_front) {
  return l10n_util::GetStringUTF16(
      is_cvc_in_front
          ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_SECURITY_CODE_POSITION_FRONT_OF_CARD
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_SECURITY_CODE_POSITION_BACK_OF_CARD);
}

}  // namespace

CardUnmaskPromptControllerImpl::CardUnmaskPromptControllerImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

CardUnmaskPromptControllerImpl::~CardUnmaskPromptControllerImpl() {
  if (card_unmask_view_)
    card_unmask_view_->ControllerGone();
}

void CardUnmaskPromptControllerImpl::ShowPrompt(
    CardUnmaskPromptViewFactory card_unmask_view_factory,
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  if (card_unmask_view_)
    card_unmask_view_->ControllerGone();

  new_card_link_clicked_ = false;
  shown_timestamp_ = AutofillClock::Now();
  pending_details_ = CardUnmaskDelegate::UserProvidedUnmaskDetails();
  card_unmask_prompt_options_ = card_unmask_prompt_options;
  card_ = card;
  delegate_ = delegate;
  card_unmask_view_ = std::move(card_unmask_view_factory).Run();
  card_unmask_view_->Show();
  unmasking_result_ = AutofillClient::PaymentsRpcResult::kNone;
  unmasking_number_of_attempts_ = 0;
  AutofillMetrics::LogUnmaskPromptEvent(AutofillMetrics::UNMASK_PROMPT_SHOWN,
                                        card_.HasNonEmptyValidNickname(),
                                        card_.record_type());
}

void CardUnmaskPromptControllerImpl::OnVerificationResult(
    AutofillClient::PaymentsRpcResult result) {
  if (!card_unmask_view_)
    return;

  std::u16string error_message;
  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      break;

    case AutofillClient::PaymentsRpcResult::kTryAgainFailure: {
      if (IsVirtualCard()) {
        error_message = l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_TRY_AGAIN_SECURITY_CODE,
            GetSideOfCardTranslationString(IsCvcInFront()));
      } else {
        error_message = l10n_util::GetStringUTF16(
            IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_TRY_AGAIN_CVC);
      }
      break;
    }

    case AutofillClient::PaymentsRpcResult::kPermanentFailure: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_PERMANENT);
      break;
    }

    case AutofillClient::PaymentsRpcResult::kNetworkError: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_NETWORK);
      break;
    }

    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_DESCRIPTION);
      break;
    }

    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_PERMANENT_ERROR_DESCRIPTION);
      break;
    }

    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }

  unmasking_result_ = result;
  AutofillClient::PaymentsRpcCardType card_type =
      IsVirtualCard() ? AutofillClient::PaymentsRpcCardType::kVirtualCard
                      : AutofillClient::PaymentsRpcCardType::kServerCard;

  AutofillMetrics::LogRealPanResult(result, card_type);
  AutofillMetrics::LogUnmaskingDuration(
      AutofillClock::Now() - verify_timestamp_, result, card_type);
  if (ShouldDismissUnmaskPromptUponResult(unmasking_result_)) {
    card_unmask_view_->Dismiss();
  } else {
    card_unmask_view_->GotVerificationResult(error_message,
                                             AllowsRetry(result));
  }
}

void CardUnmaskPromptControllerImpl::OnUnmaskDialogClosed() {
  card_unmask_view_ = nullptr;
  LogOnCloseEvents();
  unmasking_result_ = AutofillClient::PaymentsRpcResult::kNone;
  if (delegate_)
    delegate_->OnUnmaskPromptClosed();
}

void CardUnmaskPromptControllerImpl::OnUnmaskPromptAccepted(
    const std::u16string& cvc,
    const std::u16string& exp_month,
    const std::u16string& exp_year,
    bool enable_fido_auth) {
  verify_timestamp_ = AutofillClock::Now();
  unmasking_number_of_attempts_++;
  unmasking_result_ = AutofillClient::PaymentsRpcResult::kNone;
  card_unmask_view_->DisableAndWaitForVerification();

  DCHECK(InputCvcIsValid(cvc));
  base::TrimWhitespace(cvc, base::TRIM_ALL, &pending_details_.cvc);
  if (ShouldRequestExpirationDate()) {
    DCHECK(InputExpirationIsValid(exp_month, exp_year));
    pending_details_.exp_month = exp_month;
    pending_details_.exp_year = exp_year;
  }

  // On Android, FIDO authentication is fully launched and its checkbox should
  // always be shown. Remember the last choice the user made on this device.
#if BUILDFLAG(IS_ANDROID)
  pending_details_.enable_fido_auth = enable_fido_auth;
  pref_service_->SetBoolean(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState, enable_fido_auth);
#endif

  // There is a chance the delegate has disappeared (i.e. tab closed) before the
  // unmask response came in. Avoid a crash.
  if (delegate_)
    delegate_->OnUnmaskPromptAccepted(pending_details_);
}

void CardUnmaskPromptControllerImpl::NewCardLinkClicked() {
  new_card_link_clicked_ = true;
}

std::u16string CardUnmaskPromptControllerImpl::GetWindowTitle() const {
#if BUILDFLAG(IS_IOS)
  // The iOS UI has less room for the title so it shows a shorter string.
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE);
#else
  // Set title for VCN retrieval errors first.
  if (unmasking_result_ ==
      AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_VIRTUAL_CARD_PERMANENT_ERROR_TITLE);
  } else if (unmasking_result_ ==
             AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_TITLE);
  }

  // For VCN unmask flow, display unique CVC title.
  if (IsChallengeOptionPresent()) {
#if BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(
            features::kAutofillTouchToFillForCreditCardsAndroid)) {
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE_VIRTUAL_CARD);
    }
#endif
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE_SECURITY_CODE,
        card_.CardNameAndLastFourDigits());
  }

  // Title for expired cards.
  if (ShouldRequestExpirationDate()) {
#if BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(
            features::kAutofillTouchToFillForCreditCardsAndroid)) {
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE_EXPIRED_CARD);
    }
#endif
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CARD_UNMASK_PROMPT_EXPIRED_TITLE,
        card_.CardNameAndLastFourDigits());
  }

  // Default title.
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kAutofillTouchToFillForCreditCardsAndroid)) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE_DEFAULT);
  }
#endif
  return l10n_util::GetStringFUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE,
                                    card_.CardNameAndLastFourDigits());
#endif  // BUILDFLAG(IS_IOS)
}

std::u16string CardUnmaskPromptControllerImpl::GetInstructionsMessage() const {
#if BUILDFLAG(IS_IOS)
  int ids;
  if (card_unmask_prompt_options_.reason ==
          AutofillClient::UnmaskCardReason::kAutofill &&
      ShouldRequestExpirationDate()) {
    ids = IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_EXPIRED;
  } else {
    ids = IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS;
  }
  // The iOS UI shows the card details in the instructions text since they
  // don't fit in the title.
  return l10n_util::GetStringFUTF16(ids, card_.CardNameAndLastFourDigits());
#else
  // If the challenge option is present, return the challenge option instruction
  // information.
  if (IsChallengeOptionPresent()) {
    DCHECK_EQ(card_unmask_prompt_options_.challenge_option->type,
              CardUnmaskChallengeOptionType::kCvc);
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_VIRTUAL_CARD,
        base::NumberToString16(card_unmask_prompt_options_.challenge_option
                                   ->challenge_input_length),
        GetSideOfCardTranslationString(IsCvcInFront()));
  }

  // Instruction message for expired cards.
  if (ShouldRequestExpirationDate()) {
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_EXPIRED_CARD,
        GetSideOfCardTranslationString(IsCvcInFront()));
  }

  // Default instruction message.
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_DEFAULT,
      GetSideOfCardTranslationString(IsCvcInFront()));
#endif
}

std::u16string CardUnmaskPromptControllerImpl::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_CONFIRM_BUTTON);
}

int CardUnmaskPromptControllerImpl::GetCvcImageRid() const {
  return IsCvcInFront() ? IDR_CREDIT_CARD_CVC_HINT_FRONT_AMEX
                        : IDR_CREDIT_CARD_CVC_HINT_BACK;
}

bool CardUnmaskPromptControllerImpl::ShouldRequestExpirationDate() const {
  // We should not display the request to update the expiration date in the
  // virtual card retrieval flow.
  if (IsVirtualCard())
    return false;

  return card_.ShouldUpdateExpiration() ||
         new_card_link_clicked_;
}

#if BUILDFLAG(IS_ANDROID)
std::string CardUnmaskPromptControllerImpl::GetCardIconString() const {
  return card_.CardIconStringForAutofillSuggestion();
}

std::u16string CardUnmaskPromptControllerImpl::GetCardName() const {
  return card_.CardNameForAutofillDisplay();
}

std::u16string CardUnmaskPromptControllerImpl::GetCardLastFourDigits() const {
  return card_.ObfuscatedNumberWithVisibleLastFourDigits();
}

std::u16string CardUnmaskPromptControllerImpl::GetCardExpiration() const {
  return card_.AbbreviatedExpirationDateForDisplay(false);
}

const GURL& CardUnmaskPromptControllerImpl::GetCardArtUrl() const {
  return card_.card_art_url();
}

int CardUnmaskPromptControllerImpl::GetGooglePayImageRid() const {
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
}

bool CardUnmaskPromptControllerImpl::ShouldOfferWebauthn() const {
  return delegate_ && delegate_->ShouldOfferFidoAuth();
}

bool CardUnmaskPromptControllerImpl::GetWebauthnOfferStartState() const {
  return pref_service_->GetBoolean(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState);
}

std::u16string CardUnmaskPromptControllerImpl::GetCvcImageAnnouncement() const {
  return l10n_util::GetStringUTF16(
      IsCvcInFront() ? IDS_AUTOFILL_CARD_UNMASK_CVC_IMAGE_ANNOUNCEMENT_AMEX
                     : IDS_AUTOFILL_CARD_UNMASK_CVC_IMAGE_ANNOUNCEMENT);
}
#endif

bool CardUnmaskPromptControllerImpl::InputCvcIsValid(
    const std::u16string& input_text) const {
  std::u16string trimmed_text;
  base::TrimWhitespace(input_text, base::TRIM_ALL, &trimmed_text);

  // Allow three digit American Express Cvc value when it is a back of card cvc
  // challenge option.
  if (card_unmask_prompt_options_.challenge_option &&
      card_unmask_prompt_options_.challenge_option->cvc_position ==
          CvcPosition::kBackOfCard) {
    return IsValidCreditCardSecurityCode(trimmed_text, card_.network(),
                                         CvcType::kBackOfAmexCvc);
  }
  return IsValidCreditCardSecurityCode(trimmed_text, card_.network());
}

bool CardUnmaskPromptControllerImpl::InputExpirationIsValid(
    const std::u16string& month,
    const std::u16string& year) const {
  if ((month.size() != 2U && month.size() != 1U) ||
      (year.size() != 4U && year.size() != 2U)) {
    return false;
  }

  int month_value = 0, year_value = 0;
  if (!base::StringToInt(month, &month_value) ||
      !base::StringToInt(year, &year_value)) {
    return false;
  }

  // Convert 2 digit year to 4 digit year.
  if (year_value < 100) {
    base::Time::Exploded now;
    AutofillClock::Now().LocalExplode(&now);
    year_value += (now.year / 100) * 100;
  }

  return IsValidCreditCardExpirationDate(year_value, month_value,
                                         AutofillClock::Now());
}

int CardUnmaskPromptControllerImpl::GetExpectedCvcLength() const {
  CvcType cvc_type;

  // The regular CVC length for an American Express card is 4, but if we have an
  // American Express card and a challenge option that denotes the security code
  // is on the back of the American Express card, we need to handle it
  // separately because its length will be 3.
  if (IsChallengeOptionPresent() && card_.network() == kAmericanExpressCard &&
      card_unmask_prompt_options_.challenge_option->cvc_position ==
          CvcPosition::kBackOfCard) {
    cvc_type = CvcType::kBackOfAmexCvc;
  } else {
    cvc_type = CvcType::kRegularCvc;
  }

  return GetCvcLengthForCardNetwork(card_.network(), cvc_type);
}

bool CardUnmaskPromptControllerImpl::IsChallengeOptionPresent() const {
  return card_unmask_prompt_options_.challenge_option.has_value();
}

base::TimeDelta CardUnmaskPromptControllerImpl::GetSuccessMessageDuration()
    const {
  return base::Milliseconds(
      card_unmask_prompt_options_.reason ==
              AutofillClient::UnmaskCardReason::kPaymentRequest
          ? 0
          : 500);
}

AutofillClient::PaymentsRpcResult
CardUnmaskPromptControllerImpl::GetVerificationResult() const {
  return unmasking_result_;
}

bool CardUnmaskPromptControllerImpl::IsVirtualCard() const {
  return card_.record_type() == CreditCard::RecordType::kVirtualCard;
}

#if !BUILDFLAG(IS_IOS)
int CardUnmaskPromptControllerImpl::GetCvcTooltipResourceId() {
  return IsCvcInFront()
             ? IDS_AUTOFILL_CARD_UNMASK_CVC_IMAGE_DESCRIPTION_FOR_AMEX
             : IDS_AUTOFILL_CARD_UNMASK_CVC_IMAGE_DESCRIPTION;
}
#endif

bool CardUnmaskPromptControllerImpl::AllowsRetry(
    AutofillClient::PaymentsRpcResult result) {
  if (result == AutofillClient::PaymentsRpcResult::kNetworkError ||
      result == AutofillClient::PaymentsRpcResult::kPermanentFailure ||
      result ==
          AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure ||
      result ==
          AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure) {
    return false;
  }
  return true;
}

bool CardUnmaskPromptControllerImpl::IsCvcInFront() const {
  // Rely on the challenge option to inform us whether the CVC to be entered is
  // on the front or back of the card.
  if (IsChallengeOptionPresent()) {
    return card_unmask_prompt_options_.challenge_option->cvc_position ==
           CvcPosition::kFrontOfCard;
  }

  // For normal CVC unmask case, depend on the network to inform where the CVC
  // is on the card.
  return card_.network() == kAmericanExpressCard;
}

bool CardUnmaskPromptControllerImpl::ShouldDismissUnmaskPromptUponResult(
    AutofillClient::PaymentsRpcResult result) {
#if BUILDFLAG(IS_ANDROID)
  // For virtual card errors on Android, we'd dismiss the unmask prompt and
  // instead show a different error dialog.
  return result ==
             AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure ||
         result ==
             AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure;
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

void CardUnmaskPromptControllerImpl::LogOnCloseEvents() {
  AutofillMetrics::UnmaskPromptEvent close_reason_event = GetCloseReasonEvent();
  AutofillMetrics::LogUnmaskPromptEvent(close_reason_event,
                                        card_.HasNonEmptyValidNickname(),
                                        card_.record_type());
  AutofillMetrics::LogUnmaskPromptEventDuration(
      AutofillClock::Now() - shown_timestamp_, close_reason_event,
      card_.HasNonEmptyValidNickname());

  if (close_reason_event == AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS)
    return;

  if (close_reason_event ==
      AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING) {
    AutofillMetrics::LogTimeBeforeAbandonUnmasking(
        AutofillClock::Now() - verify_timestamp_,
        card_.HasNonEmptyValidNickname());
  }
}

AutofillMetrics::UnmaskPromptEvent
CardUnmaskPromptControllerImpl::GetCloseReasonEvent() {
  if (unmasking_number_of_attempts_ == 0)
    return AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS;

  // If NONE and we have a pending request, we have a pending GetRealPan
  // request.
  if (unmasking_result_ == AutofillClient::PaymentsRpcResult::kNone)
    return AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING;

  if (unmasking_result_ == AutofillClient::PaymentsRpcResult::kSuccess) {
    return unmasking_number_of_attempts_ == 1
               ? AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT
               : AutofillMetrics::
                     UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS;
  }
  return AllowsRetry(unmasking_result_)
             ? AutofillMetrics::
                   UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE
             : AutofillMetrics::
                   UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE;
}

}  // namespace autofill
