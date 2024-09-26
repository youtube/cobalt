// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/companion_app/companion_app_parser.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
constexpr char kIntentKeyPrefix[] = "intent:";
constexpr char kIntentPrefix[] = "#Intent";
constexpr char kEndSuffix[] = "end";
constexpr char companionAppKey[] = "EXTRA_COMPANION_APP=";
}  // namespace

namespace ash {
namespace quick_pair {

CompanionAppParser::CompanionAppParser() = default;

CompanionAppParser::~CompanionAppParser() = default;

void CompanionAppParser::GetAppPackageName(
    scoped_refptr<Device> device,
    base::OnceCallback<void(absl::optional<std::string>)>
        on_companion_app_parsed) {
  const auto metadata_id = device->metadata_id();
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&CompanionAppParser::OnDeviceMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), std::move(device),
                     std::move(on_companion_app_parsed)));
}

void CompanionAppParser::OnDeviceMetadataRetrieved(
    scoped_refptr<Device> device,
    base::OnceCallback<void(absl::optional<std::string>)> callback,
    DeviceMetadata* device_metadata,
    bool retryable_err) {
  if (!device_metadata)
    return;

  const std::string intent_uri_from_metadata =
      device_metadata->GetDetails().intent_uri();
  if (intent_uri_from_metadata.find(kIntentKeyPrefix) != 0) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  absl::optional<std::string> result = GetCompanionAppExtra(
      intent_uri_from_metadata.substr(strlen(kIntentKeyPrefix)));
  std::move(callback).Run(result);
}

absl::optional<std::string> CompanionAppParser::GetCompanionAppExtra(
    const std::string& intent_as_string) {
  // Here is an an example of what Intents look like
  //
  // #Intent;action=android.intent.action.MAIN;
  //         category=android.intent.category.LAUNCHER;
  //         component=package_name/activity;
  //         param1;param2;end
  //
  // They must always begin with "#Intent", have components be separated by ";"
  // and end with "end"
  const std::vector<base::StringPiece> parts = base::SplitStringPiece(
      intent_as_string, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() < 2 || parts.front() != kIntentPrefix ||
      parts.back() != kEndSuffix) {
    QP_LOG(WARNING) << "Failed to split intent " << intent_as_string << ".";
    return absl::nullopt;
  }

  for (size_t i = 1; i < parts.size() - 1; ++i) {
    const size_t separator = parts[i].find('=');
    if (separator == std::string::npos) {
      if (parts[i].empty()) {
        // Intent should not have empty param. The empty param would appear in
        // intent string as ';;'. In the last case it would cause error in
        // Android framework. Such intents must not appear in the system.
        QP_LOG(WARNING) << "Found empty param in " << intent_as_string << ".";
        return absl::nullopt;
      }
      continue;
    }
  }

  // Devices with companion apps always have a key in their intent uri titled:
  // "EXTRA_COMPANION_APP", with the name of that app stored as the value
  size_t companionAppIndex = intent_as_string.find(companionAppKey);
  if (companionAppIndex == std::string::npos) {
    return absl::nullopt;
  }

  std::string companionAppId =
      intent_as_string.substr(companionAppIndex + strlen(companionAppKey));
  return companionAppId.substr(0, companionAppId.find(';'));
}

}  // namespace quick_pair
}  // namespace ash
