// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_validation.h"

#include <string>
#include <vector>

#include "components/payments/core/native_error_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

// Creates and returns a minimal SecurePaymentConfirmationRequest object with
// only required fields filled in to pass validation.
mojom::SecurePaymentConfirmationRequestPtr CreateValidRequest() {
  auto spc_request = mojom::SecurePaymentConfirmationRequest::New();

  spc_request->credential_ids.push_back({1, 2, 3, 4});
  spc_request->challenge = {5, 6, 7, 8};
  spc_request->instrument = blink::mojom::PaymentCredentialInstrument::New();
  spc_request->instrument->display_name = "Display Name";
  spc_request->instrument->icon = GURL("https://site.example/icon.png");
  spc_request->payee_origin =
      url::Origin::Create(GURL("https://merchant.example"));
  spc_request->rp_id = "rp.example";

  return spc_request;
}

TEST(SecurePaymentConfirmationValidationTest, IsValidRequest) {
  auto request = CreateValidRequest();
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyCredentialIds) {
  auto request = CreateValidRequest();
  request->credential_ids.clear();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example"))),
      SecurePaymentConfirmationRequestValidationError::kCredentialIdsRequired);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyCredentialId) {
  auto request = CreateValidRequest();
  request->credential_ids.emplace_back();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example"))),
      SecurePaymentConfirmationRequestValidationError::kCredentialIdsRequired);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyChallenge) {
  auto request = CreateValidRequest();
  request->challenge.clear();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example"))),
      SecurePaymentConfirmationRequestValidationError::kChallengeRequired);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyDisplayName) {
  auto request = CreateValidRequest();
  request->instrument->display_name.clear();
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kInstrumentDisplayNameRequired);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyInstrumentIcon) {
  auto request = CreateValidRequest();
  request->instrument->icon = GURL();
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kValidInstrumentIconRequired);
}

TEST(SecurePaymentConfirmationValidationTest, InvalidInstrumentIcon) {
  auto request = CreateValidRequest();
  request->instrument->icon = GURL("not-a-url");
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kValidInstrumentIconRequired);
}

TEST(SecurePaymentConfirmationValidationTest, NonUtf8InstrumentDetails) {
  auto request = CreateValidRequest();
  request->instrument->details = {'\xEF', '\xB7', '\xAF'};
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kNonUtf8InstrumentDetailsString);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyInstrumentDetails) {
  auto request = CreateValidRequest();
  request->instrument->details = "";
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kEmptyInstrumentDetailsString);
}

TEST(SecurePaymentConfirmationValidationTest, TooLongInstrumentDetails) {
  auto request = CreateValidRequest();
  request->instrument->details = std::string(4097, '.');
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kTooLongInstrumentDetailsString);
}

TEST(SecurePaymentConfirmationValidationTest, InvalidRpId) {
  std::string invalid_cases[] = {
      "",
      "domains cannot have spaces.example",
      "https://bank.example",
      "username:password@bank.example",
      "bank.example/has/a/path",
      "139.56.146.66",
      "9d68:ea08:fc14:d8be:344c:60a0:c4db:e478",
  };
  for (const std::string& rp_id : invalid_cases) {
    auto request = CreateValidRequest();
    request->rp_id = rp_id;
    EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                  request, url::Origin::Create(GURL("https://rp.example"))),
              SecurePaymentConfirmationRequestValidationError::kRpIdRequired)
        << "rp_id: " << rp_id;
  }
}

TEST(SecurePaymentConfirmationValidationTest, MissingPayeeNameAndPayeeOrigin) {
  auto request = CreateValidRequest();
  request->payee_name.reset();
  request->payee_origin.reset();
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kPayeeOriginOrPayeeNameRequired);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyPayeeName) {
  auto request = CreateValidRequest();
  request->payee_name = "";
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kPayeeOriginOrPayeeNameRequired);
}

TEST(SecurePaymentConfirmationValidationTest, NonHttpsPayeeOrigin) {
  auto request = CreateValidRequest();
  request->payee_origin = url::Origin::Create(GURL("http://site.example"));
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example"))),
      SecurePaymentConfirmationRequestValidationError::kPayeeOriginMustBeHttps);
}

TEST(SecurePaymentConfirmationValidationTest, NullPaymentEntityLogo) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(nullptr);
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kNonNullPaymentEntityLogoRequired);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyPaymentEntityLogoUrl) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(
      mojom::PaymentEntityLogo::New(GURL(), "Label"));
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example"))),
      SecurePaymentConfirmationRequestValidationError::kValidLogoUrlRequired);
}

TEST(SecurePaymentConfirmationValidationTest, InvalidPaymentEntityLogoUrl) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(
      mojom::PaymentEntityLogo::New(GURL("thisisnotaurl"), "Label"));
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example"))),
      SecurePaymentConfirmationRequestValidationError::kValidLogoUrlRequired);
}

TEST(SecurePaymentConfirmationValidationTest,
     DisallowedSchemePaymentEntityLogoUrl) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      GURL("blob://blob.foo.com/logo.png"), "Label"));
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::
                kValidLogoUrlSchemeRequired);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyPaymentEntityLogoLabel) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      GURL("https://entity.example/icon.png"), ""));
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example"))),
      SecurePaymentConfirmationRequestValidationError::kLogoLabelRequired);
}

TEST(SecurePaymentConfirmationValidationTest,
     WebAuthnExtensionsAllowedForFirstParty) {
  auto request = CreateValidRequest();
  request->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  request->extensions->get_cred_blob = true;
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyWebAuthnExtensionsAllowed) {
  auto request = CreateValidRequest();
  request->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();

  // An empty WebAuthn extensions dictionary is allowed for both first and third
  // party requests.
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example"))),
            SecurePaymentConfirmationRequestValidationError::kOk);
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://third-party.example"))),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST(SecurePaymentConfirmationValidationTest,
     WebAuthnExtensionsDisallowedForThirdParties) {
  // It is not feasible to test every WebAuthn extension field (as more may be
  // added in the future), so we test one representative field (get_cred_blob).
  auto request = CreateValidRequest();
  request->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  request->extensions->get_cred_blob = true;

  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://third-party.example"))),
      SecurePaymentConfirmationRequestValidationError::
          kWebAuthnExtensionsNotSupported);
}

}  // namespace
}  // namespace payments
