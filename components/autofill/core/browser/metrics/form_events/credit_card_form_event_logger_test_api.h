// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_CREDIT_CARD_FORM_EVENT_LOGGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_CREDIT_CARD_FORM_EVENT_LOGGER_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"

namespace autofill::autofill_metrics {

class CreditCardFormEventLoggerTestApi {
 public:
  explicit CreditCardFormEventLoggerTestApi(CreditCardFormEventLogger* logger)
      : logger_(CHECK_DEREF(logger)) {}
  CreditCardFormEventLoggerTestApi(const CreditCardFormEventLoggerTestApi&) =
      delete;
  CreditCardFormEventLoggerTestApi& operator=(
      const CreditCardFormEventLoggerTestApi&) = delete;
  ~CreditCardFormEventLoggerTestApi() = default;

  size_t server_record_type_count() const {
    return logger_->server_record_type_count_;
  }

  size_t local_record_type_count() const {
    return logger_->local_record_type_count_;
  }

 private:
  const raw_ref<CreditCardFormEventLogger> logger_;
};

inline CreditCardFormEventLoggerTestApi test_api(
    CreditCardFormEventLogger& logger) {
  return CreditCardFormEventLoggerTestApi(&logger);
}

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_CREDIT_CARD_FORM_EVENT_LOGGER_TEST_API_H_
