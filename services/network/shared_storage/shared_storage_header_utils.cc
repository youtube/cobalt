// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_storage/shared_storage_header_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

constexpr auto kSharedStorageOperationTypeMap =
    base::MakeFixedFlatMap<base::StringPiece,
                           mojom::SharedStorageOperationType>(
        {{"set", network::mojom::SharedStorageOperationType::kSet},
         {"append", network::mojom::SharedStorageOperationType::kAppend},
         {"delete", network::mojom::SharedStorageOperationType::kDelete},
         {"clear", network::mojom::SharedStorageOperationType::kClear}});

constexpr auto kSharedStorageHeaderParamTypeMap =
    base::MakeFixedFlatMap<base::StringPiece, SharedStorageHeaderParamType>(
        {{"key", SharedStorageHeaderParamType::kKey},
         {"value", SharedStorageHeaderParamType::kValue},
         {"ignore_if_present",
          SharedStorageHeaderParamType::kIgnoreIfPresent}});

}  // namespace

absl::optional<mojom::SharedStorageOperationType>
StringToSharedStorageOperationType(base::StringPiece operation_str) {
  auto* operation_it =
      kSharedStorageOperationTypeMap.find(base::ToLowerASCII(operation_str));
  if (operation_it == kSharedStorageOperationTypeMap.end()) {
    return absl::nullopt;
  }

  return operation_it->second;
}

absl::optional<SharedStorageHeaderParamType>
StringToSharedStorageHeaderParamType(base::StringPiece param_str) {
  auto* param_it =
      kSharedStorageHeaderParamTypeMap.find(base::ToLowerASCII(param_str));
  if (param_it == kSharedStorageHeaderParamTypeMap.end()) {
    return absl::nullopt;
  }

  return param_it->second;
}

bool GetSecSharedStorageWritableHeader(const net::HttpRequestHeaders& headers) {
  std::string value;
  if (!headers.GetHeader(kSecSharedStorageWritableHeader, &value)) {
    return false;
  }
  absl::optional<net::structured_headers::Item> item =
      net::structured_headers::ParseBareItem(value);
  if (!item || !item->is_boolean() || !item->GetBoolean()) {
    // We only expect the value "?1", which parses to boolean true.
    // TODO(cammie): Log a histogram to see if this ever happens.
    LOG(ERROR) << "Unexpected value '" << value << "' found for '"
               << kSecSharedStorageWritableHeader << "' header.";
    return false;
  }
  return true;
}

}  // namespace network
