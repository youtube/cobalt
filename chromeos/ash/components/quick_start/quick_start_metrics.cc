// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace ash::quick_start {

namespace {

constexpr const char kWifiTransferResultHistogramName[] =
    "QuickStart.WifiTransferResult";
constexpr const char kWifiTransferResultFailureReasonHistogramName[] =
    "QuickStart.WifiTransferResult.FailureReason";
constexpr const char
    kFastPairAdvertisementEndedAdvertisingMethodHistogramName[] =
        "QuickStart.FastPairAdvertisementEnded.AdvertisingMethod";
constexpr const char kFastPairAdvertisementEndedSucceededHistogramName[] =
    "QuickStart.FastPairAdvertisementEnded.Succeeded";
constexpr const char kFastPairAdvertisementEndedDurationHistogramName[] =
    "QuickStart.FastPairAdvertisementEnded.Duration";
constexpr const char kFastPairAdvertisementEndedErrorCodeHistogramName[] =
    "QuickStart.FastPairAdvertisementEnded.ErrorCode";
constexpr const char
    kFastPairAdvertisementStartedAdvertisingMethodHistogramName[] =
        "QuickStart.FastPairAdvertisementStarted.AdvertisingMethod";
constexpr const char kMessageReceivedWifiCredentials[] =
    "QuickStart.MessageReceived.WifiCredentials";
constexpr const char kMessageReceivedBootstrapConfigurations[] =
    "QuickStart.MessageReceived.BootstrapConfigurations";
constexpr const char kMessageReceivedHandshake[] =
    "QuickStart.MessageReceived.Handshake";
constexpr const char kMessageReceivedNotifySourceOfUpdate[] =
    "QuickStart.MessageReceived.NotifySourceOfUpdate";
constexpr const char kMessageReceivedGetInfo[] =
    "QuickStart.MessageReceived.GetInfo";
constexpr const char kMessageReceivedAssertion[] =
    "QuickStart.MessageReceived.Assertion";
constexpr const char kMessageReceivedDesiredMessageTypeName[] =
    "QuickStart.MessageReceived.DesiredMessageType";
constexpr const char kMessageSentMessageTypeName[] =
    "QuickStart.MessageSent.MessageType";
constexpr const char kHandshakeResultSucceededName[] =
    "QuickStart.HandshakeResult.Succeeded";
constexpr const char kHandshakeResultDurationName[] =
    "QuickStart.HandshakeResult.Duration";
constexpr const char kHandshakeResultErrorCodeName[] =
    "QuickStart.HandshakeResult.ErrorCode";
constexpr const char kHandshakeStartedName[] = "QuickStart.HandshakeStarted";
constexpr const char kGaiaTransferAttemptedName[] =
    "QuickStart.GaiaTransferAttempted";
constexpr const char kGaiaTransferResultName[] =
    "QuickStart.GaiaTransferResult";
constexpr const char kGaiaTransferResultFailureReasonName[] =
    "QuickStart.GaiaTransferResult.FailureReason";
constexpr const char kScreenOpened[] = "QuickStart.ScreenOpened";

std::string MapMessageTypeToMetric(
    QuickStartMetrics::MessageType message_type) {
  switch (message_type) {
    case QuickStartMetrics::MessageType::kWifiCredentials:
      return kMessageReceivedWifiCredentials;
    case QuickStartMetrics::MessageType::kBootstrapConfigurations:
      return kMessageReceivedBootstrapConfigurations;
    case QuickStartMetrics::MessageType::kHandshake:
      return kMessageReceivedHandshake;
    case QuickStartMetrics::MessageType::kNotifySourceOfUpdate:
      return kMessageReceivedNotifySourceOfUpdate;
    case QuickStartMetrics::MessageType::kGetInfo:
      return kMessageReceivedGetInfo;
    case QuickStartMetrics::MessageType::kAssertion:
      return kMessageReceivedAssertion;
  }
}

}  // namespace

// static
QuickStartMetrics::MessageType QuickStartMetrics::MapResponseToMessageType(
    QuickStartResponseType response_type) {
  switch (response_type) {
    case QuickStartResponseType::kWifiCredentials:
      return MessageType::kWifiCredentials;
    case QuickStartResponseType::kBootstrapConfigurations:
      return MessageType::kBootstrapConfigurations;
    case QuickStartResponseType::kHandshake:
      return MessageType::kHandshake;
    case QuickStartResponseType::kNotifySourceOfUpdate:
      return MessageType::kNotifySourceOfUpdate;
    case QuickStartResponseType::kGetInfo:
      return MessageType::kGetInfo;
    case QuickStartResponseType::kAssertion:
      return MessageType::kAssertion;
  }
}

// static
void QuickStartMetrics::RecordScreenOpened(ScreenName screen) {
  // TODO(b/298042953): Add metric for previous screen.
  base::UmaHistogramEnumeration(kScreenOpened, screen);
}

// static
void QuickStartMetrics::RecordScreenClosed(
    ScreenName screen,
    int32_t session_id,
    base::Time timestamp,
    absl::optional<ScreenName> previous_screen) {
  // TODO(b/298042953): Add metric for screen duration.
}

// static
void QuickStartMetrics::RecordWifiTransferResult(
    bool succeeded,
    absl::optional<WifiTransferResultFailureReason> failure_reason) {
  if (succeeded) {
    CHECK(!failure_reason.has_value());
  } else {
    CHECK(failure_reason.has_value());
    base::UmaHistogramEnumeration(kWifiTransferResultFailureReasonHistogramName,
                                  failure_reason.value());
  }
  base::UmaHistogramBoolean(kWifiTransferResultHistogramName, succeeded);
}

// static
void QuickStartMetrics::RecordGaiaTransferAttempted(bool attempted) {
  base::UmaHistogramBoolean(kGaiaTransferAttemptedName, attempted);
}

// static
void QuickStartMetrics::RecordFastPairAdvertisementStarted(
    AdvertisingMethod advertising_method) {
  base::UmaHistogramEnumeration(
      kFastPairAdvertisementStartedAdvertisingMethodHistogramName,
      advertising_method);
}

// static
void QuickStartMetrics::RecordFastPairAdvertisementEnded(
    AdvertisingMethod advertising_method,
    bool succeeded,
    base::TimeDelta duration,
    absl::optional<FastPairAdvertisingErrorCode> error_code) {
  if (succeeded) {
    CHECK(!error_code.has_value());
  } else {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(
        kFastPairAdvertisementEndedErrorCodeHistogramName, error_code.value());
  }
  base::UmaHistogramBoolean(kFastPairAdvertisementEndedSucceededHistogramName,
                            succeeded);
  base::UmaHistogramTimes(kFastPairAdvertisementEndedDurationHistogramName,
                          duration);
  base::UmaHistogramEnumeration(
      kFastPairAdvertisementEndedAdvertisingMethodHistogramName,
      advertising_method);
}

// static
void QuickStartMetrics::RecordNearbyConnectionsAdvertisementStarted(
    int32_t session_id) {
  // TODO(279614071): Add advertising metrics.
}

// static
void QuickStartMetrics::RecordNearbyConnectionsAdvertisementEnded(
    int32_t session_id,
    AdvertisingMethod advertising_method,
    bool succeeded,
    int duration,
    absl::optional<NearbyConnectionsAdvertisingErrorCode> error_code) {
  // TODO(279614071): Add advertising metrics.
}

// static
void QuickStartMetrics::RecordHandshakeStarted(bool handshake_started) {
  base::UmaHistogramBoolean(kHandshakeStartedName, handshake_started);
}

// static
void QuickStartMetrics::RecordHandshakeResult(
    bool succeeded,
    base::TimeDelta duration,
    absl::optional<HandshakeErrorCode> error_code) {
  if (succeeded) {
    CHECK(!error_code.has_value());
  } else {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(kHandshakeResultErrorCodeName,
                                  error_code.value());
  }
  base::UmaHistogramBoolean(kHandshakeResultSucceededName, succeeded);
  base::UmaHistogramTimes(kHandshakeResultDurationName, duration);
}

// static
void QuickStartMetrics::RecordMessageSent(MessageType message_type) {
  base::UmaHistogramEnumeration(kMessageSentMessageTypeName, message_type);
}

// static
void QuickStartMetrics::RecordMessageReceived(
    MessageType desired_message_type,
    bool succeeded,
    base::TimeDelta listen_duration,
    absl::optional<MessageReceivedErrorCode> error_code) {
  std::string metric_name = MapMessageTypeToMetric(desired_message_type);
  if (succeeded) {
    CHECK(!error_code.has_value());
  } else {
    CHECK(error_code.has_value());
    base::UmaHistogramEnumeration(metric_name + ".ErrorCode",
                                  error_code.value());
  }
  base::UmaHistogramBoolean(metric_name + ".Succeeded", succeeded);
  base::UmaHistogramTimes(metric_name + ".ListenDuration", listen_duration);
  base::UmaHistogramEnumeration(kMessageReceivedDesiredMessageTypeName,
                                desired_message_type);
}

// static
void QuickStartMetrics::RecordAttestationCertificateRequested(
    int32_t session_id) {
  // TODO(279614284): Add FIDO assertion metrics.
}

// static
void QuickStartMetrics::RecordAttestationCertificateRequestEnded(
    int32_t session_id,
    bool succeded,
    int duration,
    absl::optional<AttestationCertificateRequestErrorCode> error_code) {
  // TODO(279614284): Add FIDO assertion metrics.
}

// static
void QuickStartMetrics::RecordGaiaTransferResult(
    bool succeeded,
    absl::optional<GaiaTransferResultFailureReason> failure_reason) {
  if (succeeded) {
    CHECK(!failure_reason.has_value());
  } else {
    CHECK(failure_reason.has_value());
    base::UmaHistogramEnumeration(kGaiaTransferResultFailureReasonName,
                                  failure_reason.value());
  }
  base::UmaHistogramBoolean(kGaiaTransferResultName, succeeded);
}

// static
void QuickStartMetrics::RecordEntryPoint(EntryPoint entry_point) {
  // TODO(280306867): Add metric for entry point.
}

QuickStartMetrics::QuickStartMetrics() = default;

QuickStartMetrics::~QuickStartMetrics() = default;

}  // namespace ash::quick_start
