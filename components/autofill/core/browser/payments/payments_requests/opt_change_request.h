// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_OPT_CHANGE_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_OPT_CHANGE_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace base {
class Value;
}  // namespace base

namespace autofill::payments {

class OptChangeRequest : public PaymentsRequest {
 public:
  OptChangeRequest(
      const PaymentsClient::OptChangeRequestDetails& request_details,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              PaymentsClient::OptChangeResponseDetails&)>
          callback,
      const bool full_sync_enabled);
  OptChangeRequest(const OptChangeRequest&) = delete;
  OptChangeRequest& operator=(const OptChangeRequest&) = delete;
  ~OptChangeRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override;

 private:
  PaymentsClient::OptChangeRequestDetails request_details_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          PaymentsClient::OptChangeResponseDetails&)>
      callback_;
  const bool full_sync_enabled_;
  PaymentsClient::OptChangeResponseDetails response_details_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_OPT_CHANGE_REQUEST_H_
