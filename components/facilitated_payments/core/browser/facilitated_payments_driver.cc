// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/pix_manager.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/validation/pix_code_validator.h"

namespace payments::facilitated {

FacilitatedPaymentsDriver::FacilitatedPaymentsDriver(
    std::unique_ptr<PixManager> pix_manager,
    std::unique_ptr<EwalletManager> ewallet_manager)
    : pix_manager_(std::move(pix_manager)),
      ewallet_manager_(std::move(ewallet_manager)) {}

FacilitatedPaymentsDriver::~FacilitatedPaymentsDriver() = default;

void FacilitatedPaymentsDriver::DidNavigateToOrAwayFromPage() const {
  pix_manager_->Reset();
  ewallet_manager_->Reset();
}

void FacilitatedPaymentsDriver::OnTextCopiedToClipboard(
    const GURL& render_frame_host_url,
    const std::u16string& copied_text,
    ukm::SourceId ukm_source_id) {
  if (!PixCodeValidator::ContainsPixIdentifier(
          base::UTF16ToUTF8(copied_text))) {
    return;
  }
  pix_manager_->OnPixCodeCopiedToClipboard(
      render_frame_host_url, base::UTF16ToUTF8(copied_text), ukm_source_id);
}

void FacilitatedPaymentsDriver::TriggerEwalletPushPayment(
    const GURL& payment_link_url,
    const GURL& page_url,
    ukm::SourceId ukm_source_id) {
  CHECK(ewallet_manager_);
  ewallet_manager_->TriggerEwalletPushPayment(payment_link_url, page_url,
                                              ukm_source_id);
}

void FacilitatedPaymentsDriver::SetPixManagerForTesting(
    std::unique_ptr<PixManager> pix_manager) {
  pix_manager_ = std::move(pix_manager);
}

void FacilitatedPaymentsDriver::SetEwalletManagerForTesting(
    std::unique_ptr<EwalletManager> ewallet_manager) {
  ewallet_manager_ = std::move(ewallet_manager);
}

}  // namespace payments::facilitated
