// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"

namespace autofill {

MockAutofillOptimizationGuide::MockAutofillOptimizationGuide()
    : AutofillOptimizationGuide(nullptr) {}
MockAutofillOptimizationGuide::~MockAutofillOptimizationGuide() = default;

}  // namespace autofill
