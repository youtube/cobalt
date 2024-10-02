// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTHENTICATION_REQUESTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTHENTICATION_REQUESTER_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/full_card_request.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"
#endif

namespace autofill {

// Test class for requesting authentication from CreditCardCvcAuthenticator or
// CreditCardFidoAuthenticator.
#if BUILDFLAG(IS_IOS)
class TestAuthenticationRequester
    : public CreditCardCvcAuthenticator::Requester,
      public CreditCardOtpAuthenticator::Requester {
#else
class TestAuthenticationRequester
    : public CreditCardCvcAuthenticator::Requester,
      public CreditCardFidoAuthenticator::Requester,
      public CreditCardOtpAuthenticator::Requester {
#endif
 public:
  TestAuthenticationRequester();
  ~TestAuthenticationRequester() override;

  // CreditCardCvcAuthenticator::Requester:
  void OnCvcAuthenticationComplete(
      const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response)
      override;
#if BUILDFLAG(IS_ANDROID)
  bool ShouldOfferFidoAuth() const override;
  bool UserOptedInToFidoFromSettingsPageOnMobile() const override;
#endif

#if !BUILDFLAG(IS_IOS)
  // CreditCardFidoAuthenticator::Requester:
  void OnFIDOAuthenticationComplete(
      const CreditCardFidoAuthenticator::FidoAuthenticationResponse& response)
      override;
  void OnFidoAuthorizationComplete(bool did_succeed) override;

  void IsUserVerifiableCallback(bool is_user_verifiable);
#endif

  // CreditCardOtpAuthenticator::Requester:
  void OnOtpAuthenticationComplete(
      const CreditCardOtpAuthenticator::OtpAuthenticationResponse& response)
      override;

  base::WeakPtr<TestAuthenticationRequester> GetWeakPtr();

  absl::optional<bool> is_user_verifiable() { return is_user_verifiable_; }

  absl::optional<bool> did_succeed() { return did_succeed_; }

  std::u16string number() { return number_; }

  payments::FullCardRequest::FailureType failure_type() {
    return failure_type_;
  }

 private:
  // Set when CreditCardFidoAuthenticator invokes IsUserVerifiableCallback().
  absl::optional<bool> is_user_verifiable_;

  // Is set to true if authentication was successful.
  absl::optional<bool> did_succeed_;

  // The failure type of the full card request. Set when the request is
  // finished.
  payments::FullCardRequest::FailureType failure_type_ =
      payments::FullCardRequest::UNKNOWN;

  // The card number returned from On*AuthenticationComplete().
  std::u16string number_;

  base::WeakPtrFactory<TestAuthenticationRequester> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTHENTICATION_REQUESTER_H_
