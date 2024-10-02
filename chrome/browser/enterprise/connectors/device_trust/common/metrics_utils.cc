// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enrollment status of the device where the Device Trust connector attestation
// is happening. These values are persisted to logs and should not be
// renumbered. Please update the DTEnrollmentStatus enum in enums.xml when
// adding a new step here.
enum class DTEnrollmentStatus {
  kManaged = 0,
  kUnmanaged = 1,
  kMaxValue = kUnmanaged,
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

DTHandshakeResult ResponseToResult(const DeviceTrustResponse& response) {
  if (!response.error) {
    return DTHandshakeResult::kSuccess;
  }

  switch (response.error.value()) {
    case DeviceTrustError::kUnknown:
      return DTHandshakeResult::kUnknown;
    case DeviceTrustError::kTimeout:
      return DTHandshakeResult::kTimeout;
    case DeviceTrustError::kFailedToParseChallenge:
      return DTHandshakeResult::kFailedToParseChallenge;
    case DeviceTrustError::kFailedToCreateResponse:
      return DTHandshakeResult::kFailedToCreateResponse;
  }
}

}  // namespace

void LogAttestationFunnelStep(DTAttestationFunnelStep step) {
  static constexpr char kFunnelStepHistogram[] =
      "Enterprise.DeviceTrust.Attestation.Funnel";
  base::UmaHistogramEnumeration(kFunnelStepHistogram, step);
  VLOG(1) << "Device Trust attestation step: " << static_cast<int>(step);
}

void LogAttestationResult(DTAttestationResult result) {
  static constexpr char kAttestationResultHistogram[] =
      "Enterprise.DeviceTrust.Attestation.Result";
  base::UmaHistogramEnumeration(kAttestationResultHistogram, result);
  if (result == DTAttestationResult::kSuccess) {
    VLOG(1) << "Device Trust attestation was successful";
  } else {
    LOG(ERROR) << "Device Trust attestation error: "
               << AttestationResultToString(result);
  }
}

void LogDeviceTrustResponse(const DeviceTrustResponse& response,
                            base::TimeTicks start_time) {
  static constexpr char kLatencyHistogramFormat[] =
      "Enterprise.DeviceTrust.Attestation.ResponseLatency.%s";
  base::UmaHistogramTimes(
      base::StringPrintf(kLatencyHistogramFormat,
                         response.error ? "Failure" : "Success"),
      base::TimeTicks::Now() - start_time);

  static constexpr char kHandshakeResultHistogram[] =
      "Enterprise.DeviceTrust.Handshake.Result";
  base::UmaHistogramEnumeration(kHandshakeResultHistogram,
                                ResponseToResult(response));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void LogOrigin(DTOrigin origin) {
  static constexpr char kOriginHistogram[] = "Enterprise.DeviceTrust.Origin";
  base::UmaHistogramEnumeration(kOriginHistogram, origin);
}

void LogEnrollmentStatus() {
  static constexpr char kEnrollmentStatusHistogram[] =
      "Enterprise.DeviceTrust.EnrollmentStatus";
  base::UmaHistogramEnumeration(
      kEnrollmentStatusHistogram,
      ash::InstallAttributes::Get()->IsEnterpriseManaged()
          ? DTEnrollmentStatus::kManaged
          : DTEnrollmentStatus::kUnmanaged);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace enterprise_connectors
