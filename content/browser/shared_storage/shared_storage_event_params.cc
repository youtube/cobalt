// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_event_params.h"

namespace content {

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::
    SharedStorageUrlSpecWithMetadata() = default;

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::
    SharedStorageUrlSpecWithMetadata(
        const GURL& url,
        std::map<std::string, std::string> reporting_metadata)
    : url(url.spec()), reporting_metadata(std::move(reporting_metadata)) {}

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::
    SharedStorageUrlSpecWithMetadata(const SharedStorageUrlSpecWithMetadata&) =
        default;

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::
    ~SharedStorageUrlSpecWithMetadata() = default;

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata&
SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::operator=(
    const SharedStorageUrlSpecWithMetadata&) = default;

bool SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::operator==(
    const SharedStorageUrlSpecWithMetadata& other) const {
  return url == other.url && reporting_metadata == other.reporting_metadata;
}

SharedStorageEventParams::SharedStorageEventParams(
    const SharedStorageEventParams&) = default;

SharedStorageEventParams::~SharedStorageEventParams() = default;

SharedStorageEventParams& SharedStorageEventParams::operator=(
    const SharedStorageEventParams&) = default;

SharedStorageEventParams::SharedStorageEventParams() = default;

SharedStorageEventParams::SharedStorageEventParams(
    absl::optional<std::string> script_source_url,
    absl::optional<std::string> operation_name,
    absl::optional<std::string> serialized_data,
    absl::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
        urls_with_metadata,
    absl::optional<std::string> key,
    absl::optional<std::string> value,
    absl::optional<bool> ignore_if_present)
    : script_source_url(std::move(script_source_url)),
      operation_name(std::move(operation_name)),
      serialized_data(std::move(serialized_data)),
      urls_with_metadata(std::move(urls_with_metadata)),
      key(std::move(key)),
      value(std::move(value)),
      ignore_if_present(ignore_if_present) {}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForAddModule(
    const GURL& script_source_url) {
  return SharedStorageEventParams(script_source_url.spec(), absl::nullopt,
                                  absl::nullopt, absl::nullopt, absl::nullopt,
                                  absl::nullopt, absl::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForRun(
    const std::string& operation_name,
    const std::vector<uint8_t>& serialized_data) {
  return SharedStorageEventParams(
      absl::nullopt, operation_name,
      std::string(serialized_data.begin(), serialized_data.end()),
      absl::nullopt, absl::nullopt, absl::nullopt, absl::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForSelectURL(
    const std::string& operation_name,
    const std::vector<uint8_t>& serialized_data,
    std::vector<SharedStorageUrlSpecWithMetadata> urls_with_metadata) {
  return SharedStorageEventParams(
      absl::nullopt, operation_name,
      std::string(serialized_data.begin(), serialized_data.end()),
      std::move(urls_with_metadata), absl::nullopt, absl::nullopt,
      absl::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForSet(
    const std::string& key,
    const std::string& value,
    bool ignore_if_present) {
  return SharedStorageEventParams(absl::nullopt, absl::nullopt, absl::nullopt,
                                  absl::nullopt, key, value, ignore_if_present);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForAppend(
    const std::string& key,
    const std::string& value) {
  return SharedStorageEventParams(absl::nullopt, absl::nullopt, absl::nullopt,
                                  absl::nullopt, key, value, absl::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForGetOrDelete(
    const std::string& key) {
  return SharedStorageEventParams(absl::nullopt, absl::nullopt, absl::nullopt,
                                  absl::nullopt, key, absl::nullopt,
                                  absl::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateDefault() {
  return SharedStorageEventParams();
}

}  // namespace content
