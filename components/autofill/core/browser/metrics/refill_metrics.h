// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_REFILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_REFILL_METRICS_H_

#include "components/autofill/core/browser/filling/form_filler.h"

namespace autofill::autofill_metrics {

void LogRefillTriggerReason(RefillTriggerReason refill_trigger_reason);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_REFILL_METRICS_H_
