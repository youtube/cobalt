// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_PAYMENTS_FEATURE_AVAILABILITY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_PAYMENTS_FEATURE_AVAILABILITY_H_

namespace autofill {

class CreditCard;

// Returns whether the `card` is populated with a card art image and a card
// product name and whether they both should be shown.
bool ShouldShowCardMetadata(const CreditCard& card);

// TODO(crbug.com/1431355): Move here payments related feature availability
// checks from autofill_experiments.

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_PAYMENTS_FEATURE_AVAILABILITY_H_
