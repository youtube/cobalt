// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_UTILS_FACILITATED_PAYMENTS_UTILS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_UTILS_FACILITATED_PAYMENTS_UTILS_H_

namespace payments::facilitated {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.facilitated_payments
// The result of invoking the purchase manager with an action token.
enum class PurchaseActionResult : int {
  // Could not invoke the purchase manager.
  kCouldNotInvoke,

  // The purchase manager was invoked successfully.
  kResultOk,

  // The user cancelled out of the purchase manager flow.
  kResultCanceled,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.facilitated_payments
// The result of invoking the instrument manager account linking flow.
enum class AccountLinkingResultCode : int {
  // Could not invoke the instrument manager.
  kCouldNotInvoke,

  // The account linking flow completed successfully.
  kResultOk,

  // The user cancelled out of the account linking flow.
  kResultCanceled,

  // An error occurred during account linking.
  kResultError,
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_UTILS_FACILITATED_PAYMENTS_UTILS_H_
