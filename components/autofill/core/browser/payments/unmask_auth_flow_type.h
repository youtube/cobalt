// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_UNMASK_AUTH_FLOW_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_UNMASK_AUTH_FLOW_TYPE_H_

namespace autofill {

// Flow type denotes which card unmask authentication method was used.
// TODO(crbug.com/40216473): Deprecate kCvcThenFido, kCvcFallbackFromFido, and
// kOtpFallbackFromFido.
enum class UnmaskAuthFlowType {
  kNone = 0,
  // Only CVC prompt was shown.
  kCvc = 1,
  // Only WebAuthn prompt was shown.
  kFido = 2,
  // CVC authentication was required in addition to WebAuthn.
  kCvcThenFido = 3,
  // FIDO authentication failed and fell back to CVC authentication.
  kCvcFallbackFromFido = 4,
  // OTP authentication was offered.
  kOtp = 5,
  // FIDO authentication failed and fell back to OTP authentication.
  kOtpFallbackFromFido = 6,
  // VCN 3DS was the only challenge option returned.
  kThreeDomainSecure = 7,
  // VCN 3DS was one of the challenge options returned in the challenge
  // selection dialog, and user selected the 3DS challenge option.
  kThreeDomainSecureConsentAlreadyGiven = 8,
  kMaxValue = kThreeDomainSecureConsentAlreadyGiven,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_UNMASK_AUTH_FLOW_TYPE_H_
