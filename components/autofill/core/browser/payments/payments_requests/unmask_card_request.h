// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UNMASK_CARD_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UNMASK_CARD_REQUEST_H_

#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill {
namespace payments {

class UnmaskCardRequest : public PaymentsRequest {
 public:
  UnmaskCardRequest(
      const PaymentsClient::UnmaskRequestDetails& request_details,
      const bool full_sync_enabled,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              PaymentsClient::UnmaskResponseDetails&)>
          callback);
  UnmaskCardRequest(const UnmaskCardRequest&) = delete;
  UnmaskCardRequest& operator=(const UnmaskCardRequest&) = delete;
  ~UnmaskCardRequest() override;

  const PaymentsClient::UnmaskResponseDetails& GetResponseDetailsForTesting() {
    return response_details_;
  }

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override;
  bool IsRetryableFailure(const std::string& error_code) override;

 private:
  // Returns whether the response contains all the information of the virtual
  // card to fill into the form.
  bool IsAllCardInformationValidIncludingDcvv();

  // Returns whether the response contains all the necessary information to
  // perform an authentication for a virtual card.
  bool CanPerformVirtualCardAuth();

  PaymentsClient::UnmaskRequestDetails request_details_;
  const bool full_sync_enabled_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          PaymentsClient::UnmaskResponseDetails&)>
      callback_;
  PaymentsClient::UnmaskResponseDetails response_details_;
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UNMASK_CARD_REQUEST_H_
