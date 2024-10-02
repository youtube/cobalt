// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_metrics_logger.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"

namespace {

const size_t kBytesPerKilobyte = 1024;
const uint64_t k5MbInBytes = 5242880;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The numbers here correspond to the
// ordering of the flow. This enum should be kept in sync with the
// NearbyShareBackgroundScanningSetupNotificationFlowEvent enum in
// src/tools/metrics/histograms/enums.xml.
enum class BackgroundScanningDevicesDetectedEvent {
  kNearbyDevicesDetected = 1,
  kMaxValue = kNearbyDevicesDetected
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class TransferFinalStatus {
  kComplete = 0,
  kUnknown = 1,
  kAwaitingRemoteAcceptanceFailed = 2,
  kFailed = 3,
  kRejected = 4,
  kCancelled = 5,
  kTimedOut = 6,
  kMediaUnavailable = 7,
  kNotEnoughSpace = 8,
  kUnsupportedAttachmentType = 9,
  kDecodeAdvertisementFailed = 10,
  kMissingTransferUpdateCallback = 11,
  kMissingShareTarget = 12,
  kMissingEndpointId = 13,
  kMissingPayloads = 14,
  kPairedKeyVerificationFailed = 15,
  kInvalidIntroductionFrame = 16,
  kIncompletePayloads = 17,
  kFailedToCreateShareTarget = 18,
  kFailedToInitiateOutgoingConnection = 19,
  kFailedToReadOutgoingConnectionResponse = 20,
  kUnexpectedDisconnection = 21,
  kMaxValue = kUnexpectedDisconnection
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class StartAdvertisingFailureReason {
  kUnknown = 0,
  kError = 1,
  kOutOfOrderApiCall = 2,
  kAlreadyHaveActiveStrategy = 3,
  kAlreadyAdvertising = 4,
  kBluetoothError = 5,
  kBleError = 6,
  kWifiLanError = 7,
  kMaxValue = kWifiLanError
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class FinalStatus {
  kSuccess = 0,
  kFailure = 1,
  kCanceled = 2,
  kMaxValue = kCanceled
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class AttachmentType {
  kUnknownFileType = 0,
  kUnknownTextType = 1,
  kImage = 2,
  kVideo = 3,
  kApp = 4,
  kAudio = 5,
  kText = 6,
  kUrl = 7,
  kAddress = 8,
  kPhoneNumber = 9,
  kWifiCredentials = 10,
  kMaxValue = kWifiCredentials
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class UpgradedMedium {
  kUnknown = 0,
  kMdns = 1,
  kBluetooth = 2,
  kWifiHotspot = 3,
  kBle = 4,
  kWifiLan = 5,
  kWifiAware = 6,
  kNfc = 7,
  kWifiDirect = 8,
  kWebRtc = 9,
  kNoUpgrade = 10,
  kBleL2Cap = 11,
  kUsb = 12,
  kMaxValue = kUsb
};

AttachmentType FileMetadataTypeToAttachmentType(
    sharing::mojom::FileMetadata::Type type) {
  switch (type) {
    case sharing::mojom::FileMetadata::Type::kUnknown:
      return AttachmentType::kUnknownFileType;
    case sharing::mojom::FileMetadata::Type::kImage:
      return AttachmentType::kImage;
    case sharing::mojom::FileMetadata::Type::kVideo:
      return AttachmentType::kVideo;
    case sharing::mojom::FileMetadata::Type::kApp:
      return AttachmentType::kApp;
    case sharing::mojom::FileMetadata::Type::kAudio:
      return AttachmentType::kAudio;
  }
}

AttachmentType TextMetadataTypeToAttachmentType(
    sharing::mojom::TextMetadata::Type type) {
  switch (type) {
    case sharing::mojom::TextMetadata::Type::kUnknown:
      return AttachmentType::kUnknownTextType;
    case sharing::mojom::TextMetadata::Type::kText:
      return AttachmentType::kText;
    case sharing::mojom::TextMetadata::Type::kUrl:
      return AttachmentType::kUrl;
    case sharing::mojom::TextMetadata::Type::kAddress:
      return AttachmentType::kAddress;
    case sharing::mojom::TextMetadata::Type::kPhoneNumber:
      return AttachmentType::kPhoneNumber;
  }
}

TransferFinalStatus TransferMetadataStatusToTransferFinalStatus(
    TransferMetadata::Status status) {
  switch (status) {
    case TransferMetadata::Status::kComplete:
      return TransferFinalStatus::kComplete;
    case TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed:
      return TransferFinalStatus::kAwaitingRemoteAcceptanceFailed;
    case TransferMetadata::Status::kFailed:
      return TransferFinalStatus::kFailed;
    case TransferMetadata::Status::kRejected:
      return TransferFinalStatus::kRejected;
    case TransferMetadata::Status::kCancelled:
      return TransferFinalStatus::kCancelled;
    case TransferMetadata::Status::kTimedOut:
      return TransferFinalStatus::kTimedOut;
    case TransferMetadata::Status::kMediaUnavailable:
      return TransferFinalStatus::kMediaUnavailable;
    case TransferMetadata::Status::kNotEnoughSpace:
      return TransferFinalStatus::kNotEnoughSpace;
    case TransferMetadata::Status::kUnsupportedAttachmentType:
      return TransferFinalStatus::kUnsupportedAttachmentType;
    case TransferMetadata::Status::kDecodeAdvertisementFailed:
      return TransferFinalStatus::kDecodeAdvertisementFailed;
    case TransferMetadata::Status::kMissingTransferUpdateCallback:
      return TransferFinalStatus::kMissingTransferUpdateCallback;
    case TransferMetadata::Status::kMissingShareTarget:
      return TransferFinalStatus::kMissingShareTarget;
    case TransferMetadata::Status::kMissingEndpointId:
      return TransferFinalStatus::kMissingEndpointId;
    case TransferMetadata::Status::kMissingPayloads:
      return TransferFinalStatus::kMissingPayloads;
    case TransferMetadata::Status::kPairedKeyVerificationFailed:
      return TransferFinalStatus::kPairedKeyVerificationFailed;
    case TransferMetadata::Status::kInvalidIntroductionFrame:
      return TransferFinalStatus::kInvalidIntroductionFrame;
    case TransferMetadata::Status::kIncompletePayloads:
      return TransferFinalStatus::kIncompletePayloads;
    case TransferMetadata::Status::kFailedToCreateShareTarget:
      return TransferFinalStatus::kFailedToCreateShareTarget;
    case TransferMetadata::Status::kFailedToInitiateOutgoingConnection:
      return TransferFinalStatus::kFailedToInitiateOutgoingConnection;
    case TransferMetadata::Status::kFailedToReadOutgoingConnectionResponse:
      return TransferFinalStatus::kFailedToReadOutgoingConnectionResponse;
    case TransferMetadata::Status::kUnexpectedDisconnection:
      return TransferFinalStatus::kUnexpectedDisconnection;
    case TransferMetadata::Status::kUnknown:
    case TransferMetadata::Status::kConnecting:
    case TransferMetadata::Status::kAwaitingLocalConfirmation:
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
    case TransferMetadata::Status::kInProgress:
    case TransferMetadata::Status::kMediaDownloading:
    case TransferMetadata::Status::kExternalProviderLaunched:
      NOTREACHED();
      return TransferFinalStatus::kUnknown;
  }
}

StartAdvertisingFailureReason
NearbyConnectionsStatusToStartAdvertisingFailureReason(
    nearby::connections::mojom::Status status) {
  switch (status) {
    case nearby::connections::mojom::Status::kError:
      return StartAdvertisingFailureReason::kError;
    case nearby::connections::mojom::Status::kOutOfOrderApiCall:
      return StartAdvertisingFailureReason::kOutOfOrderApiCall;
    case nearby::connections::mojom::Status::kAlreadyHaveActiveStrategy:
      return StartAdvertisingFailureReason::kAlreadyHaveActiveStrategy;
    case nearby::connections::mojom::Status::kAlreadyAdvertising:
      return StartAdvertisingFailureReason::kAlreadyAdvertising;
    case nearby::connections::mojom::Status::kBluetoothError:
      return StartAdvertisingFailureReason::kBluetoothError;
    case nearby::connections::mojom::Status::kBleError:
      return StartAdvertisingFailureReason::kBleError;
    case nearby::connections::mojom::Status::kWifiLanError:
      return StartAdvertisingFailureReason::kWifiLanError;
    case nearby::connections::mojom::Status::kSuccess:
      NOTREACHED();
      [[fallthrough]];
    case nearby::connections::mojom::Status::kAlreadyDiscovering:
    case nearby::connections::mojom::Status::kEndpointIOError:
    case nearby::connections::mojom::Status::kEndpointUnknown:
    case nearby::connections::mojom::Status::kConnectionRejected:
    case nearby::connections::mojom::Status::kAlreadyConnectedToEndpoint:
    case nearby::connections::mojom::Status::kNotConnectedToEndpoint:
    case nearby::connections::mojom::Status::kPayloadUnknown:
      return StartAdvertisingFailureReason::kUnknown;
  }
}

FinalStatus PayloadStatusToFinalStatus(
    nearby::connections::mojom::PayloadStatus status) {
  switch (status) {
    case nearby::connections::mojom::PayloadStatus::kSuccess:
      return FinalStatus::kSuccess;
    case nearby::connections::mojom::PayloadStatus::kFailure:
      return FinalStatus::kFailure;
    case nearby::connections::mojom::PayloadStatus::kCanceled:
      return FinalStatus::kCanceled;
    case nearby::connections::mojom::PayloadStatus::kInProgress:
      NOTREACHED();
      return FinalStatus::kFailure;
  }
}

std::string GetDirectionSubcategoryName(bool is_incoming) {
  return is_incoming ? ".Receive" : ".Send";
}

std::string GetIsKnownSubcategoryName(bool is_known) {
  return is_known ? ".Contact" : ".NonContact";
}

std::string GetShareTargetTypeSubcategoryName(
    nearby_share::mojom::ShareTargetType type) {
  switch (type) {
    case nearby_share::mojom::ShareTargetType::kUnknown:
      return ".UnknownDeviceType";
    case nearby_share::mojom::ShareTargetType::kPhone:
      return ".Phone";
    case nearby_share::mojom::ShareTargetType::kTablet:
      return ".Tablet";
    case nearby_share::mojom::ShareTargetType::kLaptop:
      return ".Laptop";
  }
}

std::string GetPayloadStatusSubcategoryName(
    nearby::connections::mojom::PayloadStatus status) {
  switch (status) {
    case nearby::connections::mojom::PayloadStatus::kSuccess:
      return ".Succeeded";
    case nearby::connections::mojom::PayloadStatus::kFailure:
      return ".Failed";
    case nearby::connections::mojom::PayloadStatus::kCanceled:
      return ".Cancelled";
    case nearby::connections::mojom::PayloadStatus::kInProgress:
      NOTREACHED();
      return ".Failed";
  }
}

std::string GetUpgradedMediumSubcategoryName(
    absl::optional<nearby::connections::mojom::Medium> last_upgraded_medium) {
  if (!last_upgraded_medium) {
    return ".NoMediumUpgrade";
  }

  switch (*last_upgraded_medium) {
    case nearby::connections::mojom::Medium::kWebRtc:
      return ".WebRtcUpgrade";
    case nearby::connections::mojom::Medium::kWifiLan:
      return ".WifiLanUpgrade";
    case nearby::connections::mojom::Medium::kUnknown:
    case nearby::connections::mojom::Medium::kMdns:
    case nearby::connections::mojom::Medium::kBluetooth:
    case nearby::connections::mojom::Medium::kWifiHotspot:
    case nearby::connections::mojom::Medium::kBle:
    case nearby::connections::mojom::Medium::kWifiAware:
    case nearby::connections::mojom::Medium::kNfc:
    case nearby::connections::mojom::Medium::kWifiDirect:
    case nearby::connections::mojom::Medium::kBleL2Cap:
    case nearby::connections::mojom::Medium::kUsb:
      return ".UnknownMediumUpgrade";
  }
}

UpgradedMedium GetUpgradedMediumForMetrics(
    absl::optional<nearby::connections::mojom::Medium> last_upgraded_medium) {
  if (!last_upgraded_medium) {
    return UpgradedMedium::kNoUpgrade;
  }

  switch (*last_upgraded_medium) {
    case nearby::connections::mojom::Medium::kUnknown:
      return UpgradedMedium::kUnknown;
    case nearby::connections::mojom::Medium::kMdns:
      return UpgradedMedium::kMdns;
    case nearby::connections::mojom::Medium::kBluetooth:
      return UpgradedMedium::kBluetooth;
    case nearby::connections::mojom::Medium::kWifiHotspot:
      return UpgradedMedium::kWifiHotspot;
    case nearby::connections::mojom::Medium::kBle:
      return UpgradedMedium::kBle;
    case nearby::connections::mojom::Medium::kWifiLan:
      return UpgradedMedium::kWifiLan;
    case nearby::connections::mojom::Medium::kWifiAware:
      return UpgradedMedium::kWifiAware;
    case nearby::connections::mojom::Medium::kNfc:
      return UpgradedMedium::kNfc;
    case nearby::connections::mojom::Medium::kWifiDirect:
      return UpgradedMedium::kWifiDirect;
    case nearby::connections::mojom::Medium::kWebRtc:
      return UpgradedMedium::kWebRtc;
    case nearby::connections::mojom::Medium::kBleL2Cap:
      return UpgradedMedium::kBleL2Cap;
    case nearby::connections::mojom::Medium::kUsb:
      return UpgradedMedium::kUsb;
  }
}

void RecordNearbySharePayloadAttachmentTypeMetric(
    AttachmentType type,
    bool is_incoming,
    nearby::connections::mojom::PayloadStatus status) {
  const std::string prefix = "Nearby.Share.Payload.AttachmentType";
  base::UmaHistogramEnumeration(prefix, type);
  base::UmaHistogramEnumeration(
      prefix + GetDirectionSubcategoryName(is_incoming), type);
  base::UmaHistogramEnumeration(
      prefix + GetPayloadStatusSubcategoryName(status), type);
}

// FuseBox (go/fuse-box) makes virtual file systems (e.g. ARC ContentProvider)
// visible on the Linux native file system through a FUSE (Filesystem in
// USErspace) abstraction layer.
bool IsFuseBoxFilePath(const base::FilePath& file_path) {
  if (file_path.empty()) {
    return false;
  }
  return base::StartsWith(file_path.value(),
                          file_manager::util::kFuseBoxMediaPath);
}

// Share Cache is used to store temporary files being shared between app
// platforms (used by ARC and WebAPK) and is owned by file manager.
bool IsShareCacheFilePath(Profile* profile, const base::FilePath& file_path) {
  if (!profile || file_path.empty()) {
    return false;
  }
  return base::StartsWith(
      file_path.value(),
      file_manager::util::GetShareCacheFilePath(profile).value());
}

}  // namespace

void RecordNearbyShareEnabledMetric(NearbyShareEnabledState state) {
  base::UmaHistogramEnumeration("Nearby.Share.Enabled", state);
}

void RecordNearbyShareEstablishConnectionMetrics(
    bool success,
    bool cancelled,
    base::TimeDelta time_to_connect) {
  FinalStatus status;
  if (success) {
    status = FinalStatus::kSuccess;
    base::UmaHistogramTimes(
        "Nearby.Share.Connection.TimeToEstablishOutgoingConnection",
        time_to_connect);
  } else {
    status = cancelled ? FinalStatus::kCanceled : FinalStatus::kFailure;
  }
  base::UmaHistogramEnumeration(
      "Nearby.Share.Connection.EstablishOutgoingConnectionStatus", status);

  // Log a high-level success/failure metric, ignoring cancellations.
  if (!cancelled) {
    base::UmaHistogramBoolean(
        "Nearby.Share.Connection.EstablishOutgoingConnection.Success", success);
  }
}

void RecordNearbyShareTimeFromInitiateSendToRemoteDeviceNotificationMetric(
    base::TimeDelta time) {
  base::UmaHistogramTimes(
      "Nearby.Share.TimeFromInitiateSendToRemoteDeviceNotification", time);
}

void RecordNearbyShareTimeFromLocalAcceptToTransferStartMetric(
    base::TimeDelta time) {
  base::UmaHistogramTimes("Nearby.Share.TimeFromLocalAcceptToTransferStart",
                          time);
}

void RecordNearbySharePayloadFileAttachmentTypeMetric(
    sharing::mojom::FileMetadata::Type type,
    bool is_incoming,
    nearby::connections::mojom::PayloadStatus status) {
  RecordNearbySharePayloadAttachmentTypeMetric(
      FileMetadataTypeToAttachmentType(type), is_incoming, status);
}

void RecordNearbySharePayloadTextAttachmentTypeMetric(
    sharing::mojom::TextMetadata::Type type,
    bool is_incoming,
    nearby::connections::mojom::PayloadStatus status) {
  RecordNearbySharePayloadAttachmentTypeMetric(
      TextMetadataTypeToAttachmentType(type), is_incoming, status);
}

void RecordNearbySharePayloadWifiCredentialsAttachmentTypeMetric(
    bool is_incoming,
    nearby::connections::mojom::PayloadStatus status) {
  RecordNearbySharePayloadAttachmentTypeMetric(AttachmentType::kWifiCredentials,
                                               is_incoming, status);
}

void RecordNearbySharePayloadFileOperationMetrics(
    Profile* profile,
    const ShareTarget& share_target,
    PayloadFileOperation operation,
    const bool success) {
  DCHECK(profile);

  if (!share_target.has_attachments()) {
    return;
  }

  const absl::optional<base::FilePath>& path =
      share_target.file_attachments[0].file_path();
  if (path) {
    // To determine the file path type, only first attachment file is checked as
    // it is expected that all file attachments from the volume location will be
    // using the same path (e.g. MTP, Downloads, Fusebox, ShareCache, etc.).
    // Only FuseBox and ShareCache are highlighted here as they are paths for
    // virtual file systems, but other specific path metrics can be added.
    const std::string metric_str = (operation == PayloadFileOperation::kOpen)
                                       ? "Nearby.Share.Payload.Open.Success"
                                       : "Nearby.Share.Payload.Read.Success";
    if (IsFuseBoxFilePath(*path)) {
      base::UmaHistogramBoolean(metric_str + ".FuseBox", success);
    } else if (IsShareCacheFilePath(profile, *path)) {
      base::UmaHistogramBoolean(metric_str + ".ShareCache", success);
    } else {
      base::UmaHistogramBoolean(metric_str + ".UnknownPath", success);
    }
    // Overall success/failure independent of the file path.
    base::UmaHistogramBoolean(metric_str, success);
  }
}

void RecordNearbySharePayloadFinalStatusMetric(
    nearby::connections::mojom::PayloadStatus status,
    absl::optional<nearby::connections::mojom::Medium> medium) {
  DCHECK_NE(status, nearby::connections::mojom::PayloadStatus::kInProgress);
  base::UmaHistogramEnumeration("Nearby.Share.Payload.FinalStatus",
                                PayloadStatusToFinalStatus(status));
  base::UmaHistogramEnumeration("Nearby.Share.Payload.FinalStatus" +
                                    GetUpgradedMediumSubcategoryName(medium),
                                PayloadStatusToFinalStatus(status));
}

void RecordNearbySharePayloadMediumMetric(
    absl::optional<nearby::connections::mojom::Medium> medium,
    nearby_share::mojom::ShareTargetType type,
    uint64_t num_bytes_transferred) {
  base::UmaHistogramEnumeration("Nearby.Share.Payload.Medium",
                                GetUpgradedMediumForMetrics(medium));
  if (num_bytes_transferred >= k5MbInBytes) {
    base::UmaHistogramEnumeration(
        "Nearby.Share.Payload.Medium.Over5MbTransferred",
        GetUpgradedMediumForMetrics(medium));
    base::UmaHistogramEnumeration(
        "Nearby.Share.Payload.Medium.Over5MbTransferred" +
            GetShareTargetTypeSubcategoryName(type),
        GetUpgradedMediumForMetrics(medium));
  }
}

void RecordNearbySharePayloadNumAttachmentsMetric(
    size_t num_text_attachments,
    size_t num_file_attachments,
    size_t num_wifi_credentials_attachments) {
  base::UmaHistogramCounts100("Nearby.Share.Payload.NumAttachments",
                              num_text_attachments + num_file_attachments +
                                  num_wifi_credentials_attachments);
  base::UmaHistogramCounts100("Nearby.Share.Payload.NumAttachments.Text",
                              num_text_attachments);
  base::UmaHistogramCounts100("Nearby.Share.Payload.NumAttachments.File",
                              num_file_attachments);
  base::UmaHistogramCounts100(
      "Nearby.Share.Payload.NumAttachments.WiFiCredentials",
      num_wifi_credentials_attachments);
}

void RecordNearbySharePayloadSizeMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    absl::optional<nearby::connections::mojom::Medium> last_upgraded_medium,
    nearby::connections::mojom::PayloadStatus status,
    uint64_t payload_size_bytes) {
  DCHECK_NE(status, nearby::connections::mojom::PayloadStatus::kInProgress);

  int kilobytes =
      base::saturated_cast<int>(payload_size_bytes / kBytesPerKilobyte);

  const std::string prefix = "Nearby.Share.Payload.TotalSize";
  base::UmaHistogramCounts1M(prefix, kilobytes);
  base::UmaHistogramCounts1M(prefix + GetDirectionSubcategoryName(is_incoming),
                             kilobytes);
  base::UmaHistogramCounts1M(prefix + GetShareTargetTypeSubcategoryName(type),
                             kilobytes);
  base::UmaHistogramCounts1M(
      prefix + GetUpgradedMediumSubcategoryName(last_upgraded_medium),
      kilobytes);
  base::UmaHistogramCounts1M(prefix + GetPayloadStatusSubcategoryName(status),
                             kilobytes);
}

void RecordNearbySharePayloadTransferRateMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    absl::optional<nearby::connections::mojom::Medium> last_upgraded_medium,
    nearby::connections::mojom::PayloadStatus status,
    uint64_t transferred_payload_bytes,
    base::TimeDelta time_elapsed) {
  DCHECK_NE(status, nearby::connections::mojom::PayloadStatus::kInProgress);

  int kilobytes_per_second = base::saturated_cast<int>(base::ClampDiv(
      base::ClampDiv(transferred_payload_bytes, time_elapsed.InSecondsF()),
      kBytesPerKilobyte));

  const std::string prefix = "Nearby.Share.Payload.TransferRate";
  base::UmaHistogramCounts100000(prefix, kilobytes_per_second);
  base::UmaHistogramCounts100000(
      prefix + GetDirectionSubcategoryName(is_incoming), kilobytes_per_second);
  base::UmaHistogramCounts100000(
      prefix + GetShareTargetTypeSubcategoryName(type), kilobytes_per_second);
  base::UmaHistogramCounts100000(
      prefix + GetUpgradedMediumSubcategoryName(last_upgraded_medium),
      kilobytes_per_second);
  base::UmaHistogramCounts100000(
      prefix + GetPayloadStatusSubcategoryName(status), kilobytes_per_second);
}

void RecordNearbyShareStartAdvertisingResultMetric(
    bool is_high_visibility,
    nearby::connections::mojom::Status status) {
  const std::string mode_suffix =
      is_high_visibility ? ".HighVisibility" : ".BLE";
  const bool success = status == nearby::connections::mojom::Status::kSuccess;

  const std::string result_prefix = "Nearby.Share.StartAdvertising.Result";
  base::UmaHistogramBoolean(result_prefix, success);
  base::UmaHistogramBoolean(result_prefix + mode_suffix, success);

  if (!success) {
    const std::string failure_prefix =
        "Nearby.Share.StartAdvertising.Result.FailureReason";
    StartAdvertisingFailureReason reason =
        NearbyConnectionsStatusToStartAdvertisingFailureReason(status);
    base::UmaHistogramEnumeration(failure_prefix, reason);
    base::UmaHistogramEnumeration(failure_prefix + mode_suffix, reason);
  }
}

void RecordNearbyShareTransferFinalStatusMetric(
    NearbyShareFeatureUsageMetrics* feature_usage_metrics,
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    TransferMetadata::Status status,
    bool is_known) {
  DCHECK(TransferMetadata::IsFinalStatus(status));

  // Emit success/failure to Standard Feature Usage Logging if there was a
  // definitive result.
  switch (TransferMetadata::ToResult(status)) {
    case TransferMetadata::Result::kSuccess:
      feature_usage_metrics->RecordUsage(/*success=*/true);
      break;
    case TransferMetadata::Result::kFailure:
      feature_usage_metrics->RecordUsage(/*success=*/false);
      break;
    case TransferMetadata::Result::kIndeterminate:
      break;
  }

  base::UmaHistogramBoolean("Nearby.Share.IsKnownContact", is_known);

  std::string send_or_receive = GetDirectionSubcategoryName(is_incoming);
  std::string share_target_type = GetShareTargetTypeSubcategoryName(type);
  std::string contact_or_not = GetIsKnownSubcategoryName(is_known);

  base::UmaHistogramEnumeration("Nearby.Share.DeviceType", type);
  base::UmaHistogramEnumeration("Nearby.Share.DeviceType" + send_or_receive,
                                type);

  // Log the detailed transfer final status enum.
  {
    TransferFinalStatus final_status =
        TransferMetadataStatusToTransferFinalStatus(status);
    const std::string prefix = "Nearby.Share.Transfer.FinalStatus";
    base::UmaHistogramEnumeration(prefix, final_status);
    base::UmaHistogramEnumeration(prefix + send_or_receive, final_status);
    base::UmaHistogramEnumeration(prefix + share_target_type, final_status);
    base::UmaHistogramEnumeration(prefix + contact_or_not, final_status);
  }

  // Log the transfer success/failure for high-level success and Critical User
  // Journey (CUJ) metrics.
  {
    absl::optional<bool> success;
    switch (TransferMetadata::ToResult(status)) {
      case TransferMetadata::Result::kSuccess:
        success = true;
        break;
      case TransferMetadata::Result::kFailure:
        success = false;
        break;
      case TransferMetadata::Result::kIndeterminate:
        success.reset();
        break;
    }
    if (success.has_value()) {
      const std::string prefix = "Nearby.Share.Transfer.Success";
      base::UmaHistogramBoolean(prefix, *success);
      base::UmaHistogramBoolean(
          prefix + send_or_receive + share_target_type + contact_or_not,
          *success);
    }
  }
}

void RecordNearbyShareDeviceNearbySharingNotificationFlowEvent(
    NearbyShareBackgroundScanningDeviceNearbySharingNotificationFlowEvent
        event) {
  base::UmaHistogramSparse(
      "Nearby.Share.BackgroundScanning.DeviceNearbySharing.Notification.Flow",
      static_cast<int>(event));
}

void RecordNearbyShareDeviceNearbySharingNotificationTimeToAction(
    base::TimeDelta time) {
  base::UmaHistogramMediumTimes(
      "Nearby.Share.BackgroundScanning.DeviceNearbySharing.Notification."
      "TimeToAction",
      time);
}

void RecordNearbyShareBackgroundScanningDevicesDetected() {
  base::UmaHistogramEnumeration(
      "Nearby.Share.BackgroundScanning.DevicesDetected",
      BackgroundScanningDevicesDetectedEvent::kNearbyDevicesDetected);
}

void RecordNearbyShareBackgroundScanningDevicesDetectedDuration(
    base::TimeDelta duration) {
  base::UmaHistogramLongTimes(
      "Nearby.Share.BackgroundScanning.DevicesDetected.Duration", duration);
}

void RecordNearbyShareBackgroundScanningSessionStarted(bool success) {
  base::UmaHistogramBoolean("Nearby.Share.BackgroundScanning.SessionStarted",
                            success);
}

void RecordNearbyShareSetupNotificationFlowEvent(
    NearbyShareBackgroundScanningSetupNotificationFlowEvent event) {
  base::UmaHistogramSparse(
      "Nearby.Share.BackgroundScanning.Setup.Notification.Flow",
      static_cast<int>(event));
}

void RecordNearbyShareSetupNotificationTimeToAction(base::TimeDelta time) {
  base::UmaHistogramMediumTimes(
      "Nearby.Share.BackgroundScanning.Setup.Notification.TimeToAction", time);
}

void RecordNearbyShareWifiConfigurationResultMetric(bool success) {
  base::UmaHistogramBoolean("Nearby.Share.WifiNetworkConfiguration.Result",
                            success);
}
