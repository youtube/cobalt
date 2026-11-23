// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_ACCOUNT_LINKING_RESULT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_ACCOUNT_LINKING_RESULT_H_

#include <cstdint>

#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"

namespace payments::facilitated {

struct AccountLinkingResult {
  bool is_successful = false;
  int64_t instrument_id = 0;
  AccountLinkingResultCode error_code =
      AccountLinkingResultCode::kCouldNotInvoke;

  bool operator==(const AccountLinkingResult& other) const = default;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_ACCOUNT_LINKING_RESULT_H_
