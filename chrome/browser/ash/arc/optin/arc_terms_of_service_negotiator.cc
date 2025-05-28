// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/optin/arc_terms_of_service_negotiator.h"

#include <utility>

namespace arc {

ArcTermsOfServiceNegotiator::ArcTermsOfServiceNegotiator() = default;

ArcTermsOfServiceNegotiator::~ArcTermsOfServiceNegotiator() = default;

void ArcTermsOfServiceNegotiator::StartNegotiation(
    NegotiationCallback callback) {
  DCHECK(pending_callback_.is_null());
  pending_callback_ = std::move(callback);
  StartNegotiationImpl();
}

void ArcTermsOfServiceNegotiator::ReportResult(bool accepted) {
  DCHECK(!pending_callback_.is_null());
  std::move(pending_callback_).Run(accepted);
}

}  // namespace arc
