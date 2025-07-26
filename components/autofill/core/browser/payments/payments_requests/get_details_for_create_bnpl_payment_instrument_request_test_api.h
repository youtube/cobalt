// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_CREATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_CREATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_bnpl_payment_instrument_request.h"

namespace autofill::payments {

class GetDetailsForCreateBnplPaymentInstrumentRequestTestApi {
 public:
  explicit GetDetailsForCreateBnplPaymentInstrumentRequestTestApi(
      GetDetailsForCreateBnplPaymentInstrumentRequest*
          get_details_for_create_bnpl_payment_instrument_request)
      : get_details_for_create_bnpl_payment_instrument_request_(CHECK_DEREF(
            get_details_for_create_bnpl_payment_instrument_request)) {}
  GetDetailsForCreateBnplPaymentInstrumentRequestTestApi(
      const GetDetailsForCreateBnplPaymentInstrumentRequestTestApi&) = delete;
  GetDetailsForCreateBnplPaymentInstrumentRequestTestApi& operator=(
      const GetDetailsForCreateBnplPaymentInstrumentRequestTestApi&) = delete;
  ~GetDetailsForCreateBnplPaymentInstrumentRequestTestApi() = default;

  std::u16string get_context_token() const {
    return get_details_for_create_bnpl_payment_instrument_request_
        ->context_token_;
  }

  base::Value::Dict* get_legal_message() const {
    return get_details_for_create_bnpl_payment_instrument_request_
        ->legal_message_.get();
  }

 private:
  const raw_ref<GetDetailsForCreateBnplPaymentInstrumentRequest>
      get_details_for_create_bnpl_payment_instrument_request_;
};

inline GetDetailsForCreateBnplPaymentInstrumentRequestTestApi test_api(
    GetDetailsForCreateBnplPaymentInstrumentRequest& request) {
  return GetDetailsForCreateBnplPaymentInstrumentRequestTestApi(&request);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_CREATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_TEST_API_H_
