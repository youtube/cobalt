// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/diagnostics_service_converters.h"

#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/wilco_dtc_supportd/mojo_utils.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace converters {

namespace unchecked {

namespace {

std::string GetStringFromMojoHandle(mojo::ScopedHandle handle) {
  base::ReadOnlySharedMemoryMapping shared_memory;
  return std::string(MojoUtils::GetStringPieceFromMojoHandle(std::move(handle),
                                                             &shared_memory));
}

}  // namespace

crosapi::mojom::DiagnosticsRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdatePtr input) {
  return crosapi::mojom::DiagnosticsRoutineUpdate::New(
      input->progress_percent,
      GetStringFromMojoHandle(std::move(input->output)),
      ConvertDiagnosticsPtr(std::move(input->routine_update_union)));
}

crosapi::mojom::DiagnosticsRoutineUpdateUnionPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdateUnionPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::RoutineUpdateUnion::Tag::kInteractiveUpdate:
      return crosapi::mojom::DiagnosticsRoutineUpdateUnion::
          NewInteractiveUpdate(ConvertDiagnosticsPtr(
              std::move(input->get_interactive_update())));
    case cros_healthd::mojom::RoutineUpdateUnion::Tag::kNoninteractiveUpdate:
      return crosapi::mojom::DiagnosticsRoutineUpdateUnion::
          NewNoninteractiveUpdate(ConvertDiagnosticsPtr(
              std::move(input->get_noninteractive_update())));
  }
}

crosapi::mojom::DiagnosticsInteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::InteractiveRoutineUpdatePtr input) {
  return crosapi::mojom::DiagnosticsInteractiveRoutineUpdate::New(
      Convert(input->user_message));
}

crosapi::mojom::DiagnosticsNonInteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::NonInteractiveRoutineUpdatePtr input) {
  return crosapi::mojom::DiagnosticsNonInteractiveRoutineUpdate::New(
      Convert(input->status), std::move(input->status_message));
}

crosapi::mojom::DiagnosticsRunRoutineResponsePtr UncheckedConvertPtr(
    cros_healthd::mojom::RunRoutineResponsePtr input) {
  return crosapi::mojom::DiagnosticsRunRoutineResponse::New(
      input->id, Convert(input->status));
}

cros_healthd::mojom::NullableUint32Ptr UncheckedConvertPtr(
    crosapi::mojom::UInt32ValuePtr value) {
  return cros_healthd::mojom::NullableUint32::New(value->value);
}
}  // namespace unchecked

absl::optional<crosapi::mojom::DiagnosticsRoutineEnum> Convert(
    cros_healthd::mojom::DiagnosticRoutineEnum input) {
  switch (input) {
    case cros_healthd::mojom::DiagnosticRoutineEnum::kUnknown:
      return crosapi::mojom::DiagnosticsRoutineEnum::kUnknown;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity:
      return crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCapacity;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth:
      return crosapi::mojom::DiagnosticsRoutineEnum::kBatteryHealth;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck:
      return crosapi::mojom::DiagnosticsRoutineEnum::kSmartctlCheck;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower:
      return crosapi::mojom::DiagnosticsRoutineEnum::kAcPower;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache:
      return crosapi::mojom::DiagnosticsRoutineEnum::kCpuCache;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress:
      return crosapi::mojom::DiagnosticsRoutineEnum::kCpuStress;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy:
      return crosapi::mojom::DiagnosticsRoutineEnum::kFloatingPointAccuracy;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel:
      return crosapi::mojom::DiagnosticsRoutineEnum::kNvmeWearLevel;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest:
      return crosapi::mojom::DiagnosticsRoutineEnum::kNvmeSelfTest;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead:
      return crosapi::mojom::DiagnosticsRoutineEnum::kDiskRead;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch:
      return crosapi::mojom::DiagnosticsRoutineEnum::kPrimeSearch;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge:
      return crosapi::mojom::DiagnosticsRoutineEnum::kBatteryDischarge;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge:
      return crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCharge;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kMemory:
      return crosapi::mojom::DiagnosticsRoutineEnum::kMemory;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kLanConnectivity:
      return crosapi::mojom::DiagnosticsRoutineEnum::kLanConnectivity;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolution:
      return crosapi::mojom::DiagnosticsRoutineEnum::kDnsResolution;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolverPresent:
      return crosapi::mojom::DiagnosticsRoutineEnum::kDnsResolverPresent;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kSignalStrength:
      return crosapi::mojom::DiagnosticsRoutineEnum::kSignalStrength;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kGatewayCanBePinged:
      return crosapi::mojom::DiagnosticsRoutineEnum::kGatewayCanBePinged;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kSensitiveSensor:
      return crosapi::mojom::DiagnosticsRoutineEnum::kSensitiveSensor;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kFingerprintAlive:
      return crosapi::mojom::DiagnosticsRoutineEnum::kFingerprintAlive;
    case cros_healthd::mojom::DiagnosticRoutineEnum::
        kSmartctlCheckWithPercentageUsed:
      return crosapi::mojom::DiagnosticsRoutineEnum::
          kSmartctlCheckWithPercentageUsed;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kEmmcLifetime:
      return crosapi::mojom::DiagnosticsRoutineEnum::kEmmcLifetime;
    default:
      return absl::nullopt;
  }
}

std::vector<crosapi::mojom::DiagnosticsRoutineEnum> Convert(
    const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>& input) {
  std::vector<crosapi::mojom::DiagnosticsRoutineEnum> output;
  for (const auto element : input) {
    absl::optional<crosapi::mojom::DiagnosticsRoutineEnum> converted =
        Convert(element);
    if (converted.has_value()) {
      output.push_back(converted.value());
    }
  }
  return output;
}

crosapi::mojom::DiagnosticsRoutineUserMessageEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineUserMessageEnum input) {
  switch (input) {
    case cros_healthd::mojom::DiagnosticRoutineUserMessageEnum::kUnknown:
      return crosapi::mojom::DiagnosticsRoutineUserMessageEnum::kUnknown;
    case cros_healthd::mojom::DiagnosticRoutineUserMessageEnum::kUnplugACPower:
      return crosapi::mojom::DiagnosticsRoutineUserMessageEnum::kUnplugACPower;
    case cros_healthd::mojom::DiagnosticRoutineUserMessageEnum::kPlugInACPower:
      return crosapi::mojom::DiagnosticsRoutineUserMessageEnum::kPlugInACPower;
    case cros_healthd::mojom::DiagnosticRoutineUserMessageEnum::kCheckLedColor:
      NOTIMPLEMENTED();
      return crosapi::mojom::DiagnosticsRoutineUserMessageEnum::kUnknown;
  }
  NOTREACHED();
  return static_cast<crosapi::mojom::DiagnosticsRoutineUserMessageEnum>(
      static_cast<int>(
          crosapi::mojom::DiagnosticsRoutineUserMessageEnum::kMaxValue) +
      1);
}

crosapi::mojom::DiagnosticsRoutineStatusEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineStatusEnum input) {
  switch (input) {
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kUnknown:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kUnknown;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kRunning:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kRunning;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kWaiting:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kWaiting;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kPassed:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kPassed;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kFailed:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kFailed;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kError:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kError;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kCancelled:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kCancelled;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kFailedToStart:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kFailedToStart;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kRemoved:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kRemoved;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kCancelling:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kCancelling;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kUnsupported:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kUnsupported;
    case cros_healthd::mojom::DiagnosticRoutineStatusEnum::kNotRun:
      return crosapi::mojom::DiagnosticsRoutineStatusEnum::kNotRun;
  }
  NOTREACHED();
  return static_cast<crosapi::mojom::DiagnosticsRoutineStatusEnum>(
      static_cast<int>(
          crosapi::mojom::DiagnosticsRoutineStatusEnum::kMaxValue) +
      1);
}

cros_healthd::mojom::DiagnosticRoutineCommandEnum Convert(
    crosapi::mojom::DiagnosticsRoutineCommandEnum input) {
  switch (input) {
    case crosapi::mojom::DiagnosticsRoutineCommandEnum::kUnknown:
      return cros_healthd::mojom::DiagnosticRoutineCommandEnum::kUnknown;
    case crosapi::mojom::DiagnosticsRoutineCommandEnum::kContinue:
      return cros_healthd::mojom::DiagnosticRoutineCommandEnum::kContinue;
    case crosapi::mojom::DiagnosticsRoutineCommandEnum::kCancel:
      return cros_healthd::mojom::DiagnosticRoutineCommandEnum::kCancel;
    case crosapi::mojom::DiagnosticsRoutineCommandEnum::kGetStatus:
      return cros_healthd::mojom::DiagnosticRoutineCommandEnum::kGetStatus;
    case crosapi::mojom::DiagnosticsRoutineCommandEnum::kRemove:
      return cros_healthd::mojom::DiagnosticRoutineCommandEnum::kRemove;
  }
  NOTREACHED();
  return static_cast<cros_healthd::mojom::DiagnosticRoutineCommandEnum>(
      static_cast<int>(
          cros_healthd::mojom::DiagnosticRoutineCommandEnum::kMaxValue) +
      1);
}

cros_healthd::mojom::AcPowerStatusEnum Convert(
    crosapi::mojom::DiagnosticsAcPowerStatusEnum input) {
  switch (input) {
    case crosapi::mojom::DiagnosticsAcPowerStatusEnum::kUnknown:
      return cros_healthd::mojom::AcPowerStatusEnum::kUnknown;
    case crosapi::mojom::DiagnosticsAcPowerStatusEnum::kConnected:
      return cros_healthd::mojom::AcPowerStatusEnum::kConnected;
    case crosapi::mojom::DiagnosticsAcPowerStatusEnum::kDisconnected:
      return cros_healthd::mojom::AcPowerStatusEnum::kDisconnected;
  }
  NOTREACHED();
  return static_cast<cros_healthd::mojom::AcPowerStatusEnum>(
      static_cast<int>(cros_healthd::mojom::AcPowerStatusEnum::kMaxValue) + 1);
}

cros_healthd::mojom::NvmeSelfTestTypeEnum Convert(
    crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum input) {
  switch (input) {
    case crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum::kUnknown:
      return cros_healthd::mojom::NvmeSelfTestTypeEnum::kUnknown;
    case crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum::kShortSelfTest:
      return cros_healthd::mojom::NvmeSelfTestTypeEnum::kShortSelfTest;
    case crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum::kLongSelfTest:
      return cros_healthd::mojom::NvmeSelfTestTypeEnum::kLongSelfTest;
  }
  NOTREACHED();
  return static_cast<cros_healthd::mojom::NvmeSelfTestTypeEnum>(
      static_cast<int>(cros_healthd::mojom::NvmeSelfTestTypeEnum::kMaxValue) +
      1);
}

cros_healthd::mojom::DiskReadRoutineTypeEnum Convert(
    crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum input) {
  switch (input) {
    case crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead:
      return cros_healthd::mojom::DiskReadRoutineTypeEnum::kLinearRead;
    case crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum::kRandomRead:
      return cros_healthd::mojom::DiskReadRoutineTypeEnum::kRandomRead;
    case crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum::kUnknown:
      // Fall-through to not-supported case.
      break;
  }
  NOTREACHED();
  return static_cast<cros_healthd::mojom::DiskReadRoutineTypeEnum>(
      static_cast<int>(
          cros_healthd::mojom::DiskReadRoutineTypeEnum::kMaxValue) +
      1);
}

}  // namespace converters
}  // namespace ash
