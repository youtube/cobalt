// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPLOAD_CARD_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPLOAD_CARD_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace base {
class Value;
}  // namespace base

namespace autofill::payments {

class UploadCardRequest : public PaymentsRequest {
 public:
  UploadCardRequest(
      const PaymentsClient::UploadRequestDetails& request_details,
      const bool full_sync_enabled,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              const PaymentsClient::UploadCardResponseDetails&)>
          callback);
  UploadCardRequest(const UploadCardRequest&) = delete;
  UploadCardRequest& operator=(const UploadCardRequest&) = delete;
  ~UploadCardRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override;

 private:
  const PaymentsClient::UploadRequestDetails request_details_;
  const bool full_sync_enabled_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          const PaymentsClient::UploadCardResponseDetails&)>
      callback_;
  PaymentsClient::UploadCardResponseDetails upload_card_response_details_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPLOAD_CARD_REQUEST_H_
