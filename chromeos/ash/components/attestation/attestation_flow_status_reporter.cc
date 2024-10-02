// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow_status_reporter.h"

#include "base/metrics/histogram_functions.h"

namespace ash {
namespace attestation {

namespace {

constexpr char kAttestationFlowStatusName[] =
    "ChromeOS.Attestation.AttestationFlowStatus";
constexpr int kAttestationFlowStatusMaxValue = 1 << 6;

}  // namespace

AttestationFlowStatusReporter::AttestationFlowStatusReporter() = default;

AttestationFlowStatusReporter::~AttestationFlowStatusReporter() {
  Report();
}

void AttestationFlowStatusReporter::OnHasProxy(bool has_proxy) {
  has_proxy_ = has_proxy;
}

void AttestationFlowStatusReporter::OnIsSystemProxyAvailable(
    bool is_system_proxy_available) {
  is_system_proxy_available_ = is_system_proxy_available;
}

void AttestationFlowStatusReporter::OnDefaultFlowStatus(bool success) {
  does_default_flow_succeed_ = success;
}

void AttestationFlowStatusReporter::OnFallbackFlowStatus(bool success) {
  does_fallback_flow_succeed_ = success;
}

void AttestationFlowStatusReporter::Report() {
  // Perform rationality check and define all the missing flags.
  DCHECK(has_proxy_.has_value());
  DCHECK(is_system_proxy_available_.has_value());
  DCHECK(does_default_flow_succeed_.has_value() ||
         does_fallback_flow_succeed_.has_value());
  const bool does_run_default_flow = does_default_flow_succeed_.has_value();
  const bool does_default_flow_succeed =
      does_run_default_flow && *does_default_flow_succeed_;
  const bool does_run_fallback_flow = does_fallback_flow_succeed_.has_value();
  const bool does_fallback_flow_succeed =
      does_run_fallback_flow && *does_fallback_flow_succeed_;

  int value = 0;
  for (bool flag : {*has_proxy_, *is_system_proxy_available_,
                    does_run_default_flow, does_default_flow_succeed,
                    does_run_fallback_flow, does_fallback_flow_succeed}) {
    value = (value << 1) | flag;
  }

  base::UmaHistogramExactLinear(kAttestationFlowStatusName, value,
                                kAttestationFlowStatusMaxValue);
}

}  // namespace attestation
}  // namespace ash
